# ATOS 2.1 — PUNKT STARTOWY

Kopia robocza projektu ATOS 2.0, przeniesiona do ATOS 2.1. To katalog, od
którego zaczyna się dalsza praca. Data kopiowania: 2026-09-26.

## 1. Co przeczytać NAJPIERW
Dokumenty w docs/, w tej kolejności:
1. PRZEGLAD-PRACY.md — niezależny przegląd stanu punktów 1-8 (dowody plik:linia)
2. UI-BOOT-ZADANIA.md — 3 bieżące zadania UI/boot
3. BIOS-BRINGUP-CHECKLIST.md — co sprawdzić przed fizycznym Atari
4. V10-STATUS.md — status (UWAGA: miejscami nieaktualny i sprzeczny)
5. SHELL-REFACTOR-PLAN.md — plan rozdzielenia shell_module.S

## 2. Budowanie
    cd "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
    pkill -9 -f hatari
    python3 tools/build_atos2.py --install

Bez --install: tylko kompilacja i testy hostowe.
Przed --install ZAWSZE zabij Hatari — obraz work/atos-v2.img jest wspólny.

## 3. Testowanie
Harness: atos2-tools/atos2_auto.py (ścieżki wewnątrz już wskazują na ATOS 2.1).
Emulator: /Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536
  (wrapper shellowy; źródła w atari-030-port/hatari-src). Leży POZA projektem,
  więc działa bez zmian także z ATOS 2.1.
Przykład:
    pkill -9 -f hatari
    python3 atos2-tools/scenario_abifix.py

## 4. Artefakty
- build/atos-bios-tf536-st-dual.img — 512 KB, EPROM: bank1 ST + bank2 MSTE
- build/atos-bios-st-68000.img — 256 KB, emulatory 68000
- build/atos-bios-mste.img — 256 KB
- work/atos-v2.img — nośnik systemu, ~768 MB

## 5. Stan na moment kopiowania
- Build zielony, ABI CHECK PASS, boot dochodzi do pulpitu Commander.
- ROM potwierdzony na dwóch emulatorach: patched Hatari + noSTalgia (68000, mono).
- Oryginał projektu: /Users/xxx/Documents/Codex/ATOS 2.0 — NIE modyfikować.

## 6. Znane pułapki
- Harness gubi/powiela znaki przez ACIA — "PROCESS START FAILED" bywa
  artefaktem testu, nie błędem systemu. Weryfikuj echo linii przed wyciąganiem
  wniosków.
- V10-STATUS.md ma sprzeczne i nieaktualne wnioski — nie ufaj bez sprawdzenia.
- system/AUTOEXEC.BAT to MARTWY plik: nie jest instalowany ani wykonywany.
- Zapis na ACSI = no-op (hw/acsi.c) — na fizycznym ST/STE zapisy konfiguracji
  i FAT mogą milcząco przepadać.
- diag_ram_pattern istnieje, ale NIE jest wołany; POST nie odpala diag_*;
  brak sumy kontrolnej ROM.
- "LOADING COMPONENTS" to jeden aktualizowany wiersz — zamierzone, ale nieczytelne.
- Desktop 800x600x256 JEST zdefiniowany (system/desktop_wm.c) — trzeba go
  ustawić jako domyślny.

## 7. Czego NIE ma w tej kopii
- W work/ jest tylko atos-v2.img i BIOS.CFG. Stare obrazy testowe i logi
  zostały w ATOS 2.0/work (2,3 GB).
- atari-030-port/hatari-src (98 MB) zostało na miejscu — patrz punkt 3.
