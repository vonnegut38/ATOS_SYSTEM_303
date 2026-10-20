# TOS w oknie — punkt kontrolny (2026-10-04, po audycie)

Cel: program TOS (.PRG/.TOS) w oknie pulpitu ATOS; rozmiar/format za trybem programu.

## Wykonane w tej turze (wg kolejności audytu A→E)

### A. Pamięć obrazu + układ bitplanów — GOTOWE
- `kgfx_blit_planar()` (`system/kernel_graphics.c`) przepisany na **przeplatany układ
  słów** standardowego ekranu ST: adres = `y*stride + (x/16)*(2*planes) + 2*plane`,
  bit `15-(x&15)`, słowo big-endian. Stary model (osobne bloki `plane_bytes*plane`)
  czytał płaszczyznę 3 od offsetu 96000 z bufora 64 KB — **usunięte**.
- Dodany `source_capacity` (limit pojemności) w sygnaturze.
- `toscolour.c` przepisany na ten sam kontrakt (wcześniej pisał osobne bloki 8000 B).

### B. Setscreen ABI + prywatny bufor — GOTOWE
- Handler `Setscreen` (`tos_prg.c`): `rez` czytany jako `int16_t a[4]` (nie `a[4]<<16|a[5]`),
  `log=-1`/`log=0` = zostaw obecny ekran.
- `toscolour.c`: poprawne opakowanie `setscreen(log, phys, rez)` (WORD rez, LONG phys,
  LONG log, WORD fn na stosie).
- **Tryb okna ustalany PRZED wyborem ekranu**: `tos_window_enabled=1` na początku
  `kernel_tos_run_program()`, przed `kernel_tos_bootstrap()`. Prywatny bufor 64 KB
  zawsze dla okna; brak pamięci = błąd (nie fallback na live VRAM); po zakończeniu
  `tos_window_enabled=0`.

### C. Mono + wspólny dispatcher + wielkość liter — GOTOWE
- **Mono podłączone**: `kgfx_blit_mono()` + `tos_blit_mono` (wcześniej `blit_mono=0`,
  więc program mono rysował nic).
- **Jeden wspólny matcher** `include/tos_ext.h` (`tos_program_path_ext` /
  `tos_ext_is`): rozpoznaje `.PRG/.TOS/.TTP/.GTP` bez rozróżniania wielkości liter.
  Używany przez wszystkie punkty uruchamiania; regresja hostowa `check-tos-ext`.
- **Skróty (launcher)** i **pliki (menedżer)** już trasowały PRG/TOS do
  `ATOS_CALL_TOS_RUN`; teraz oba idą przez wspólny matcher (TTP/GTP też).
- **Polecenia (RUN... pulpitu)** — `spawn_run()` trasuje nazwy TOS do
  `ATOS_CALL_TOS_RUN` zamiast bezwarunkowego `SYS_SPAWN_ASYNC`.
- **Polecenia (shell)** — `shell_tos_dispatch()` w `shell_fs.c` (wywoływany z
  `shell_start_launch`) trasuje `RUN`/`START`/nazwę bez rozszerzenia do
  `module_api.tos_run`; kod wyjścia raportowany jako `TOS RC <n>` (bez libgcc
  `__udivsi3`/`__umodsi3`).
- **Wielkość liter**: jedno wspólne, bezskładniowe porównanie `a..z -> A..Z`
  (koniec z 24-bitowym składaniem + dwoma porównaniami 32-bitowymi).

## Zablokowane / do zrobienia

### P2 #7 — NUL w komunikacie kodu wyjścia — ZROBIONE
Formatter w `launch_run` jest teraz bezpieczny: NUL po cyfrach, znak minus dla
ujemnego kodu, negacja bez znaku (`0u-a`) poprawna nawet dla INT32_MIN.
`tos_program_path` uproszczony do `fb_ext_is` (już case-insensitive) zamiast
składania 24-bitowego + dwóch porównań 32-bitowych — dzięki temu formatter się
zmieścił w limicie `-mpcrel`.

### D — model integracji WM — USTALONY (okno na pulpicie)
Model jest ustalony: **program TOS rysuje w oknie nałożonym na pulpit**, a nie
na osobnym pulpicie. `tos_open_window` nie czyści już ekranu ani nie rysuje
własnego pulpitu (FLOPPY/HARDDISK/TRASH). Zamiast `tos_draw_desktop` jest
`tos_draw_window_chrome`: okno (ramka + belka + klient) jest **nakładane na
istniejący pulpit**, a po zakończeniu `tos_close_window` nic nie przerysowuje —
aplikacja uruchamiająca (pulpit) maluje się sama po powrocie z `gui_call(65)`.
Zrobione też: **ciężka konwersja przeniesiona z przerwania Timer C do pętli
głównej** (`tos_composite_tick` tylko drenuje wejście + ustawia flagę;
`tos_composite_work` robi konwersję z `kernel_main`). Usuwa „1:1 client froze".
Pozostało (dalszy krok, poza tą turą): prawdziwy obiekt WM (okluzja/Z-order)
oraz asynchroniczny syscall 65; model „okno zamiast osobnego pulpitu" jest już
wdrożony i nie wymaga osobnego pulpitu TOS.

