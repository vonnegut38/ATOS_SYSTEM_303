# Plan działania — BIOS dla Atari Mega ST
**Cel:** własny firmware (BIOS) dla Atari Mega ST + akcelerator TF536 (68030, 64 MB ALT‑RAM, IDE) + karta ET4000/W32i na adapterze VOFA (MegaBus), zastępujący TOS/EmuTOS i startujący **od pierwszego piksela na VGA**, z tabelą konfiguracji w stylu AWARD/AMI.

Dokument oparty na realnym stanie repozytorium (analiza źródeł, build, Hatari), nie na założeniach. Wersja: 2026‑09‑11.

---

## 0. TL;DR dla decydenta

1. **Nie zaczynamy od zera.** W katalogu `files-pasted-by-the-user-projekt` jest działający BIOS 0.03: start 68000/68030, detekcja CPU/RAM/maszyny, ACSI, POST, menu F1–F5, loader `BOOT.SYS`/`KERNEL.SYS` oraz własne jądro wielozadaniowe. Build jest reprodukowalny (`gmake`), a ROM zajmuje realnie **~24 KB z 192 KB** — limit „256 KB” **nie jest wąskim gardłem**.
2. **Prawdziwe luki** to: (a) systemowy bug DMA ACSI psujący stertę na Mega ST, (b) brak sterownika **IDE**, (c) **brak trwałego zapisu konfiguracji**, (d) karta **ET4000 nie przełącza obrazu** (kod wchodzi w VGA, ale zostawia Shifter), (e) brak **maprom/MMU handoff** do TOS/EmuTOS/MiNT.
3. **Rekomendacja architektoniczna:** BIOS = firmware (POST + Setup + abstrakcja sprzętu + boot manager). Na nim budujemy **własny system wielozadaniowy** na źródłach ASF, z **nakładką graficzną wzorowaną na EmuTOS**, aż do uruchamiania **natywnych aplikacji TOS w oknach** na ET4000/W32i z akceleracją blitterem i przełączaniem zadań. MMU 68030 (TF536) daje izolację procesów i prywatne bufory ekranu — to fundament okienkowania.
4. **Kolejność:** najpierw naprawa DMA + wideo ET4000 + IDE + config + boot manager, dopiero potem OS. Emulator Hatari jest już rozszerzony o `--tf536` i `--graphics-card vofa`.

---

## 1. Cel i zakres

### 1.1 Wymagania funkcjonalne

| # | Wymaganie | Stan obecny | Do zrobienia |
|---|---|---|---|
| R1 | Start zamiast TOS/EmuTOS, obraz ≤ 256 KB | ✅ 192 KB @0xFC0000 i 256 KB @0xE00000 | utrzymać oba warianty |
| R2 | Obraz na **VGA ET4000 od razu**, bez widocznego Shiftera | ⚠️ kod wchodzi w VGA, ale przełącza też Shifter | rozdzielić, poprawić geometrię 640×480 |
| R3 | Tabela konfiguracji w stylu AWARD/AMI (tekst + semigrafika) | ⚠️ prosty POST | nowy ekran POST + CP437 box‑drawing |
| R4 | Detekcja **68030** i cache/FPU | ✅ | dopracować raport 030 |
| R5 | Detekcja **ST RAM** i **TT/ALT‑RAM** (TF536, 64 MB @0x01000000) | ⚠️ 24‑bit alias, brak modelu do 64 MB | poprawić `memory_detect_fast` |
| R6 | Detekcja **ET4000 / ET4000‑W32i** + blitter | ⚠️ rodzina ET4000, bez rozróżnienia W32i | odczyt ID + tabela możliwości |
| R7 | Detekcja dysków **ACSI** i **IDE (TF536)** | ⚠️ ACSI tak, IDE brak | nowy sterownik IDE |
| R8 | **Boot z karty** (ACSI/IDE), nie z TOS płyty | ⚠️ tylko ACSI + loader własnego OS | loader device‑agnostic + boot menu |
| R9 | Trwała **konfiguracja** (NVRAM lub dysk) | ❌ tylko RAM | `cfg_load/cfg_save` z backendami |
| R10 | Setup podstawowych opcji komputera | ⚠️ data/godzina/zegar/wideo/blitter | pełne menu AWARD‑style |
| R11 | Później: EmuTOS / MiNT / MAGIC | ❌ | maprom + bootinfo + handoff |
| R12 | Partycje **FAT16, FAT32, AHDI** | ⚠️ FAT16 + AHDI (tylko odczyt) | parser FAT32 + zapis configu |

