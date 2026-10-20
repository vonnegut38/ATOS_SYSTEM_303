# BIOS — checklista przed włożeniem do fizycznego Atari

## Stan obrazów (2026-09-26)
- **ST** (bank1): .header 196 B + .text 41 690 B ~ 42 KB kodu w oknie 192 KB
  (0xFC0000-0xFEFFFF) -> **~150 KB wolne**
- **MSTE** (bank2): ten sam kod w oknie 256 KB (0xE00000-0xE3FFFF) -> **~214 KB wolne**
- .bss 11 442 B leży w RAM pod 0x410 (nie zajmuje ROM)
- Dual 512 KB: bank1 ST (padding 0xFF), bank2 MSTE

## Zrobione w tej sesji
**POST na ACIA MIDI 0xfffffc06** — 31250 baud z zegara sprzętowego, więc
**nie wymaga żadnej konfiguracji MFP** i działa od pierwszej instrukcji:
- A = wejście BIOS
- C = cold boot / W = warm reset (na podstawie 0x0420)
- R = wykryty sufit RAM, potem jeden bajt = RAM w MiB
- F = wejście do firmware_main

**Cold/warm boot** — 0x0420 jest teraz czytane, raportowane jako C/W
i **zawsze czyszczone**, żeby następny start był z znanego stanu.

## Do sprawdzenia na kości (czego emulator nie pokaże)
1. **68000 vs 68030** — boot/mmu030.S i boot/mmu_boot.c nie mogą się wykonać
   na ST (MOVEC = illegal instruction). Sprawdzić, że ścieżka ST tam nie wchodzi.
2. **Koniec RAM** — 512 KB / 1 MB / 4 MB; na ST nie ma TT-RAM.
3. **Warm reset** (przycisk / Ctrl+Alt+Del) — RAM nie jest czyszczony.
4. **Timing FDC / DMA / ACSI** — na silikonie wolniejszy niż w Hatari.
5. **Obecność RTC** (MSTE) i wersja IKBD.
6. **Padding 0xFF** — OK, to stan skasowanego EPROM-u.

## Do dodania (miejsca dużo)
1. **ODŁOŻONE** — POST na **RS-232 (0xfffffc04)** wymaga zegara MFP
   (Timer D + USART); patrz „Korekta priorytetów” niżej (RS-232 odpada na tym
   etapie). MIDI działa już od resetu.
2. POST z testami: RAM (wzorce), ROM checksum, IKBD echo, timer MFP,
   status FDC, RTC, wykrycie TT-RAM.
3. Ekran awaryjny — czytelny komunikat na mono i color (baza: diag/*, ui/post.c).
4. Boot menu / recovery z FDD, gdy HDD/IDE zawiedzie (docs/FLOPPY-BOOT-PLAN.md).
5. Suma kontrolna BIOS-u + wersja/data w .header (196 B — sprawdzić, czy suma jest).
6. Ekran powitalny: wersja, typ maszyny, RAM, wykryty sprzęt.
7. Obsługa cartridge'a diagnostycznego i watchdog przy zawieszeniu POST.
8. Minimonitor w ROM (serial/klawiatura): podgląd pamięci, restart.

## Jak czytać POST
PC + adapter MIDI, 31250 8N1. Przykładowy ślad startu: A C R 04 F.

## Korekta priorytetów (decyzja 2026-09-26)
RS-232 **odpada** na tym etapie. Kolejność: **testy POST -> ekran awaryjny ->
ekran powitalny z konfiguracją**. Wszystko ma działać na 68000.

### Co JUŻ istnieje — nie duplikować
- **Ekran powitalny z konfiguracją = `ui/post.c:post_run()`** — wypisuje
  DATE/TIME, MACHINE, CPU, CLOCK, CACHE, SHIFTER, RAM, ALT-RAM, BLITTER,
  potem menu (F1 SETUP / F2 DIAGNOSTICS / F3 HARDWARE / F4 BOOT / F5 LOAD TOS /
  F10 SAVE & EXIT / CONTINUE BOOT) i odliczanie AUTO BOOT.
- **Testy sprzętu = `diag/tests.c` + `include/diagnostics.h`**:
  `diag_cpu`, `diag_stram`, `diag_ttram`, `diag_shifter`, `diag_blitter`,
  `diag_mfp`, `diag_ikbd`, `diag_rtc` + `diag_result_name`
  (PASS / FAIL / NOT_PRESENT / UNSUPPORTED).
- **Menu bootowania = `ui/boot.c:boot_menu_run()`** (urządzenie + partycja).

### Czego faktycznie brakuje (stan 2026-09-26)
1. ~~**Automatyczne uruchomienie testów w POST**~~ — **ZROBIONE**:
   `ui/post.c` woła `diag_*` (w tym `diag_ram_pattern`) i rysuje
   `POST TESTS ALL PASS/FAIL`.
2. ~~**`diag_ram_pattern` martwy kod**~~ — **ZROBIONE** (wołany w POST).
3. ~~**Suma kontrolna ROM-u**~~ — **ZROBIONE**: `diag_rom_checksum` +
   `tools/patch_rom_checksum.py` (pole w nagłówku 0x14). Hatari: dobry ROM →
   `ROM:PASS`, przekłamany bajt → `ROM:FAIL`.
4. ~~**Ekran awaryjny**~~ — **ZROBIONE**: przy FAIL wiersz 23
   `*** POST FAILURE ... ***` i **brak auto-bootu** (`post_tests_failed()`).

### Kolejność wdrożenia
1. ~~Spiąć `diag_*` w POST~~ — **ZROBIONE** (POST + PASS/FAIL).
2. ~~Dodać `diag_rom_checksum`~~ — **ZROBIONE**.
3. ~~Ekran awaryjny na FAIL~~ — **ZROBIONE**.