### E — test akceptacyjny z dwoma uruchomieniami — NAPISANY
`atos2-tools/scenario_tos_accept.py`: po zimnym starcie uruchamia **dwa** razy
program TOS przez wspólny dispatcher shell (`run \SYSTEM\TEST\TOSPROBE.PRG`,
potem `run \SYSTEM\TEST\TOSCOL.PRG`) i weryfikuje deterministycznie:
- uruchomienie 1: kod wyjścia `TOS RC 0` oraz `TOSPROBE: ALL PASS` w konsoli,
- uruchomienie 2: znaczniki TOSCOL w 0x800..0x809 (`Setscreen` wrócił,
  `Setpalette` wrócił, rysowanie zakończone),
- koniec z PASS/FAIL i kodem wyjścia.
Warunki wstępne: obraz TOS=1 z KERNEL.SYS/SHELL.SYS (TOS=1) i obydwoma .PRG,
bez DESKTOP.APP (żeby shell zachował klawiaturę). Pełna macierz (duże/małe PRG,
TTP z argumentami, mono + oba tryby koloru, Setscreen(-1,-1,-1), zły nagłówek)
jest opisana w docstringu scenariusza do rozszerzenia.

## Stan
- Oba profile (TOS=0, TOS=1) budują się czysto; bramka hostowa (gmake check) zielona.
- **Format obrazu ujednolicony**: obraz testowy TOS=1 ma teraz `BIOS.CFG` z
  `--video-driver 2 --video-mode 3` (VOFA/ET4000 800x600x256 chunky), więc
  `graphics.mode=8`/`mode_bpp=8` i `kgfx_blit_planar` (kolorowe okno TOS) realnie
  działa — wcześniej mono (1 bpp) blokował konwerter koloru. Generowanie:
  `python3 tools/make_bios_cfg.py work/BIOS.CFG --video-driver 2 --video-mode 3`
  + instalacja do `\BIOS\BIOS.CFG` i root.
- **D2 (integracja WM) — plan.** Dziś `tos_draw_window_chrome()` rysuje ramkę
  bezpośrednio (`vid_fill`/`vid_text`), a klient idzie `kgfx_blit_planar` prosto
  do bufora prezentacji. Audyt: zarejestrować okno TOS jako normalny obiekt WM.
  Kroki: (1) prywatna powierzchnia klienta chunky (320x200x8, ok. 64 KB) — cel
  konwersji zamiast bufora prezentacji; (2) wpis do `wm.win[]` (`kernel_wm.c`,
  `wm_state_t`/`wm_window_t`: frame+client+pid+flags, `pid`=wskaźnik powierzchni
  jak dla `app_surface[]`); (3) `wm_recompose`+`glue_blit` blitują po `pid`, więc
  okluzja/Z-order zadziałają same. Ramkę rysuje WM (flaga `2`=bez dekoracji).
- **Timer C / IKBD aktywne podczas programu TOS** (SR `0x0700` → `0x0500`): przerwanie
  Timer C wywołuje `tos_composite_tick` (tanie: input + flaga), a natywna walidacja
  kontekstu użytkownika jest pomijana, gdy `kernel_tos_runtime_enabled` (persona TOS
  ma własne wektory/USP).
- **Odświeżanie przy każdym wywołaniu OS** — dispatchery trap #1/#13/#14 (`tos_entry.S`)
  wywołują `tos_composite_work` po każdym GEMDOS/BIOS/XBIOS (throttluje /5, bandy);
  **ostatnia klatka jest wymuszana** (`tos_composite_divider=4` przed wyjściem), więc
  nie odpada na dzielniku. To pokrywa programy robiące wywołania OS (np. animacje na
  `Vsync`); czysty spin bez wywołań OS nadal wymaga pełnego wywłaszczania (krok 2).
- **Test akceptacyjny TOS (dwa uruchomienia) — PASS.**
- Zero kodu TEMP (diagnostyka usunięta).
- Argumenty linii poleceń persony TOS przeplecione: `kernel_user_get_arguments()`
  zwraca `pending_arguments` (kernel_exception.c); shell resetuje je po uruchomieniu.