### 1.2 Poza zakresem „na teraz”
- Własny GEMDOS/VDI pełnej zgodności z TOS (to warstwa OS, nie BIOS).
- Obsługa Falcona/TT (BIOS ma być przenośny, ale cel to Mega ST).
- Sieć, USB, dźwięk.

### 1.3 Definicje
- **ALT‑RAM / TT‑RAM** — pamięć szybka TF536 mapowana od `0x01000000`, do 64 MB.
- **VOFA** — adapter Volksfarben/„VOFA” na MegaBus: VRAM `0x00C00000`, rejestry VGA `0x00D00000`, I/O 8‑bit.
- **STGA** — alternatywny adapter (Till Harbaum): rejestry `0x00CF0000`, I/O 16‑bit.
- **maprom** — skopiowanie obrazu ROM/OS do ALT‑RAM i zmapowanie go w okno ROM przez MMU 68030.

---

## 2. Stan wyjściowy — co już mamy (fakty z kodu)

- **ROM/pamięć:** `linker/st-rom.ld` (192 KB @0xFC0000) i `linker/mste-rom.ld` (256 KB @0xE00000). Realny rozmiar obrazu: `text 24070 B + data 0 + bss 4292 B` → **~24 KB kodu**, resztę stanowi padding. Zapas jest ogromny.
- **Start:** `boot/reset.S` → nagłówek 48 B, `ram_detect_early`, walidacja progu co 512 KB → `firmware_main()` (`main.c`).
- **`firmware_main()`:** `memory_detect → cpu_detect_type/fpu → machine_detect → memory_detect_fast → expansions_detect → rtc_init → shifter_init/ikbd_init → post_run → autoboot_countdown → boot_stage2_run`, a potem pętla menu F1..F5/F10.
- **Detekcje:** `hw/machine.c` (ST/Mega ST/STE/Mega STE/TT/Falcon, blitter, zegar), `hw/memory.c` (ST RAM z bootstrapu, ALT‑RAM sonda @0x01000000 od 68020), `hw/nova.c` (VME/CrazyDots/VOFA/STGA, Mach64/Mach32/ET4000), `hw/acsi.c` (DMA ACSI, TEST UNIT, READ, tablica partycji AHDI `0x1234`), `hw/rtc.c` (RP5C15 Mega / NVRAM STE/TT).
- **Konsola:** `ui/console.c` obsługuje Shifter (low/med/mono) **oraz** ścieżkę `vga_console` do VRAM `0x00C00000` (1 bpp, 80 B/linia, 30 wierszy × 16 px = 640×480).
- **Setup:** `ui/setup.c` edytuje datę/godzinę, zegar (tylko Mega STE), preferencję wideo i blitter — **bez trwałego zapisu**.
- **Loader:** `loader/bootstage.c` skanuje ACSI 0–7, czyta `SYSTEM/BOOT.SYS` (FAT16), potem `KERNEL.SYS`; `loader/tos.c` + `ui/tos.c` to katalog/walidator obrazów TOS (bez uruchamiania).
- **Jądro (późniejszy etap):** moduły `ACSI/CORE/VFS/PROC/TIMER/EVENT/INPUT/SHELL`, GUI mono, ~72 aplikacje testowe. VFS tylko odczyt FAT16/AHDI.
- **Znany błąd krytyczny (`FINDINGS.md`):** DMA ACSI na Mega ST zapisuje **256 B poza bufor** i niszczy stertę („BLOCK LAYER: NO MEMORY”). Obejście = statyczny bufor; **docelowo naprawa systemowa** (margines DMA + kanarki).
- **Build:** `m68k-elf-gcc 16.2.0`, `gmake 4.4.1` (Makefile używa grupowanych celów `&:` — **tylko gmake**). Hatari zbudowany lokalnie z `--tf536` i `--graphics-card {none|vofa}` + `--gfx-chip`.

---

## 2a. Ustalenia diagnostyczne i ROZWIĄZANIE (2026‑09‑11)

**✅ BIOS startuje na ET4000 i dochodzi do shella.**

Profil: `--machine megast --cpulevel 3 --cpuclock 32 --memsize 4 --ttram 64 --tf536 on --graphics-card vofa --gfx-chip et4000 --mono`, ROM `bios_mste.bin` (256 KB), ACSI `work.img`. Render VRAM karty potwierdza pełny łańcuch:

```
ATARI SYSTEM KERNEL 0.03
MEMORY MANAGER: READY
GRAPHICS BACKEND: SHIFTER 640X400 / CLIP+DIRTY / BLITTER READY
ST-RAM POOL: 3715 KB FREE / LARGEST 3804128 B
TT-RAM POOL: 65535 KB FREE
BLOCK LAYER: READY
AHDI VOLUMES: 2   C: BGM 256 MB BOOT
FAT LAYER: FAT16 MOUNTED
KERNEL STATUS: RUNNING / PREEMPTIVE / SHELL READY
STARTUP SERVICES: SYSTEM.INIT READY / SYSTEM.GFX READY / SYSTEM.GUI READY
C:\> _
```

**Przyczyny (trzy błędy, wszystkie naprawione):**
1. **Hatari, mostek VOFA** (`video.c`): `memset(STRam + VideoBase, 0, 32000)` kasował wektory i `.bss` (0x400+), bo przed ustawieniem bazy Shiftera `VideoBase == 0`. To powodowało zniknięcie `g_system` (`MEMORY MANAGER: FAILED`) i `vga_console`. **Poprawka:** czyszczenie tylko dla `VideoBase >= 0x4000`.
2. **Hatari, render karty** (`conv_st.c`/`conv_gen.c`): karta była rysowana z `&STRam[0xC00000]` zamiast z `vofa_vram`. **Poprawka:** `ConvGen_DrawPointer()` z pointerem z `VOFA_GetMode()`.
3. **BIOS** (`hw/nova.c`): zapis do Shiftera używał `0xff8260` zamiast `0xffff8260` (`SHIFTER_RES`). **Poprawka:** `REG8(SHIFTER_RES) = 0x02u`.

**Etap 1 — DMA (zrobione):** systemowy `dma_stage` (8 sektorów + 2048 B kanarka) w `kernel_acsi_read_blocks`; wynik `DMA GUARD: 0 OVERRUN(S)`. Wcześniejsze „0/256" było artefaktem błędu Hatari (kasowanie `.bss`), nie realnym nadmiarem DMA.

**Narzędzie:** render VRAM karty do PNG (`/tmp/card_final.png`) — do zautomatyzowania w Etapie 7 (`hatari_shot.py` wymaga przejścia na `hatari-debug screenshot`).

**Czystość drzewa:** wszystkie tymczasowe instrumentacje BIOS-u cofnięte; w ROM zostają wyłącznie 3 realne poprawki (dwie w Hatari, jedna w BIOS-ie).

---

## 3. Konfiguracja docelowa i mapy adresowe

| Zasób | Adres / parametr | Uwagi |
|---|---|---|
| ST RAM | `0x000000` | Mega ST: 1/2/4 MB |
| ALT/TT RAM (TF536) | `0x01000000`, do 64 MB | wymaga 32‑bit addressing (68030) |
| **ROM docelowy (karta TF536)** | **`0xE00000`, 256 KB** | `bios_mste.bin`, programowany na karcie; brak TOS płyty |
| ROM awaryjny (stock ST) | `0xFC0000`, max 192 KB | tylko jako wariant testowy |
| VOFA — VRAM | `0x00C00000` (1 MB), I/O 8‑bit | nasza karta ET4000 |
| VOFA — rejestry VGA | `0x00D00000` + port | sekwencer/CRTC/GC/AC/DAC |
| STGA — VRAM / rejestry | `0x00C00000` / `0x00CF0000`, I/O 16‑bit | alternatywa |
| Nova VME | VRAM `0xFEC00000`, regs `0xFEDC0000` | Mega STE/TT |
| IDE (TF536) | **kandydat `0x00F00000`**; Falcon/EmuTOS `0xFFF00000` | potwierdzić; zrobić auto‑sondę i opcję |
| ACSI | `0xFFFF8604`/06/09/0B/0D | istnieje |
| NVRAM STE/TT | `0xFFFF8961`/`63` | brak na Mega ST |
| RTC Mega ST | RP5C15 `0xFFFFFC20` | pytanie o wolne rejestry RAM |

Źródło prawdy dla rejestrów ET4000: `work/emutos-reference/bios/nova.c` (EMUTOS 1.4) + NVDI/VMG (`T3/`, `T8/`).

---

## 4. Decyzje architektoniczne

**Przyjęte (ustalone z użytkownikiem):**

