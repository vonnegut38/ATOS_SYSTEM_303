# PLASMA — efekt demoscenowy (2026-10-22)

## Co jest teraz

- **320×200, 8 bpp**, pełna rozdzielczość (bez klocków 10×10).
- Klasyczny wzór interferencyjny: fale X/Y + pierścienie radialne,
  policzony **raz** do bufora `base` przy starcie.
- Wzór rysowany **raz** (`ATOS_CALL_BLIT8`); animacja to wyłącznie
  **16 wpisów DAC 236..251** (prywatny zakres TOS), obracane kołem barw.
  Zero kopiowania pikseli na klatkę, zero damage WM przy animacji.
- Rampa kolorów z **palety systemowej 0..19** (czerń → granat → blue →
  jasny blue → szarość → biel → teal), więc **nie zmieniamy globalnej
  palety** i nie recolorem ikon innych okien.
- ~40–50 fps (`FRAME_COUNTS 246`); pomiar: **40,3 fps**.
  WVIS 0/1 wstrzymuje animację i przywraca paletę.
- Bufor wzoru (64 KB) w **heapie aplikacji** (`ATOS_CALL_ALLOC`), zwalniany
  po pierwszym blicie; aplikacja PC-relative mieści się w ±32 KB.
- Zakres 236..251 to paleta klienta TOS; gdy równolegle działa program TOS,
  jego kolory będą animowane razem z PLASMA (do dopracowania, jeśli przeszkadza).

## Nowe ABI

`ATOS_CALL_BLIT8 = 89`:
`d1=w d2=h a0=uint8_t[w*h]` → 8bpp do własnej powierzchni okna (0,0).
Jądro waliduje zakres użytkownika, kopiuje wiersze i zgłasza damage
przez `kernel_wm_app_dirty()` (z clip/WVIS).

## Pliki

- `system/plasma.c` — nowy efekt,
- `include/app_abi.h` — BLIT8 (89), PALETTE (90),
- `system/kernel_wm.c` — `kernel_wm_app_blit8()`,
- `system/kernel_exception.c` — syscall 89/90.

## Testy

- `test-dock-running` — PASS (start, minimalizacja, restore PLASMA),
- golden — PASS (bez zmian startu),
- zrzut: `display-demo/dockrun-right-restored.png`.

Build: `KERNEL.SYS` **234 328 B** (TOS=1) / 180 762 B (TOS=0),
`PLASMA.APP` **2 116 B**.


## Optymalizacja 2026-10-22 (wydajność)

Wcześniej: pełny blit 320×200 + WM + flush na każdą klatkę → **8,4 fps**.
Teraz: wzór statyczny, animowana tylko paleta 16 wpisów → **40,3 fps**
(`kdbg_palette_calls`, 5 s). Brak per-frame BLIT8/damage WM, więc okna pod
PLASMA nie są co klatkę przemalowywane.
