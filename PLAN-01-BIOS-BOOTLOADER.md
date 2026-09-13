# PLAN-01 — BIOS + BOOTLOADER (zakres M1)

Dokument roboczy, podporządkowany PLAN-00-FUNDAMENT. Opisuje **stan istniejący**
(wynik czytania kodu, nie założenia) oraz **luki i kolejność prac** dla M1.

---

## 1. Istniejący łańcuch startowy (fakty z kodu)

1. **ROM**: build/bios_mste.bin, linkowany pod 0x00E00000 (linker/mste-rom.ld),
   wejście **_reset** z boot/reset.S; boot/memdetect.S i boot/exception.S
   przygotowują pamięć i wektory; boot/ramjump.S ma **ram_image_start** (skok do
   obrazu TOS/EmuTOS w RAM) i **system_image_cache_sync**.
2. **firmware_main()** (main.c): memory_detect, cpu_detect_type/fpu,
   machine_detect, memory_detect_fast (ALT-RAM), expansions_detect, cfg_load
   z \BIOS.CFG, rtc_init, shifter_init, ikbd_init, post_run, odliczanie
   autobootu, potem menu F1..F7.
3. **Menu BIOS**: F1 Setup (ui/setup.c), F2 Diagnostics, F3 Hardware Inventory
   (ui/hardware.c), F4 Boot Device (ui/boot.c), F5 Load TOS (ui/tos.c +
   loader/tos.c), F6 demo video8, F7/Enter boot_stage2.
4. **Bootloader**: boot_stage2_run() (loader/bootstage.c) — sonduje ACSI 0..7,
   potem IDE 0..1; waliduje AHDI (suma 0x1234), bierze pierwszą partycję,
   szuka pliku SYSTEM\BOOT.SYS w FAT16, ładuje pod 0x00020000, buduje
   boot_handoff_t i skacze.
5. **BOOT.SYS** (system/boot_main.c + boot_entry.S): z handoffu wywołuje
   start_kernel/load_module i dociąga KERNEL.SYS oraz moduły.

## 2. Co już działa i jest mocne

- Konsola tekstowa 8bpp **na ET4000** (hw/video8.c + hw/nova.c): video8_enter,
  nova_enter_text_vga — BIOS potrafi startować na VGA.
- Detekcja: NOVA/VOFA + chipset, IDE (TF536) przez hw/ide.c, ACSI przez hw/acsi.c,
  NetUSBee na porcie cartridge (hw/expansion.c: RTL8019, ISP1160).
- Config \BIOS.CFG (include/config.h) z polami video_pref, blitter, boot_delay,
  **boot_device** (0 auto, 1 acsi, 2 ide), fast_boot, beep.
- Ujednolicone czytanie sektorów: acsi_read_sector() sam kieruje do
  ide_read_sectors() dla id z bitem IDE_ID_FLAG (0x80|unit).
- Walidacja obrazów (magic ASYSBOOT/ASYSMOD0, suma, ABI) i łańcucha klastrów.

## 3. Luki względem specyfikacji M1

| # | Luka | Gdzie | Wpływ |
|---|---|---|---|
| G1 | F4 Boot Device obsługuje **tylko ACSI**; pole boot_device z configu nieużywane | ui/boot.c | nie da się wybrać IDE TF536 jako urządzenia bootowania z UI |
| G2 | Port cartridge = NetUSBee — jest detekcja, brak podsystemu: sieć RTL8019 i dysk USB jako ostatni dysk | hw/expansion.c, ui/hardware.c | brak sieci i pendrive'a USB |
| G3 | Loader czyta **tylko FAT16**; brak FAT32 | loader/bootstage.c, loader/tos.c | FAT32 ze specyfikacji nieobsłużony |
| G4 | Ścieżki tylko **jednego poziomu** (\SYSTEM\PLIK) | locate_system_file() | docelowe \SYSTEM\BOOT\BOOT.SYS i \SYSTEM\KERNEL\KERNEL.SYS nie zadziałają |
| G5 | BIOS-owy inspektor partycji czyta **tylko 4 sloty główne**, brak łańcuchów XGM/BGM | acsi_inspect_device() | dyski z rozszerzonymi partycjami nie bootują |
| G6 | Rozpoznanie systemu plików (Atari vs FAT16 vs FAT32) nieujednolicone | loader vs kernel_partition.c | dwa różne parsery, różne zachowania |
| G7 | Setup nie ma pola wyboru urządzenia bootowania | ui/setup.c | konfiguracja podstawowa niekompletna |
| G8 | Brak wspólnej warstwy „urządzenie → partycja → system plików" | cały loader | dalsze rozbudowy duplikują kod |