- **D1. Obraz 256 KB z karty TF.** BIOS jest programowany w 256 KB @0xE00000 (`bios_mste.bin`) i uruchamiany z nośnika na karcie TF536; TOS płyty nie jest używany. Wariant 192 KB @0xFC0000 utrzymujemy tylko jako awaryjny/testowy.
- **D2. Konsola tekstowa w trybie 8 bpp chunky** (1 bajt = 1 piksel, komórka 8×16, 80×30 = 640×480), z atrybutami koloru i znakami CP437 (ramki, paski semigraficzne). Spójna z istniejącym `console.c`; docelowo rozszerzona o akcelerację blitterem. Prawdziwy VGA text mode (font w plane 2) pozostaje opcją, nie warunkiem.
- **D3. Warstwa abstrakcji urządzeń blokowych** `blkdev_ops` z backendami ACSI i IDE — loader i później VFS nie znają transportu.
- **D4. Boot manager + maprom.** BIOS wykrywa urządzenia, ładuje wybrany obraz OS z karty/dysku i przekazuje sterowanie. MMU 68030 służy do remapu okna ROM (obrazy większe niż okno ROM) i — w systemie — do izolacji procesów.
- **D5. Konfiguracja trwała z wieloma backendami:** NVRAM STE/TT → wolne rejestry RP5C15 (jeśli są) → zarezerwowany sektor na boot‑dysku (podwójna kopia + CRC) → defaults. Zapis tylko po walidacji.
- **D6. Wideo od pierwszego piksela:** POST i Setup piszą wyłącznie do VRAM ET4000; Shifter tylko jako awaryjny fallback, gdy karty brak.

**Ustalone przez użytkownika (odpowiedzi z 2026‑09‑11):**
- **U1 (ROM):** **256 KB z karty TF** — docelowy jest `bios_mste.bin` (@0xE00000), nie stock 192 KB.
- **U2 (karta):** **ET4000 oraz ET4000/W32i**. **Programowane tak samo** (wspólna tabela rejestrów); BIOS ma **wykryć odmianę** i raportować ją; przy W32i może użyć blittera.
- **U3 (kolejność):** **BIOS → własny system wielozadaniowy (na źródłach ASF z katalogu) → nakładka graficzna wzorowana na EmuTOS (wygląd i sposób rysowania) → natywne aplikacje TOS w oknach na ET4000/W32i z akceleracją i przełączaniem zadań.**

**Rozstrzygnięte przez użytkownika (2026‑09‑11, runda 2):**
- **O3.** Adres bazowy IDE TF536: **auto‑sonda** (`0x00F00000` i `0xFFF00000`) z możliwością wymuszenia w configu.
- **O5.** Config trzymany **na partycji C: w katalogu `BIOS_CFG`** (plik), a nie w zarezerwowanym sektorze.
- **Nowe:** BIOS ma obsługiwać partycje **FAT16, FAT32 oraz standardowy format Atari (AHDI)**.
- **O6.** Realny sprzęt/programator EPROM — do przekazania przez użytkownika (na razie Hatari).

---

## 5. Architektura oprogramowania

### 5.1 Warstwy

```
boot/reset.S (nagłówek, RAM, stos)
   -> firmware_main()
        POST (ekran AWARD, test pamięci)
        Setup (menu, edycja)
        hw/  machine, memory, acsi, ide, nova, rtc, config_store
        loader/  blkdev (ACSI|IDE), fat16, maprom (MMU), boot manager
   -> handoff: wlasny kernel ASF  |  EmuTOS/TOS (maprom)  -> MiNT/MAGIC
```

### 5.2 Nowe / zmieniane pliki

| Plik | Rola |
|---|---|
| `hw/ide.c`, `include/ide.h` | sterownik IDE TF536 (reset, IDENTIFY, LBA28/48, odczyt/zapis) |
| `hw/video_et4000.c` | wydzielone z `nova.c`: tabela trybów, sekwencje rejestrów, paleta |
| `hw/video_text.c` (lub rozszerzenie `ui/console.c`) | konsola znakowa 8×16 + semigrafika CP437 + atrybuty |
| `hw/config_store.c`, `include/bios_config.h` | trwały zapis konfiguracji (NVRAM/RP5C15/dysk) |
| `loader/blkdev.c` | wspólny interfejs ACSI/IDE |
| `loader/maprom.c` | kopiowanie obrazu OS do ALT‑RAM + programowanie MMU 030 |
| `loader/bootinfo.c` | pakiet wykrytej konfiguracji dla ładowanego OS |
| `ui/post_award.c` | nowy ekran POST w stylu AWARD |
| `ui/setup_award.c` | rozbudowane menu Setup |
| `linker/*.ld` | sekcje na sterowniki; ewentualny wariant 512 KB dla maprom |
| `Makefile` | nowe moduły; wymuszenie `gmake` w dokumentacji i skryptach |

