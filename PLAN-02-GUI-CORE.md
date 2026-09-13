# PLAN-02 — Rdzeń GUI

Cel: graficzne środowisko zgodne z TOS/EmuTOS, ale aplikacje działają
wielozadaniowo we własnych oknach; nowoczesny wygląd i szybkość.

## G1 — menedżer N okien — ZROBIONE

- `include/mono_wm.h`, `system/mono_wm.c`: WM_MAX=8, kolejność Z (`order[]`),
  fokus, `wm_open/wm_select/wm_cycle/wm_close/wm_hit/wm_motion/wm_top/
  wm_next_open`. Model czysty, bez zależności od jądra.
- `tests/test_mono_wm.c`: test hostowy (Z-order, fokus, cykl, pełny ekran,
  zamykanie, pojemność, 10000 iteracji drag/resize) — PASS.
- `system/mono_desktop.c`: desktop rysuje wszystkie okna w kolejności Z,
  pasek zadań dla N okien, trzecie okno wewnętrzne SYSTEM.
- `system/kernel_main.c`: `MONODESK.APP` jako czwarta usługa startowa
  (`SYSTEM.WM`) — auto-start po INIT/GFX/GUI.
- Demo: `atari-030-port/gui-core-nwindows.png` (SYSTEM, LICZNIK, NOTATKA).


## G2 — pasek menu EmuTOS + TASKS — ZROBIONE

- **Menu bar** wziety 1:1 z EmuTOS (`desk/desk_rsc.c`):
  `Desk` (Desktop info...), `File` (Open, Info/Rename..., Search...,
  New folder..., Close folder, Close top window, Cycle windows,
  Select all items, Set file mask..., Delete..., Format floppy...,
  Execute EmuCON, Shutdown), `View` (Show as icons/text, Sort by
  name/type/size/date, No sort, Size to fit), `Options` (Set background...,
  Install icon/application/devices, Desktop configuration...).
- **Ikonka TASKS** w prawym gornym rogu: rozwija liste uruchomionych
  programow (`1 SYSTEM / 2 LICZNIK / 3 NOTATKA *`), klikniecie przelacza.
- **Mysz**: obsluga w desktopie (klik w tytul menu, wybor pozycji, TASKS,
  przeciaganie okien, przycisk zamykania, zmiana rozmiaru). Klawiatura:
  F1-F4 menu, F5 TASKS, strzalki + Enter, Esc.
- **Naprawiony blad**: `static const` tablice **wskaznikow** w `.rodata`
  nie przezywaly zaladowania (aplikacja PC-relative) — `menus[].title` i
  `items[].text` byly smieciami. Dane menu przepisane bez wskaznikow
  (indeksy + literaly w kodzie).
- Zwiekszony budzet rysowania `GUI_DRAW_MAX` 32 -> 96 (menu + dropdown +
  okna nie miescily sie w 32).
- Zrzuty: `atari-030-port/gui-menu-emutos.png`, `gui-tasks-list.png`.

### Uwaga o myszy w Hatari

Mysz w GUI dziala dopiero, gdy emulator przechwytuje hosta: **`--grab`**
(albo AltGr+M w oknie). Bez tego ruch hosta nie trafia do emulowanego ST.
Sciezka w systemie jest poprawna: GUI service op 6 -> syscall 47 (trwale
przechwycenie) -> KMOUSE do desktopu -> `mouse()`.

## Ważne ustalenie o ścieżce obrazu

- GUI jądra rysuje do **ekranu ST** (`SCREEN_BASE=0x10000` /
  `desktop_framebuffer`, Shifter), a konsola BIOS/kernel pisze do **VRAM
  karty ET4000** (`video8`, `0xC00000`).
- Dlatego na karcie widać konsolę, a desktop widać na Shifterze
  (demo zrobione bez karty).
- Do decyzji: albo VOFA ma przełączać się na obraz ST (pass-through), albo
  GUI ma renderować do VRAM karty. To warunek „ładnego GUI na ET4000”.

## Kolejne kroki

- **G2**: nowoczesny motyw (cienie, zaokrąglenia, paleta kolorów) w
  `kgfx_emutos_window` / `kgfx_emutos_background`,
- **G3**: menu, okna dialogowe, pasek zadań z zegarem,
- **G4**: aplikacje TOS w oknach (GEMDOS/VDI/AES w oknie),
- **G5**: ikony pulpitu i menedżer plików.