## 4. Decyzje projektowe M1

- **D-B1. Jedna warstwa blokowa.** Identyfikator urządzenia: 0x00..0x07 ACSI,
  0x80..0x81 IDE (jak dziś). API: probe, model, capacity, read/write.
- **D-B2. Wybór urządzenia bootowania** trzymany w \BIOS.CFG (pole boot_device
  rozszerzone do pełnego id) i używany zarówno przez F4/Setup, jak i przez
  bootloader — koniec z „pierwsza znaleziona partycja".
- **D-B3. Jedna tabela wolumenów:** AHDI (GEM/BGM/F32) + łańcuchy XGM + FAT16 +
  FAT32, z rozpoznaniem systemu plików i wspólnym API dla bootloadera i jądra.
- **D-B4. Ścieżki wielopoziomowe** w loaderze (np. \SYSTEM\BOOT\BOOT.SYS),
  najpierw 8.3, LFN później.
- **D-B5. Inwentaryzacja cartridge** jako odczyt nagłówka ROM (magic + nazwa) z
  okno 0xFFFA0000 (zapis 0xFFFB0000) przez istniejące probe_read16 — to ten sam
  port ROM/ISA, którego używa już NetUSBee (ROM_ISA_ADDR w include/hardware.h).
  Bez modyfikowania system_info_t, jeśli da się to zrobić lokalnie.
- **D-B6. Zero regresji:** każdy krok kończy się buildem odtwarzalnym i testem w
  Hatari; boot BIOS-u ma pozostać zielony.
- **D-B7. Config firmware w C:\BIOS\BIOS.CFG** (katalog BIOS na dysku C); gdy
  katalogu nie ma, firmware go utworzy albo użyje zgodnościowego \BIOS.CFG.
- **D-B8. Interfejs BIOS bez okien i przyrostowy** — brudne prostokąty, bez
  pełnego odświeżania ekranu (wzorzec EmuTOS).
- **D-B9. Port cartridge = NetUSBee**: sieć RTL8019 oraz USB ISP1160; dysk USB
  (pendrive) jako dodatkowy, ostatni dysk.

## 5. Kolejność prac (M1)

