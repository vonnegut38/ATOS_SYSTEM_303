# Rozszerzenie Hatari: TF534 (Mega ST) + ET4000/W32i

Plan i ustalenia. Cel: Hatari ma emulować docelową konfigurację
**Mega ST + TF536 (68030 50 MHz, 64 MB ALT-RAM, IDE) + adapter STGA z ET4000/W32i**,
żeby dało się na niej uruchamiać i debugować nasz system bez realnego sprzętu.
FPU pomijamy.

> Korekta ustaleń: pierwotnie zakładaliśmy TF534 + Mega4000. Użytkownik potwierdził,
> że docelowy sprzęt to **TF536 (64 MB)** i **adapter STGA** (Till Harbaum).
> Mapy adresowe tych dwóch adapterów są **różne** — patrz niżej.

## 1. Co już wiemy (ustalone, nie zgadywane)

### Adapter graficzny STGA — mapa adresowa
Źródło: atari-home.de „Basisadresse IO und Speicher bei Grafikkarten”,
wpis autora adaptera Mega4000 (Lukas Frank):

```
STGA:
  Screenbaseaddress:   $FEC00000
  Registerbaseaddress: $FECF03B0
  "16bit I/O" muss gewählt werden
  CLUT: 8bit 00 / 15bit A0 / 16bit C0 / 24bit F0
```

Interpretacja: VRAM = `0xFEC00000` (1 MB), a rejestry VGA są wystawione jako
**okno `0xFECF0000` + numer portu VGA** — stąd `0xFECF03B0` to port `0x3B0`.
Zakres portów VGA: `0x3B0`–`0x3DF`. **I/O 16-bit** (inaczej niż Volksfarben).

| Adapter | VRAM | Rejestry | Szerokość I/O |
|---|---|---|---|
| **STGA** (nasz cel) | `0xFEC00000` | `0xFECF0000` + port VGA | **16-bit** |
| Mega4000 / Volksfarben | `0x00C00000` | `0x00D00000` | 8-bit |
| Nova VME (MegaSTE/TT) | `0xFEC00000` | `0xFEDC0000` | 16-bit |
| CrazyDots | `0xFEC00000` | `0xFEBF0000` | 16-bit |

Uwaga: VRAM `0xFEC00000` jest **wspólny** dla STGA, Nova VME i CrazyDots —
różni je okno rejestrów. To upraszcza model w Hatari: jedno okno VRAM,
przełączane okno rejestrów.

**Nasze `hw/nova.c` nie zna STGA** — ma tylko Nova VME (`0xFEDC0000`),
CrazyDots (`0xFEBF0000`) i Vofa (`0x00D00000`). Trzeba dodać czwarty wariant.

### TF536 — co daje karcie
- 68030 @ 50 MHz, **64 MB ALT-RAM** (SDRAM), **IDE**, bez FPU (w TF536 usunięty).
- Na ST działa jako „ST536” (firmware exxos/agranlund); bootuje **TOS 2.06 z rev5
  fixes** lub **EmuTOS**; w Mega ST4 potwierdzone działanie (wątek „ST536 in a Mega ST4”).
- **IDE**: praktycznie używane z adapterem IDE2CF/IDE2SD + HDDriver; EmuTOS bootuje
  bez dodatkowych przewodów, Atari TOS wymaga przewodu INT2 → ACSI pin 10.
- EmuTOS używa dla Atari IDE bazy **`0xfff00000`** (`bios/ide.c`) — wspólnej dla
  Falcona i akceleratorów; to kandydat na okno IDE.
- Praktyka z forum: przy ST536 **cache wyłączone**, HDDriver **bez TTRAM** (stabilność).


### Hatari — punkt zaczepienia
- **VME nie jest emulowane**: `src/scu_vme.c` mówi wprost „we don't emulate any VME
  board at the moment”, ale zna okna adresowe VME (TT `0xFE000000`, MegaSTE `0x00A00000`).
- **IDE jest**: `src/ide.c` (2834 linie, pełne ATA), wpięte przez warstwę pamięci CPU
  (`src/cpu/memory.c:1204` → `Ide_Mem_bget/…`, bramkowane `Ide_IsAvailable()` /
  `Ide_IsValid()`), a nie przez tabele `ioMemTab*`. Podłączenie do ST to zmiana
  zakresu adresów + włączenie `Ide_Init()` dla maszyn ST.
- **Wyświetlanie**: `src/video.c` + `src/screen.c`; nowe źródło obrazu (VGA) trzeba
  wprowadzić tak, by dało się je wybrać zamiast Shiftera.

