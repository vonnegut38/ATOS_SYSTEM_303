# PLASMA — efekt demoscenowy (2026-10-22)

## Co jest teraz

- **320×200, 8 bpp**, pełna rozdzielczość (bez klocków 10×10).
- Klasyczny wzór interferencyjny: fale X/Y + pierścienie radialne,
  policzony **raz** do bufora `base` przy starcie.
- Każda klatka: `base[i] + phase` → 256-entry paleta → bufor `buf`
  i **jeden blit** do powierzchni okna.
- Rampa kolorów z **palety systemowej 0..19** (czerń → granat → blue →
  jasny blue → szarość → biel → teal), więc **nie zmieniamy globalnej
  palety** i nie recolorem ikon innych okien.
- ~25 fps (`FRAME_COUNTS 492`), animacja phase; WVIS 0/1 wstrzymuje/
  wznawia rysowanie.
- Bufor `base` i `buf` (2×64 KB) w **heapie aplikacji** (`ATOS_CALL_ALLOC`,
  TT-RAM), nie w BSS — aplikacja PC-relative mieści się w ±32 KB.

## Nowe ABI

`ATOS_CALL_BLIT8 = 89`:
`d1=w d2=h a0=uint8_t[w*h]` → 8bpp do własnej powierzchni okna (0,0).
Jądro waliduje zakres użytkownika, kopiuje wiersze i zgłasza damage
przez `kernel_wm_app_dirty()` (z clip/WVIS).

## Pliki

- `system/plasma.c` — nowy efekt,
- `include/app_abi.h` — BLIT8,
- `system/kernel_wm.c` — `kernel_wm_app_blit8()`,
- `system/kernel_exception.c` — syscall 89.

## Testy

- `test-dock-running` — PASS (start, minimalizacja, restore PLASMA),
- golden — PASS (bez zmian startu),
- zrzut: `display-demo/dockrun-right-restored.png`.

Build: `KERNEL.SYS` **234 148 B** (TOS=1) / 180 582 B (TOS=0),
`PLASMA.APP` **2 114 B**.