### 5.3 Struktura konfiguracji (propozycja)

```c
#define BIOS_CFG_MAGIC   0x41534643UL   /* 'ASFC' */
#define BIOS_CFG_VERSION 1
typedef struct {
    uint32_t magic; uint16_t version, size; uint32_t crc;
    uint8_t  boot_order[4];        /* ACSI, IDE, FLOPPY, ROM */
    uint8_t  boot_timeout_s, fast_boot;
    uint8_t  video_adapter;        /* AUTO/VOFA/STGA/VME/SHIFTER */
    uint8_t  video_mode;           /* VGA_TEXT / 640x480x8 / SHIFTER */
    uint8_t  video_columns, video_font;
    uint8_t  cpu_cache, cpu_mmu;
    uint8_t  alt_ram_auto;
    uint32_t alt_ram_base, alt_ram_mb;
    uint8_t  ide_enable, ide_base_sel, ide_byteswap;
    char     os_image[32];
    uint8_t  os_load, post_beep, hide_shifter;
    uint16_t reserved;
} bios_config_t;
```
Backend zapisu (ustalony): **plik `BIOS_CFG\BIOS.CFG` na partycji C:** (FAT16/FAT32/AHDI) z podwójną kopią i CRC; fallback: NVRAM STE/TT → wolne rejestry RP5C15 → defaults. Wymaga warstwy odczytu/zapisu dla trzech systemów plików.

### 5.4 Model startu i handoff
1. Reset → nagłówek ROM → detekcja RAM → `firmware_main`.
2. Init ET4000 (640×480×8) → logo ~1 s → POST AWARD (na VGA).
3. Detekcja: CPU/cache/FPU, ST RAM, ALT‑RAM, ET4000/W32i, ACSI, IDE, RTC, Shifter, blitter.
4. `cfg_load()`; odliczanie / klawisz DEL → Setup.
5. Boot manager: lista urządzeń → wybór → (a) własny `KERNEL.SYS`, albo (b) obraz OS z dysku → `maprom` do ALT‑RAM → MMU → skok.
6. Po handoff: EmuTOS/TOS → MiNT/MAGIC. Bootinfo dostępny przez standardowy mechanizm / cookie.

---

## 6. Plan etapowy

### Etap 0 — Baseline i decyzje (2–3 dni)
Zadania: potwierdzić pozostałe O3/O5/O6; `gmake -B -j4` zielony; uruchomić obecny ROM w Hatari (`run-bios-vga.sh`) i zapisać zrzut referencyjny; zamrozić `checksums.txt`.
**DoD:** powtarzalny build + działający start w Hatari + decyzje zapisane (U1–U3 ✅).

### Etap 1 — Fundament: DMA i stabilny boot na Mega ST ✅ (zrobione 2026‑09‑11)
Zadania: **systemowy bufor pośredniczący z kanarkiem** (`dma_stage`: 8 sektorów + 2048 B marginesu) w `kernel_acsi_read_blocks`; każde DMA (moduł ACSI lub fallback BIOS-u) trafia do ST‑RAM, a dane są kopiowane do wywołującego; diagnostyka tymczasowa usunięta. Uruchomiony pełny łańcuch na Mega ST + 030 + 64 MB ALT‑RAM.
**DoD:** ✅ brak `BLOCK LAYER: NO MEMORY`; łańcuch dochodzi do shella (`C:\>`); `DMA GUARD: 0 OVERRUN(S)`.
**Ważne ustalenie:** wcześniejsze „0/256" było artefaktem błędu Hatari (kasowanie `.bss`), a nie realnym nadmiarem DMA — po naprawie Hatari kanarki są czyste. Staging zostaje jako defensywa i gwarancja DMA w 24‑bitowym ST‑RAM (także dla odbiorców w TT‑RAM).

### Etap 2 — Wideo ET4000: VGA od pierwszego piksela (1–2 tygodnie)
Zadania: wydzielić `video_et4000.c`; tabela trybów (sekwenser/CRTC/GC/AC/DAC + rozszerzone rejestry Tseng wg EmuTOS `nova.c`); tryb 640×480×8bpp; font 8×16 + CP437; atrybuty koloru; konsola znakowa 80×30; scroll (najpierw memmove, potem blitter W32i); usunąć wymuszanie Shiftera z `nova_detect`; potwierdzić geometrię 640×480 w Hatari (nie 400).
**DoD:** zimny start pokazuje logo i POST **wyłącznie** na VGA; brak migotania; zrzut z Hatari zgodny pikselowo.