## 2. Plan implementacji (etapami, każdy weryfikowalny)

### Etap A — budowa i baseline
- [x] Źródła Hatari (`git clone`, `atari-030-port/hatari-src`).
- [x] CMake + Ninja bez admina (pip `--target tools/py`).
- [x] Konfiguracja: `-DCMAKE_OSX_ARCHITECTURES=arm64` (bez tego buduje x86_64
      i nie linkuje bibliotek Homebrew).
- [ ] Build przechodzi; zbudowany Hatari uruchamia nasz ROM.
- [ ] Uruchamiać z **własnym plikiem konfiguracji** (`--configfile`), żeby nie
      dotykać konfiguracji użytkownika.

### Etap B — akcelerator TF536
- [ ] Nowa opcja (np. `--accelerator st536`) aktywna dla maszyn ST/MegaST.
- [ ] **ALT-RAM** pod `0x01000000`, domyślnie **64 MB** (rozmiar konfigurowalny) —
      odblokowuje `--ttram` na MegaST i pozwala testować pulę TT jądra.
- [ ] **IDE**: `Ide_Init()` + `Ide_IsValid()` dla okna TF536 (kandydat `0xFFF00000`),
      `--ide-master` działa na MegaST.
- [ ] TOS/system z EPROM karty = nasz obraz ROM przez `--tos` (już działa).

### Etap C — karta ET4000 na adapterze STGA
- [ ] Nowe urządzenie: okno VRAM `0xFEC00000` + 1 MB, okno rejestrów
      `0xFECF0000` + port VGA (`0x3B0`–`0x3DF`), **16-bit I/O**.
- [ ] Sygnatury obecności: odczyt `0xFECF03C3` (`VIDSUB`) musi się udawać.
- [ ] Podzbiór rejestrów ET4000: sekwencer, CRTC, kontroler graficzny, atrybuty, DAC
      (paleta), rozszerzenia Tseng — dokładnie tyle, ile używa sterownik.
- [ ] VRAM 1 MB + paleta (CLUT: `00`/`A0`/`C0`/`F0` dla 8/15/16/24-bit); obraz VGA
      widoczny w oknie Hatari.
- [ ] **W32i: blitter** (kopiowanie/wypełnianie) — to jest „akceleracja sprzętowa”
      z celu projektu; ET4000AX blittera nie ma, W32i ma.

### Etap D — strona systemu (równolegle)
- [ ] `hw/nova.c`: dopuścić ścieżkę ST (Volksfarben) na `MACHINE_MEGA_ST`
      (dziś sprawdza tylko MegaSTE/TT przy VME). Stałe już są.
- [ ] Sterownik ET4000 (tryb graficzny, paleta, blit) — referencją rejestrów jest
      `work/emutos-reference/bios/nova.c`.
- [ ] Sterownik IDE dla TF534 (nowy sterownik blokowy, obok ACSI).

## 3. Rozstrzygnięte i otwarte

Rozstrzygnięte:
1. **Adapter**: STGA (Till Harbaum) — VRAM `0xFEC00000`, rejestry `0xFECF0000`, 16-bit I/O.
2. **Karta graficzna**: ET4000/W32i — czyli **z blitterem** (W32i), nie AX.
3. **Akcelerator**: TF536, **64 MB ALT-RAM**.

Otwarte:
1. **Okno IDE TF536/ST536** — czy pod `0xFFF00000` (jak Falcon i EmuTOS), czy pod
   adresem z firmware karty. Do potwierdzenia z dokumentacją/firmware ST536;
   do czasu ustalenia zrobię to jako opcję konfiguracyjną.
2. **Sterownik STGA** — czy używać podejścia z NVDI/VMG (rejestry jak wyżej),
   czy pisać własny sterownik oparty o `bios/nova.c` z EmuTOS jako referencję rejestrów.
   Rekomendacja: własny sterownik, NVDI jako źródło prawdy o rejestrach.

## 4. Stan roboczy

- `hatari-src/` — klon źródeł Hatari (do modyfikacji).
- `tools/py/` — cmake + ninja (pip, bez admina).
- `disk/work.img` — obraz roboczy z naszym `KERNEL.SYS`.
- `FINDINGS.md` — ustalenia z portu systemu (bug DMA na MegaST, TT-RAM).
- Konfiguracja użytkownika (`~/Library/Application Support/Hatari/hatari.cfg`)
  **nietknięta** — do testów używamy własnego pliku.
