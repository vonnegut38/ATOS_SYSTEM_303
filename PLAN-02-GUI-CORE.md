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