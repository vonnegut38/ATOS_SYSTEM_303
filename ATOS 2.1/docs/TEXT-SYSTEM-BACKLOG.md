# ATOS 2.0 — backlog systemu tekstowego

Data: 2026-09-26. Zbiór otwartych spraw **poza BIOS-em i GUI/AES**, zebrany z:
`docs/terminal-plan.md`, `docs/STAN_ATOS_2_PL.md`, `docs/JEZYKI_PL.md`,
`docs/SHELL-REFACTOR-PLAN.md`, `docs/AUDYT-RAM-KMEM_2026-09-24_PL.md`,
`docs/ANALIZA-ROZMIARU-KERNEL-SHELL_2026-09-24_PL.md`, `docs/PRZEGLAD-PRACY.md`.

Kolejność = proponowany priorytet.

## 1. Stabilność RUN/EXIT — ZAMKNIĘTE (2026-09-26)
- **Weryfikacja runtime:** `run SYSINFO.APP` → `PROCESS READY, PID: 4`; po
  2/4/6 s **brak promptu**, gdy aplikacja działa. `shell_enter` pomija prompt
  przez `shell_foreground_attached`; po nieudanym starcie prompt wraca normalnie.
- **Martwy kod:** `shell_app_exit_text` („PROGRAM EXIT: ") nie był nigdzie
  używany — **usunięty**. Objaw z `JEZYKI_PL.md` dotyczył starego,
  synchronicznego loadera (komentarz przy `shell_do_run`).
- **Otwarte, osobno:** CUA/harness gubi/powiela znaki (zaobserwowane `rrun`
  zamiast `run`) — rozróżnić emulator / autorepeat / wejście OS.

## 2. B3 — containment błędu użytkownika na 68000 — ZAMKNIĘTE (2026-09-27)
- **68030 + 68000: PASS** — `scenario_faults_cpu.py` (CPU=0 i CPU=3) daje
  10/10 izolowanych błędów; `scenario_supervisor_fault.py` (CPU=0/3)
  potwierdza, że błąd nadzorcy nadal panikuje. `gmake check` 24/24.
- **Przyczyna:** ramka **group-1 68000** (bus error 2 / address error 3) nie ma
  SR na +0: `mode@0`, `adres błędu@2`, `opcode@6`, `SR@8`, bez złożonego PC.
  Kernel czytał SR z +0 (otrzymywał `mode word`, którego bit 13 imitował S=1)
  i PC z +2 (adres błędu), więc brał błąd użytkownika za nadzorczy i panikował.
- **Poprawka:** `kernel_exception_install` zapisuje `kernel_cpu_type`; nowe makro
  `kernel_capture_group1` dla wektorów 2/3 czyta SR z `8(%sp)` na 68000;
  `kernel_try_user_fault` testuje bit S z `12(%sp)` dla tych wektorów. Szczegóły:
  `docs/B3-KWALIFIKACJA-WYJATKOW_2026-09-24_PL.md` §B3.3.
- **Otwarte, osobno:** `tos_fault_stub` dla wektorów 2/3 na 68000 (diagnostyczny
  PC; containment persony bez zmian) — razem z B4.

## 3. B4 — odcięcie nieaktywnej persony TOS — ZAMKNIĘTE (2026-09-27)
- **Domyślny profil jest teraz natywny (`TOS ?= 0`).** Persona
  (`tos_prg`/`tos_vdi`/`tos_entry`/`tos_start`) nie jest linkowana do jądra:
  `text 101 241 B / data 128 B / BSS 60 232 B` zamiast
  `125 129 / 157 / 87 308` → **−23 888 B text, −27 076 B BSS**.
- **Utrzymanie kompatybilności:** dwa niezmienione miejsca wołają
  `kernel_tos_run_program` (tablica API modułu i syscall 65). Przy `TOS=0`
  jest stub zwracający `-0x2ff`, więc link się domyka, a shell drukuje
  `TOS PERSONA NOT BUILT IN THIS KERNEL`. `gmake TOS=1` wraca do pełnej
  persony GEMDOS/BIOS/XBIOS/AES-VDI.
- **Weryfikacja:** `atos2-tools/scenario_tos_profile.py` — TOS=0: banner
  natywny, `run SYSINFO.APP` startuje, `tos` pokazuje brak persony; TOS=1:
  persona startuje (`TOS ENV: ... READY`). `gmake check` 34/34.
- **Moduł `TOS.SYS` odrzucony świadomie:** wymagałby API dla ~27 symboli
  jądra + danych i musiałby się zmieścić w ±32 KB przy ~33 KB BSS (patrz
  `ANALIZA-ROZMIARU-KERNEL-SHELL` §10). Profil build-time daje ten sam zysk
  bez ryzyka; ewentualny powrót to `TOS=1`.
- **Otwarte, osobno:** `tos_fault_stub` dla wektorów 2/3 na 68000 (§2).

## 4. Item 7 — `shell_fs` + COPY/MOVE/DEL/MKDIR — ZAMKNIĘTE (2026-09-27)
- **Nowy plik `system/shell_fs.c`** (PIC, `-mpcrel`) implementuje `copy`/`cp`,
  `move`/`ren`/`rename`, `del`/`delete`/`erase`/`rm`, `mkdir`/`md`. Dispatch ma
  **jeden** hook w `shell_module.S` (`shell_unknown_command` → `shell_fs_run`),
  więc assembly nie rośnie o nowe komendy.
- **API:** v6 ogon `kernel_module_api_t` (`copy_path`, `mkdir_path`,
  `delete_path`, `move_path`) mapowany na `kfat_copy_path`/`kfat_mkdir_path`/
  `kfat_delete_path`/`kfat_move_path`. Shell sprawdza `api->size`, więc starsze
  jądro bezpiecznie spada do uruchomienia programu.
- **Weryfikacja:** `atos2-tools/scenario_shell_fs.py` — `md \TMP`,
  `copy \SYSTEM\FAULT.APP \TMP\F.APP`, `dir` (`F.APP 46`), `move` →
  `G.APP`, `del`, `del \TMP` → `PATH NOT FOUND: \TMP`. `gmake check` 39/39
  (doszedł `tests/test_shell_fs.py`).
- **Pułapka modułów (ważne dla kolejnych plików):** SHELL jest ładowany pod
  adresem runtime (`kernel_module.c`: `entry = image + offset`), więc każdy plik
  C shella budujemy `-mpcrel`, a tablica komend trzyma nazwy **inline**
  (`char name[8]`), nie wskaźniki — wskaźniki w danych wskazywałyby adresy
  link-time.
- **Optymalizacja przy okazji:** `type`/`cat` filtruje cały 512-bajtowy blok i
  oddaje go do konsoli **jednym** wywołaniem; wcześniej `shell_puts` na znak
  dawał 512 wywołań API + 512 przejść `term_write`, a przy włączonym capture
  `capture_append` skanował linię za każdym znakiem (O(n²)).

- **Dalsze optymalizacje (2026-09-27):** `type`/`cat` czyta blokami 4 KB
  zamiast 512 B, więc `read_path_at` (który za każdym razem rozwiązuje ścieżkę
  i idzie po łańcuchu FAT od pierwszego klastra) wołany jest 8× rzadziej.
  `term_write()` zwraca liczbę znaków, więc `kconsole_puts()` nie skanuje tekstu
  drugi raz tylko po to, by podbić licznik `pending`. Demo na żywo (okno, nie
  headless): `atos2-tools/scenario_demo_fs.py`.
## 5. Sesje i terminal
- **Ważenie CPU konsol — ZAMKNIĘTE (2026-09-27).** `user_task_quantum` daje
  aktywnej sesji 16 ticków, tłu 1 (16:1 → 5,9 % dla jednego procesu w tle);
  preempcyjny `kernel_user_take_ready`/`user_pick_ready` trzyma proces aż do
  wyczerpania kwantu (wcześniej był czysty round robin co tick). Test:
  `tests/test_cpu_weight.py` (deterministyczny 16:1).
- **Montowania niezależne od sesji — ZAMKNIĘTE (2026-09-27).** `kernel_fat.c`
  ma kontekst montowania per sesja (`fat_mounts[SESSION_COUNT]` +
  `kfat_select_session`); `select_drive` zmienia wolumen tylko bieżącej
  konsoli (usunięto blokadę `ksessions_mask()!=1`), a syscalle i moduł API
  aktywują kontekst sesji procesu. Runtime: tts2 `D:` → `DRIVE MOUNTED`, a
  tts1 dalej na C:.
- **Wiele jednoczesnych desktopów** — nadal otwarte.
- **„Nowy” myszą — ZAMKNIĘTE na poziomie regresji (2026-09-27).** Menu sesji
  otwiera **lewy** przycisk (IKBD koduje lewy jako bit 1; `handle_mouse`
  testuje `press&2`), zgodnie z `tests/test_sessions.c`. Test istniał, ale
  **nie był uruchamiany** przez `gmake check`; teraz `check` woła
  `check-sessions` (plus terminal/konsolę/FAT). Pojedynczego lewego kliku nie
  da się zautomatyzować (`hatari-event` ma tylko `doubleclick`/`rightdown`/
  `rightup`), więc pełne potwierdzenie zostaje dla fizycznej myszy.
## 6. B2 — kontrakty FAT/VOFA
- Osobne testy FAT overwrite i wyrównania VOFA; bezpieczeństwo zapisu przy
  zaniku zasilania (brak dziennika transakcji dla `\BIOS\BIOS.CFG` — mirror
  FAT1/FAT2 naprawiony, przerwanie drugiego zapisu wymaga kontroli dysku).

## 7. Rozmiar / RAM (droga do 512 KB)
- Baza kernela `0x80000` → `0x20000`/`0x30000` (−320…384 KB), spójnie:
  `include/boot_protocol.h`, `linker/kernel-sys.ld`, `system/kernel_entry.S`,
  `loader/bootstage.c`, `tests/test_kernel_load_addr.py`.
- Block cache 128 → 32 sektory (−48 KB); lazy `desktop_framebuffer` (−32 KB
  w profilu tekstowym); `TOS=0` (−13 KB).
- Etapy: LTO + `-fno-tree-loop-distribute-patterns` + flagi; leniwe BSS;
  modularyzacja (TOS/WM/VOFA jako moduły); profil `tiny/text` (~43–48 KB kodu).

## 8. Testy i profil sprzętowy
- **Dłuższy test obciążeniowy — ZAMKNIĘTE (2026-09-27).**
  `atos2-tools/scenario_stress_live.py` (okno, 300 s): 7 cykli, **125 komend z
  ENTER, 0 zgubionych**, mieszanka `memory`/`inputstat`/`tasks`, fault-apki
  (BUSFLT/ADRFLT/DIVFLT + `\NOPE.APP`), `shell_fs` (md/copy/dir/move/del),
  `start hang.app` + `jobs` + `kill`, przełączanie `tts2`/`tts1`, zrzuty co dwa
  cykle. Brak paniki i wycieku procesów.
- Ustalić drugi profil sprzętowy i dopiero potem kwalifikować 030/TT-RAM/MMU/VOFA.

## Poza zakresem (świadomie)
Nowy desktop, MagiC/EmuTOS jako rdzeń, port KolibriOS, VM, ochrona pamięci.