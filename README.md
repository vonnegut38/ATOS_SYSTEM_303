# ATOS 2.1 — system dla Atari ST/TT (m68000, TF536, ET4000/VOFA)

Własny system operacyjny: jądro, kompozytor okien, pulpit i zestaw aplikacji.
Uruchamiany na Atari ST/TT (emulator Hatari albo sprzęt z TF536) z kartą ET4000/VOFA.
**Aktualna linia projektu to `ATOS 2.1/`** — poprzednie etapy (ATOS v10 na źródłach
EmuTOS, `files-pasted-by-the-user-projekt/`, `atari-030-port/`) zostają w repo jako
archiwum.

- **Punkt wejścia / stan projektu:** [PRZEKAZANIE-2026-10-17.txt](PRZEKAZANIE-2026-10-17.txt)
  (co działa, co jest otwarte, jak testować; punkty 10a–10ad z ostatnich sesji).
- **Target:** Mega ST4 (4 MB ST-RAM) + TF536 (64 MB TT-RAM, IDE) + ET4000 przez VOFA w MegaBus + VGA.
- **Priorytety:** szybkość, bezpieczeństwo, kompatybilność z TOS, nowoczesny wygląd, miejsce na dysku.

## Mapa repozytorium

| Ścieżka | Co to jest |
|---|---|
| `ATOS 2.1/` | **bieżące źródła**: jądro (`system/`), sterowniki (`hw/`), bootloader, UI (`ui/`), aplikacje, testy hostowe (`tests/`), narzędzia (`tools/`), `Makefile` |
| `ATOS 2.1/tests/` | testy hostowe — m.in. ikony 32/48/64 (`test_icons_dat_v5.py`, `test_icons_decoders.py`), tapeta CUSTOM (`test_wallpaper_custom.py`), tryby 4/16 bpp, kompozytor, WM |
| `display-demo/` | harnessy emulatorowe (`stress_atos.py`, `stress_display.py`, `stress_sessions.py`, `stress_console.py`, `test-icons-size.py`, `test-wall-custom.py`), raporty i zrzuty dowodowe |
| `PRZEKAZANIE-2026-10-17.txt` | kanoniczny opis stanu i przepis uruchomienia (sekcja 5) |
| `HANDOFF-20261014.txt`, `NAPRAWA-TRAP-4BPP-20261017.txt`, `ANALIZA-MAGIC-DRAG-20261018.txt` | historia i analizy konkretnych zmian |
| `files-pasted-by-the-user-projekt/`, `atari-030-port/`, `commander*/` | archiwum wcześniejszej linii (ATOS v10 na EmuTOS, fork Hatari, Commander) |

## Budowanie

Wymagania: `m68k-elf-gcc` (albo `m68k-linux-gnu-gcc`), `python3` + Pillow.
Dla emulatora: `cmake` + `ninja` (tylko przy przebudowie Hatari).

```bash
cd "ATOS 2.1"
make TOS=1 -j8          # TOS=1: warstwa GEMDOS/BIOS/XBIOS (obraz demo używa TOS=1)
```

Wynik: `build/system/KERNEL.SYS`, `DESKTOP.APP`, `SETTINGS.APP`, `GFX.APP`, `VOFA.SYS`
oraz `ICONS.DAT` (format v5 — sekcje ikon 32/48/64 generowane z `system/ICONS/*.png`).

Komplet do wgrania na obraz FAT16 (6 plików: KERNEL.SYS, GFX.APP, DESKTOP.APP,
SETTINGS.APP, VOFA.SYS→ET4000.SYS, ICONS.DAT) — `PRZEKAZANIE`, punkt 6.

Uwaga: build wymaga `gmake`/GNU make 4.x (cele grupowe `&:`); `make` 3.81 tego nie obsłuży.

## Testy

```bash
cd "ATOS 2.1"
make check-graphics     # ikony (format v5, dekodery), tapeta CUSTOM, EmuTOS planar/mono
make check-video        # tryby 1/4/8/16 bpp, guard zmiany trybu, warstwa wideo
make check              # pełny zestaw testów hostowych
```

Testy emulatorowe (Hatari + obraz `display-demo/demo-guard.img`):

```bash
cd display-demo
python3 test-icons-size.py boot48     # zrzut startowy musi być BAJT W BAJT z wzorcem 0e821ede…
python3 test-icons-size.py live       # przełączenie rozmiaru ikon 48→64→32 na żywo
python3 stress_atos.py 30             # stress całego systemu (~30 min)
python3 stress_display.py 10          # tryby ekranu, motywy, kolory, tapeta, ikony, siatka
```

## Uruchomienie w Hatari

Scalona binarka z audio (libxmp/ESP32) i poprawnym 4/16 bpp:
`display-demo/hatari-scalony-20261017`. Pełny przepis (TF536, 4 MB ST-RAM, 64 MB TT-RAM,
`--graphics-card vofa --gfx-chip et4000`, `--ide-master <obraz>`) — `PRZEKAZANIE`, sekcja 5.

Uwaga: Hatari przyjmuje `--cpuclock` tylko 8/16/32; 50 MHz to realny TF536, nie emulator.

## Czego w repo nie ma

Obrazy dysków (`*.img`), checkpointy (`*.tar.gz`) i binarki emulatora są celowo poza gitem
(`.gitignore`) — są duże i odtwarzalne (klon `demo-guard.img` + `prepare()` w harnessach).
Kopie zapasowe repo i obrazów testowych leżą na dysku zewnętrznym `D`
(`ATOS-BACKUP/2026-10-20/`, sumy SHA-256 w `README.txt` tej kopii).