1. **B1 — inwentaryzacja urządzeń** (ACSI + IDE + NetUSBee) w F3.
2. **B2 — Setup minimalny**: data/czas, wybór urządzenia bootowania, zapis.
3. **B3 — config w C:\BIOS\BIOS.CFG** (podkatalog FAT16).
4. **B4 — przyrostowe rysowanie interfejsu BIOS** (brudne prostokąty).
5. **B5 — bootloader respektuje wybrane urządzenie** (koniec skanowania „na ślepo").
6. **B6 — parser wolumenów**: AHDI GEM/BGM/F32 + XGM, FAT16, FAT32.
7. **B7 — loader FAT32 + ścieżki wielopoziomowe** (\SYSTEM\BOOT\BOOT.SYS).
8. **B8 — NetUSBee**: sieć i dysk USB jako ostatni.
9. **B9 — testy w Hatari** (ACSI/IDE, FAT16/FAT32) + zrzuty.

## 6. Kryteria akceptacji M1

- BIOS startuje na ET4000, a bez karty na Shifterze (bez zmian względem dziś).
- F3 pokazuje IDE (model), ACSI i urządzenia cartridge.
- F4/Setup pozwala wybrać urządzenie i partycję; wybór przeżywa restart.
- Bootloader startuje system z **wybranego** urządzenia (ACSI albo IDE).
- Montaż partycji Atari oraz FAT16 i FAT32.
- Loader odnajduje system pod \SYSTEM\... (co najmniej jeden poziom podkatalogu).

## 7. Test (Hatari)

    atari-030-port/hatari-tf536 --machine megast --cpulevel 3 --cpuclock 32
      --memsize 4 --ttram 64 --tf536 on --graphics-card vofa --gfx-chip et4000
      --mono --zoom 2 --tos build/bios_mste.bin --patch-tos false

(obrazy dysków z atari-030-port/disk; IDE: --ide-master, ACSI: --acsi N=plik)

Zrzut headless — sprawdzony przepis:
    export SDL_VIDEODRIVER=dummy
    ... --disable-video NIE UŻYWAĆ (nie renderuje do zrzutu) ... --cmd-fifo FIFO
    printf 'hatari-event keypress 61\n' > FIFO      (F3 = scancode 0x3D)
    printf 'hatari-debug screenshot /tmp/out.png\n' > FIFO
Komendy w FIFO wymagają prefiksów hatari-event / hatari-debug.

---

## 8. Wybór TOS/EmuTOS z katalogu \ROMS (F2) — stan i problem

**Zrobione i działa:** F2 otwiera ekran wyboru, skanuje \ROMS na partycji
bootującej (ACSI albo IDE), rozpoznaje pliki .PRG/.ROM/.IMG/.BIN i pokazuje je
z wersją TOS. Przykład z Hatari:
    EMUTOSPL.PRG  TOS 2.06  RAM LOAD

**Problem (potwierdzony eksperymentalnie):** obecny loader wyciąga z PRG
"obraz ROM" przez szukanie nagłówka TOS w pierwszych 1024 bajtach. W pliku
emutospl.prg (EmuTOS 1.4) znalazł **29 fałszywych dopasowań**, wybrał pierwsze
(dodatkowo offset 584, addr 0x2CB0) i wskoczył w środek kodu — CPU kończy
"double bus/address error => CPU halted".

**Przyczyna:** zgodnie z readme EmuTOS, emutos*.prg to **programy GEMDOS**
(budowane przez "make allprg"), a nie surowe obrazy ROM. Uruchomienie ich wymaga:
- nagłówka 0x601A (rozmiary text/data/bss),
- wczytania text/data i wyzerowania bss,
- **zastosowania tabeli relokacji**,
- basepage i minimum usług GEMDOS (Malloc/Super itd.).

**Dwie drogi do "uruchom jak z ROMU":**
- **A. Surowy obraz ROM + mapowanie MMU 030.** Bierzemy emutos-*.img (np.
  256 KB), mapujemy blok RAM pod 0x00E00000 przez MMU 68030, kopiujemy obraz i
  skaczemy. To jest dosłownie "jak z ROMU" i **nie wymaga GEMDOS**. Zgodne z
  wcześniejszym ustaleniem z PLAN-MINT ("obraz trzeba zmapować pod 0xE00000").
- **B. Pełny loader PRG + minimalny shim GEMDOS** (basepage, relokacja,
  Malloc/Super i kilka trapów). Wtedy działa emutos*.prg dokładnie tak, jak
  przewidział producent. Więcej pracy; część logiki istnieje już w jądrze
  (system/tos_prg.c, persona TOS T1).

Rekomendacja: **A** dla BIOS-owego wyboru systemu (spójne z "jak z ROMU"),
**B** jako rozszerzenie później — najlepiej w jądrze, które już ma GEMDOS.

### 8a. Droga A (MMU 68030) — implementacja i stan

Zrobione: **boot/mmu030.S** (trampolina pmove w RAM) + **boot/mmu_boot.c**
(budowa tablic, kopia obrazu do ALT-RAM, zmienne warm-boot), podpięte pod
ENTER dla obrazów .IMG/.ROM w ekranie F2.

Trzy realne błędy znalezione po drodze (ważne, nie zgadywać ponownie):
1. **Hatari wymaga jawnego --mmu on** — bez tego PMOVE jest ignorowane i CPU
   czyta ROM BIOS-u, a skok ląduje w pętli halt BIOS-u.
2. **68030 NIE MA stron 512 KB.** Pole PS w TC ma 4 bity (maks. 32 KB).
   Poprawny układ: dwupoziomowa tablica, TC = 0x80F09800 (IS=0, PS=15,
   TIA=9, TIB=8); indeks A = addr>>23, indeks B = (addr>>15)&0xFF,
   offset = addr&0x7FFF.
3. **Tablice translacji nie mogą leżeć w BSS** — EmuTOS ustawia stos na 0x1000
   i je nadpisuje. Przeniesione do ST-RAM pod 0x003F0000 (poniżej phystop).

BLOKER: pierwszy tłumaczony dostęp nadal faultuje (CPU ląduje w handlerze
BIOS-u), mimo TC/CRP i deskryptorów zgodnych z dekoderem WinUAE/Hatari.
Do ustalenia: dokładny format **4-bajtowego** deskryptora strony (możliwe
wymagane bity "reserved" w górnym słowie) albo przejście na format 8-bajtowy.

Diagnostyka: Hatari ma komendę debuggera "mmu" tylko w buildzie z MMU-debug;
alternatywnie dodać własny log MMUSR/reason z handlera błędu.

Uwaga na przyszłość: nawet po uruchomieniu EmuTOS na Shifterze obraz na
karcie VOFA może się nie zmienić, jeśli OS nie steruje ET4000 — trzeba
sprawdzić most Hatari albo dodać sterownik.

### 9. Graficzny BIOS i rysowanie przyrostowe (B4) — ZROBIONE

- **ui/damage.c + include/damage.h**: tracker brudnych prostokątów (max 8,
  scalanie nakładających się, przepełnienie -> pełny ekran 640x480),
  raportuje liczbę prostokątów i pikseli.
- Wpięty w konsolę: **console_clear**, **console_clear_text_row** i
  **console_putc** rejestrują dotknięty obszar automatycznie — każde rysowanie
  jest liczone, bez zmian w kodzie ekranów.
- **Ekran Setup (F1)** przerobiony na graficzny panel (pasek tytułu, pola)
  z odświeżaniem WYŁĄCZNIE zmienionych wierszy. Na ekranie licznik:
  "REDRAW: n RECT p PX   FULL SCREEN = 307200 PX".
- **Wynik w Hatari** (Mega ST + TF536 + VOFA, zrzuty bios-gui-*.png):
  - menu główne: REDRAW 1 RECT 10240 PX (sam pasek opcji),
  - wejście do Setup: 1 RECT 307200 PX (pełny ekran, jednorazowo),
  - nawigacja strzałką: 1 RECT 10240 PX (jeden wiersz 640x16) = **30x mniej**.
- TOS-y odłożone na życzenie — kod pozostaje, ale nie jest dalej rozwijany.
- **F4 BOOT DEVICE** przerobiony tak samo jak Setup: pasek tytułu, linia
  **CARTRIDGE** (NetUSBee), lista wykrytych urządzeń, przyrostowe odświeżanie.
  Wynik w Hatari: wejście 1 RECT 307200 PX, nawigacja **1 RECT 20480 PX**
  (dwa sąsiednie wiersze scalone) = **15x mniej** niż pełny ekran.
- **F3 (Hardware Inventory)** i **F5 (Diagnostics)** dostały ten sam pasek
  tytułu (odwrócony) i licznik kosztu. F5 podczas nawigacji: 1 RECT 20480 PX
  (dwie sąsiednie pozycje scalone) = 15x mniej niż pełny ekran. F3 najpierw się
  przewijał i gubił tytuł — skompaktowany do 25 wierszy, teraz mieści się cały.
- Zrzuty: bios-gui-setup.png, bios-gui-incremental.png, bios-gui-menu.png,
  bios-f4-bootdevice.png, bios-f4-incremental.png, bios-f3-inventory.png,
  bios-f5-diagnostics.png.

### 10. B7 — ładowanie systemu ze ścieżek wielopoziomowych — ZROBIONE

- **loader/bootstage.c**: ogólny walker FAT16 (mount_geometry, dir_find,
  path_lookup) obsługujący dowolny łańcuch katalogów:
  - BOOT.SYS   -> \SYSTEM\BOOT\BOOT.SYS
  - KERNEL.SYS -> \SYSTEM\KERNEL\KERNEL.SYS
- **tools/install_path_ahdi.py**: tworzy zagnieżdżone katalogi 8.3 i wgrywa
  pliki. Naprawiony błąd: alloc() nie oznaczał klastra jako zajętego, przez co
  każdy łańcuch >1 klastra był self-loopem (wyszło dopiero na 21-klastrowym
  KERNEL.SYS).
- **Wynik w Hatari** (zrzut bios-system-boot.png): pełny łańcuch
  BIOS -> \SYSTEM\BOOT\BOOT.SYS -> \SYSTEM\KERNEL\KERNEL.SYS -> jądro:
  C: BGM 63 MB, FAT16 mounted, BLOCK CACHE w TT-RAM,
  moduły CORE/VFS/PROC/TIMER/EVENT/INPUT READY,
  PROCESS MANAGER 2 TASKS / FULL CTX RR, TIMER 4Q/WAKE, EVENT WAIT/WAKE,
  IKBD POLLING, KERNEL RUNNING / PREEMPTIVE / SHELL READY,
  usługi SYSTEM.INIT / SYSTEM.GFX / SYSTEM.GUI Ready, prompt C:\>.
### 10a. B7b — moduły i usługi z \SYSTEM\KERNEL\ — ZROBIONE

- **kernel_fat.c**: locate_system_file najpierw szuka w \SYSTEM\KERNEL\,
  potem fallback \SYSTEM\<nazwa> (starsze wolumeny dalej bootują).
- **kernel_main.c**: ścieżki startup services zmienione na
  \SYSTEM\KERNEL\{INIT,GFX,GUI}.APP.
- **Wynik w Hatari** (zrzut bios-system-boot.png): cały system wczytywany z
  \SYSTEM\BOOT\ i \SYSTEM\KERNEL\ — moduły CORE/VFS/PROC/TIMER/EVENT/INPUT
  READY, usługi SYSTEM.INIT / SYSTEM.GFX / SYSTEM.GUI Ready, shell C:\>.

### 11. B6 — FAT16 + FAT32 (i wolumeny AHDI) — ZROBIONE

- **loader/bootstage.c**: geometria 32-bitowa, detekcja FAT16/FAT32
  (spf16==0 i spf32!=0), katalog główny jako łańcuch klastrów, 32-bitowe
  wpisy FAT. Uwaga na pułapkę: w katalogowym wpisie FAT32 pierwszy klaster to
  (HI<<16)|LO, gdzie LO jest pod offsetem 26, a HI pod 20 — czytanie le32(26)
  zlepia klaster z rozmiarem pliku.
- **system/kernel_fat.c**: mount FAT32 (root jako klaster, data_start bez
  obszaru root), 32-bitowe read/write wpisów FAT, flagi wolumenu z BPB+40,
  kfat_fat32() do etykiety.
- **system/kernel_main.c**: "FAT LAYER: FAT32 MOUNTED" / "FAT16 MOUNTED".
- **tools/make_fat32_ahdi.py**: formater AHDI + FAT32 (ID "F32").
- **tools/install_path_ahdi.py**: obsługa FAT16 i FAT32.
- **Wynik w Hatari**: boot FAT16 (bios-system-boot-fat16.png) i FAT32
  (bios-system-boot-fat32.png) — w obu: C:, moduły, usługi INIT/GFX/GUI,
  shell C:\>, VOLUME FLAG: CLEAN.
### 11a. B6b — 32-bitowe numery klastrów — ZROBIONE

- **system/kernel_fat.c**: cała warstwa FAT na uint32_t klastrach
  (read/write_fat_entry, scan_directory_sector, scan_any_sector,
  find_in_directory, scan_dir_chain, resolve_path, split_path, alloc_cluster,
  load/write, list). EOC/bad zależne od typu: eoc_min() = 0x0FFFFFF8 /
  0xFFF8, cluster_bad() = 0x0FFFFFF7 / 0xFFF7. Limit FAT32 podniesiony do
  0x0FFFFFF5 klastrów.
- **Loader** był już 32-bitowy.
- **Wynik w Hatari** (bios-system-boot-fat32-511mb.png): wolumen FAT32
  511 MB / **130 811 klastrów** montuje się (FAT32 MOUNTED, CLEAN),
  moduły i usługi startują. Regresja FAT16 (bios-system-boot-fat16.png)
  czysta — BGM, FAT16 MOUNTED, CLEAN, shell C:.

### 12. Port cartridge — detekcja ROM kartridża — ZROBIONE (B8a)

- **hw/expansion.c**: cartridge_detect() sonduje 0xFFFA0000 na magic
  0xABCD i czyta nazwę programu spod offsetu 0x14 (przez probe_read, więc
  puste gniazdo nie faultuje). Wynik w system_info: cart_rom_present,
  cart_rom_name.
- **ui/hardware.c (F3)**: linia CARTRIDGE ROM <nazwa>.
- **ui/boot.c (F4)**: CARTRIDGE: ROM <nazwa> obok NetUSBee.
- **Weryfikacja**: Hatari --cartridge /tmp/testcart.img -> F3 i F4 pokazują
  TESTCART (zrzuty bios-cartridge-f3.png, bios-cartridge-f4.png).
- **B8 (NetUSBee) zablokowane w emulatorze**: Hatari nie emuluje RTL8019
  ani ISP1160 (brak jakichkolwiek odwołań w źródłach), więc sieci i dysku
  USB nie da się zweryfikować bez realnego sprzętu. Kod detekcji NetUSBee
  już istnieje; sterowniki można dopisać, ale test dopiero na żelazie.

### 13. B9a — łańcuchy XGM w BIOS-ie — ZROBIONE

- **system_info**: 16 slotów partycji (acsi_part_id[16][4], acsi_part_start[16]),
  acsi_partitions jako 32-bitowa maska, plus acsi_part_count.
- **hw/acsi.c**: acsi_inspect_device czyta 4 sloty główne, a dla wpisu XGM
  podąża za łańcuchem deskryptorów (wpis logiczny + link), doliczając kolejne
  wolumeny; limit 16 i ochrona przed pętlą.
- **ui/boot.c (F4), hw/config.c, ui/tos.c, loader/bootstage.c**: 32-bitowa
  maska i licznik zamiast sztywnego 4.
- **tools/make_xgm_ahdi.py**: obraz primary XGM -> deskryptor -> BGM.
- **tools/install_path_ahdi.py**: rozpoznaje XGM przy wyborze partycji.
- **Weryfikacja w Hatari**: F4 pokazuje PARTITION P0 BGM START 3, a system
  bootuje z tej partycji (bios-xgm-f4.png, bios-xgm-boot.png). Wcześniej
  BIOS widział 0 partycji, bo XGM nie jest typem wolumenu.

Pozostaje (B9b): LFN (długie nazwy) w FAT32 — na razie tylko 8.3.

### 14. M0 (reszta) — wersjonowanie repo — ZROBIONE

- **git init** w korzeniu projektu. `.gitignore` wyklucza: obrazy dysków
  (*.img), archiwa (*.zip), media, `**/build/`, `**/outputs/`,
  `atari-030-port/disk/` (dane), `atari-030-port/Hatari.app/`, binarkę
  `hatari-tf536`, vendored `tools/py/` oraz drzewa z własnym `.git`
  (`hatari-src`, `work/emutos-reference`).
- **Pierwszy commit**: 406 plików, ~19 MB `.git` (bez obrazów i zależności).
  Hash 7ca126e.
- Uwaga: modyfikacje forka Hatari (vofa.c, tf536, graphics-card) siedzą w
  osobnym repo `atari-030-port/hatari-src` i trzeba je zacommitować tam
  osobno — inaczej znikną przy `git clean` w tamtym drzewie.

### B9b — LFN — ODŁOŻONE z uzasadnieniem

Struktura wpisu katalogu dla natywnego shella (`user_dir_entry_t`, name[13])
jest częścią ABI czytanego przez SHELL.SYS (asembler). Rozszerzenie na
name[64] wymaga skoordynowanej zmiany jądra i shella, więc LFN robimy jako
osobny, świadomy krok, a nie przy okazji.

Podsumowanie graficznego BIOS-u: pasek tytułu, ramka pól, licznik
"REDRAW: n RECT p PX" na każdym ekranie; interaktywne ekrany (menu, F1, F4,
F5) odświeżają wyłącznie zmienione wiersze, pełny rysunek tylko raz przy
wejściu.