### Etap 3 — Detekcja sprzętu + POST AWARD ✅ (zrobione 2026‑09‑11)
✅ Zrobione: tabelka AWARD 80×30 na ET4000 (ramki ASCII, wiersze: CPU/FPU/zegar/cache/RAM/TT‑RAM/karta/tryb/blitter/IDE/ASCI/RTC/config + pasek F‑keys i countdown). Countdown przepisany na **VBL** (niezależny od zegara). Fixy przy okazji: Mega ST nie jest już mylony ze STE; VOFA wykrywana przed STGA (STGA fałszywie łapała kartę VOFA); model IDE cache'owany.
**DoD:** ✅ POST z wykrytą konfiguracją widoczny na VGA przed bootem.
**Fix krytyczny (2026‑09‑11):** po komendzie (np. `help`) cały zapisany ekran przesuwał się w prawo. Przyczyna: GCC 16 na m68k zwinął pętlę przewijania do `moveb %a0@+,%a0@(-1280)` — ten sam rejestr adresowy jako źródło (post‑increment) i cel, czyli **hazard 68000** (adres celu liczony po inkrementacji → +1 bajt na każdy scroll). Zamienione na `vga_scroll_up()` w inline asm na dwóch rejestrach; przewijanie i zawijanie są poprawne.
Pozostaje w tym etapie: `hide_shifter` (obraz wyłącznie na karcie), rozróżnienie ET4000 vs W32i (ID + blitter), kody/beep błędów POST, szybszy test pamięci.