- Obraz testowy TOS=1: `work/atos-tos1.img` (KERNEL.SYS/SHELL.SYS TOS=1 + moduły +
  `SYSTEM/TEST/TOSPROBE.PRG` + `SYSTEM/TEST/TOSCOL.PRG`, bez DESKTOP.APP).
- `tosprobe.c` publikuje bitmaskę wyniku pod 0x810 (1=Sversion, 2=Malloc,
  4=Fopen, 8=Dgetdrv); 0 = wszystkie GEMDOS OK.
- `.PRG` relokacja: `mk_tosprg.py` (bajtowe delty, link od 0) + loader (`+ text`)
  — zgodne ze standardem GEMDOS/EmuTOS.

## Spójność układu rastra (ważne!)
`tos_putpixel`/`tos_getpixel` (emulacja Line-A/VDI) pisały stary układ „osobnych
bloków" (płaszczyzna na offsecie `p*32000`), a konwerter czyta przeplatany —
program przez VDI/Line-A rysowałby śmieci. Oba zunifikowane przez `st_raster`.
`tos_composite_now`/bandy/skalowanie mono już były zgodne (mono = 1 plan, byte =
przeplatane).

## Weryfikacja runtime
Uruchomiono test akceptacyjny (`scenario_tos_accept.py`) na obrazie TOS=1
(`work/atos-tos1.img`, bez DESKTOP.APP). **Wynik: PASS (oba uruchomienia, z
zaostrzonymi markerami START/DONE).**
- **TOSPROBE (uruchomienie 1) — PASS.** `START=0x5A5AA5A5`, `mask=0x0`,
  `DONE=0x900D`, `fopen=0` → program realnie wszedł, przeszedł wszystkie testy
  GEMDOS i dobiegł do końca (nie fałszywy PASS na niezainicjowanych zerach).
- **TOSCOL (uruchomienie 2) — PASS.** Znaczniki `01 AA 00 00 02 C0 CD DD EE 55`
  → Getrez → Setscreen → Setpalette → rysowanie → wyjście.

### Naprawione w tej turze (wg audytu, plus dwa dodatkowe)
1. **Relokacja PRG** — loader dodaje **TEXT base** (`mem+0x100`, wg EmuTOS
   `bdos/kpgmld.c`), testy linkowane **od 0** (`linker/tos-prg.ld`), generator
   bez błędu `delta=254` (`>254` zamiast `>=254`) i z 32-bitowym zerem przy braku
   relokacji.
2. **ABI GEMDOS** — Fsfirst **0x4E**, Fsnext **0x4F**, Frename **0x56** (+ argumenty
   `a`/`a+4`), Pexec **mode 0** ładuje+wykonuje, Sversion **`major<<8|minor`**
   (`0x00000206`), nieobsłużone → **EINVFN (-32)**.
3. **Test nie może już dać fałszywego PASS** — wymagane `START`+`DONE`+`mask==0`.
4. **`TOSPROBE` Fread bez argumentu `count`** — `gdos2(0x3f,h,buf)` podawało tylko
   2 argumenty, więc kernel czytał wskaźnik bufora ze złego slotu stosu (śmieci
   `0xffb20000`) → Bus Error w `copy_bytes` (FAT). Naprawiono: `gdos3(0x3f,h,16,buf)`.
5. **`TOSCOLOUR.PRG` → `TOSCOL.PRG`** — 9-znakowa nazwa nie mieściła się w 8.3 (FAT).
6. **Stale moduły na obrazie** — po zmianie kernela trzeba reinstalować też
   SHELL.SYS/moduły, inaczej shell nie dysponuje (launch pada).

## Testy
- `tests/test_st_raster.c` + `check-st-raster`: hostowy test (ASan) kontraktu
  przeplatanego rastra ST dla 320x200x4 / 640x200x2 / 640x400x1. Wpięty w `check`.
- `tests/test_tos_ext.c` + `check-tos-ext`: hostowy test (ASan) wspólnego matchera
  rozszerzeń TOS (PRG/TOS/TTP/GTP, wielkość liter). Wpięty w `check`.
- `tests/test_shell_fs.py`: doszły asercje haka `shell_tos_dispatch` i wspólnego
  matchera (razem 47 testów Pythona w `check`).

## Narzędzia
- `atos2-tools/probe_tos_state.py` — adresy z aktualnego .elf, pola uint8_t = 1 bajt.
- Uwaga: `gmake TOS=0` nadpisuje kernel/map; trzymać TOS=1 jako profil roboczy.