### Etap 4 — IDE TF536 ✅ (zrobione 2026‑09‑11)
Zadania: `hw/ide.c` (IDENTIFY DEVICE, READ SECTORS LBA28, auto‑sonda `0x00F00000`/`0xFFF00000`); jeden `acsi_read_sector` z flagą bit 7 dla IDE; skan ACSI→IDE w loaderze; AHDI/FAT16 z IDE; pominięcie modułu ACSI przy boocie z IDE; fix `module_cpu_type` (sync cache 68030 przy ładowaniu modułów).
**DoD:** ✅ boot **wyłącznie z IDE** (`--acsi 0=none --ide-master work.img`) do shella `C:\>`; na karcie `BOOT DEVICE: IDE / BIOS FALLBACK`; `DMA GUARD: 0`.
**Dalej:** MBR/FAT32, config na `C:\BIOS_CFG\`, kolejność bootowania w Setup.
**Bonus fix:** `hw/machine.c` — Mega ST był błędnie wykrywany jako STE (sonda `0xFFFF820D` istnieje też na Mega ST); teraz najpierw sprawdzany jest RTC RP5C15 `0xFFFFFC21`, a etykieta loadera pokazuje `BOOT DEVICE: IDE 0`.

### Etap 5 — Konfiguracja trwała + Setup (w toku, 2026‑09‑11)
✅ Zrobione: `bios_config_t` (magic/version/CRC), zapis/odczyt `\BIOS.CFG` na FAT16 (`ide_write_sectors` + `acsi_write_sector`), walidacja CRC z kodami błędów, **stosowanie** przy starcie (`boot_delay` → countdown, video, blitter), **zapis z Setup** (Enter → `cfg_save`), podgląd wartości na ekranie bootu. Demonstracja: 1. boot „DEFAULTS SAVED”, 2. boot z podmienionym configiem → „LOADED ... DELAY=1 VIDEO=2 BLITTER=0”, shell `C:\>`.
Do zrobienia: katalog `\BIOS_CFG\` (teraz plik w rootcie), zapis przez ACSI, fallback NVRAM/RP5C15, pełne menu Setup (boot order, IDE base).
**DoD:** ✅ konfiguracja przeżywa power‑cycle i jest stosowana; ✅ Setup zapisuje pola wideo/blitter.

### Etap 6 — Boot manager + maprom + handoff do OS (2 tygodnie)
Zadania: lista urządzeń/partycji; boot z FAT16 (ACSI/IDE); ładowanie obrazu OS do ALT‑RAM; programowanie MMU 030 (remap okna ROM); `bootinfo` dla EmuTOS; boot menu z timeoutem i „fast boot”; bezpieczne przejście bez TOS płyty.
**DoD:** BIOS startuje EmuTOS 192/256/512 KB z dysku na Mega ST (przez maprom) bez udziału TOS; następnie MiNT/MAGIC.

### Etap 7 — Hatari: domknięcie emulacji docelowej (równolegle)
Zadania: TF536 — ALT‑RAM 64 MB @0x01000000 dla Mega ST, IDE pod wybranym adresem, maprom/MMU; ET4000 — rejestry Tseng, CLUT, tryby 8bpp/1bpp, integracja okna 640×480 (rozszerzona ścieżka VDI); testy automatyczne (`hatari_shot.py`, `avi_frames.py`).
**DoD:** pełny cykl POST→boot→OS odtwarzalny w Hatari; zrzuty golden.

### Etap 8 — System wielozadaniowy z własnego jądra ASF (3–5 tygodni)
Zadania: rozwój istniejącego jądra (`system/`) w kierunku pełnego systemu: wywłaszczanie z priorytetami (Timer C + ewentualnie poziom 4 MFP), kolejka gotowych, pełny syscall ABI, VFS z zapisem, sterownik blokowy `blkdev_ops` (ACSI+IDE), obsługa RTC/klawiatury/myszy. Wykorzystać MMU 68030 do izolacji procesów (dziś ochrona wyłącznie programowa — na 030 można dać prywatne przestrzenie adresowe). Bazą są źródła ASF w `files-pasted-by-the-user-projekt/system/`.
**DoD:** kilka procesów działa wywłaszczeniowo z izolacją adresową; stabilny shell i usługi systemowe na profilu TF536.

### Etap 9 — Nakładka graficzna wzorowana na EmuTOS (3–4 tygodnie)
Zadania: przenieść konwencje rysowania i wygląd z EmuTOS (`work/emutos-reference/`): VDI-like API (workstation, raster ops, blit, czcionki, kursory, paleta), menedżer okien z Z-order, clippingiem, drag/resize i aktywnym oknem, desktop z ikonami/paskami, dialogi. Backend ET4000 8 bpp z akceleracją W32i (BitBlt, wypełnienia, kopia prostokątów). Fonty i semigrafika CP437, potem skalowalne.
**DoD:** GUI w stylu EmuTOS/GEM działa na ET4000/W32i; okna, menu, dialogi, przewijanie z użyciem blittera.

### Etap 10 — Natywne aplikacje TOS w oknach + akceleracja (4–6 tygodni)
Zadania: warstwa zgodności, która uruchamia niezmodyfikowane `.PRG`/`.APP`/`.TOS`:
- pułapki **GEMDOS (Trap #1)**, **BIOS (Trap #13)**, **XBIOS (Trap #14)**, **AES/VDI (Trap #2)**,
- emulacja basepage/PD i GEMDOS-owego modelu procesu,
- **wirtualizacja ekranu przez MMU 030**: każdy proces dostaje prywatny bufor ekranu zmapowany pod adres, którego oczekuje aplikacja (0x00010000), a kompozytor składa go do okna na ET4000 → aplikacja „myśli”, że ma pełny ekran,
- przechwytywanie VDI/AES i przekierowanie rastra do powierzchni okna (clip, skala, paleta),
- przełączanie zadań (Alt+Tab / menu) i akceleracja operacji blit przez blitter W32i,
- tryb awaryjny: uruchomienie aplikacji pełnoekranowo, gdy nie da się jej zwirtualizować.
**DoD:** dwie–trzy natywne aplikacje TOS działają jednocześnie w osobnych oknach, z przełączaniem i akceleracją; brak widocznego Shiftera.

---

## 7. Testy i weryfikacja

- **Emulator:** Hatari `--machine megast --cpulevel 3 --cpuclock 32/50 --memsize 4 --ttram 64 --tf536 on --graphics-card vofa --gfx-chip et4000`; własny `--configfile` (nie ruszać konfiguracji użytkownika); jedna instancja na obraz dysku.
- **Zrzuty:** `tools/hatari_shot.py` (FIFO debuggera), `tools/avi_frames.py` (klatki AVI).
- **Matryca:** {68000, 68030} × {1, 2, 4 MB ST} × {0, 64 MB ALT} × {VOFA, STGA, VME, brak} × {ACSI, IDE, brak}.
- **Regresja:** `gmake check`, `check-graphics`, `check-wm`, `check-gui-apps`; sumy kontrolne ROM.
- **Hardware:** EPROM w podstawce, test na realnym Mega ST + TF536 + VOFA ET4000; porównanie zachowania z Hatari.

---

## 8. Budżet ROM i pamięci

| Pozycja | Rozmiar |
|---|---|
| Obecny kod | ~24 KB |
| Planowany zapas na IDE+config+UI+maprom | ~40–80 KB |
| Limit Mega ST (stock) | 192 KB — **bez presji** |
| Limit „full” | 256 KB |

Wniosek: **nie trzeba kompresji ani wycinania funkcji**; mieścimy POST AWARD, Setup, IDE, config i maprom w 192 KB z dużym zapasem.

---

## 9. Ryzyka i mitygacje

| Ryzyko | Wpływ | Mitygacja |
|---|---|---|
| Bug DMA ACSI (256 B poza bufor) | niestabilny boot Mega ST | Etap 1 przed resztą |
| 192 KB ROM na stock Mega ST | brak miejsca na 256 KB OS | maprom+MMU dla obrazów OS; dwa warianty ROM |
| Nieznany adres IDE TF536 | brak wykrycia dysku | auto‑sonda `0xF00000`/`0xFFF00000` + opcja w config |
| Brak NVRAM na Mega ST | nie zapisze configu | backend dyskowy + RP5C15 (potwierdzić) |
| Geometria ekranu (ST High 400 vs VGA 480) | zły obraz | własne źródło obrazu 640×480 (już w toku w `vofa.c`) |
| ET4000 AX vs W32i | brak/„zły” blitter | detekcja ID + fallback programowy |
| Różnice realnego sprzętu i emulatora | regresje | testy na EPROM + golden zrzuty |
| `make` zamiast `gmake` | cichy zły build | wymusić `gmake` w skryptach/docs |

---

## 10. Pytania — status

**Rozstrzygnięte (2026‑09‑11):**
1. ✅ Nośnik ROM: **256 KB z karty TF** (`bios_mste.bin`, @0xE00000).
2. ✅ Karta: **ET4000 / ET4000‑W32i**, programowane identycznie; BIOS wykrywa odmianę.
3. ✅ Kolejność: **BIOS → własny system wielozadaniowy (ASF) → nakładka graficzna EmuTOS‑style → natywne aplikacje TOS w oknach na ET4000/W32i z akceleracją.**

4. ✅ IDE: wykrywany **automatycznie** (auto‑sonda adresu bazowego).
5. ✅ Config: **partycja C:, katalog `BIOS_CFG`**.
6. ✅ Partycje: BIOS obsługuje **FAT16, FAT32 i standard Atari (AHDI)**.

**Pozostaje:**
1. Czy masz realny sprzęt (Mega ST + TF536 + VOFA ET4000) i programator EPROM do testów na żelazie?

---

## 11. Pierwszy sprint (konkret, 1–2 tygodnie)

1. **Dzień 1–2:** `gmake -B -j4`; uruchomić `atari-030-port/run-bios-vga.sh`; zapisać zrzut referencyjny POST.
2. **Dzień 3–7:** naprawa DMA ACSI (margines + kanarki + weryfikacja), testy 1/4/8 sektorów, usunięcie diagnostyki.
3. **Dzień 8–10:** `hw/video_et4000.c` + tryb 640×480×8bpp + font 8×16 + konsola znakowa; usunięcie wymuszania Shiftera; zrzut z Hatari.
4. **Dzień 11–14:** `hw/ide.c` (reset + IDENTIFY + odczyt) i integracja z `blkdev_ops`; test z obrazem CF w Hatari.

**Efekt sprintu:** stabilny start na Mega ST, obraz POST na VGA i wykryty dysk IDE — fundament pod Etapy 5–6.

---

## 12. Załączniki — źródła

- Źródła projektu: `files-pasted-by-the-user-projekt/` (BIOS, loader, jądro, EmuTOS reference w `work/emutos-reference/`).
- Ustalenia portu 030: `atari-030-port/FINDINGS.md`.
- Plan emulatora: `atari-030-port/HATARI-PLAN.md`; spec wideo: `atari-030-port/VOFA-VGA-SPEC.md`.
- Analiza całości: `analiza-systemu-atari.md`.
- EmuTOS Nova/ET4000: `work/emutos-reference/bios/nova.c`; IDE: `bios/ide.c` (baza `0xfff00000`).
- Forum TF536 (IDE): „TF536 + Atari ST (IDE Interface)” — IDE może być pod `0xF00000`; maprom i 192 KB ROM bootujący TF536: „TF536 EmuTOS and TOS2.06”.
