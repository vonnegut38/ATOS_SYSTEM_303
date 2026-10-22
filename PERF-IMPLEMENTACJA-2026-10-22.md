# ATOS 2.1 — wdrożenie kroków 1–3 planu (2026-10-22)

**Zakres:** krok 1 (clip damage do `wm.clip`), krok 2 (dock tylko przy
zmianie/przecięciu), krok 3 (PLASMA bez identycznych klatek).
**Plik modelu:** `perf-model-20261022/` (uruchamiany `sh run.sh`).

---

## 1. Co zmieniono

### Krok 1 — `kernel_wm_app_dirty()` (kernel_wm.c)
Damage aplikacji jest teraz:
- odrzucany dla okna niewidocznego (`flags&1==0`) lub zminimalizowanego
  (`flags&8`),
- przycinany do `wm.clip[i]` — faktycznie widocznej części okna,
- liczony z uwzględnieniem przewinięcia `src_x/src_y` (F7).

TOS (`tos_prg.c`) korzysta z tej samej funkcji, więc obejmuje go bez zmian.

### Krok 2 — dock (kernel_wm.c)
- `dock_dirty` + sygnatura widocznych okien (`dock_state_sig`) — pasek
  przerysowywany tylko przy zmianie stanu,
- `dock_damage_hit()` — pasek przerysowywany, gdy damage kompozycji przecina
  jego prostokąt (okno najechało/odsłoniło pasek),
- `dock_run_build()` tylko przy rysowaniu lub trafieniu myszą w prawą stronę;
  `dock_part_at()` ma early-out poza paskiem,
- hover/menu/open/close ustawiają `dock_dirty`.

### Krok 3 — PLASMA (plasma.c)
`frame()` wywoływane tylko, gdy faza naprawdę się zmieniła; przy 50 Hz
identyczny obraz nie wysyła partii 46 800 px.

### Pomiar
- `kdbg_flush_area` liczone w `kernel_video.c` (słowa przeglądane przez flush).

---

## 2. Pomiary (800×600, 4 apps, 5 s bez inputu)

| etap | composes | dock_draws | total_cycles | recompose | dmg_px | blit_px | flush_area | cycles/compose |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| V0 przed | 91 | 92 | 52 526 | 6 096 | — | — | — | 577 |
| V1 clip | 60 | 58 | 27 179 | 1 342 | 2 315 988 | 54 834 | 1 058 481 | 452 |
| **V1+V2 dock dirty** | **66** | **0** | **6 899** | **1 468** | **1 454 110** | **56 426** | **677 849** | **104** |
| V1+V2 dock closed 5 s | 25 | 2 | 2 707 | 548 | 803 208 | 104 096 | 368 118 | 108 |

- **`total_cycles` WM: 52 526 → 6 899 (−87%)**,
- **`dock_draws`: 92 → 0** w oknie 5 s (pasek pozostaje widoczny; rysowany
  przy otwarciu i gdy damage go dotknie),
- **`kdbg_dmg_px`: 2,32 M → 1,45 M** (delta V1→V2 w oknie 5 s),
- **`flush_area`: 1,06 M → 0,68 M** (delta V1→V2),
- `cycles/compose`: **577 → 104**.
- Model V1+V2 dawał 6,31→0,53 MB/s; zmierzony spadek ticku WM jest zgodny
  kierunkowo (model liczy wolumen, nie czas 68030).

Krok 3 nie zmienia WM (PLASMA pod MODPLAY i tak ma clip 0), ale w modelu
app GFX spada 873→499 tys. px/s; krok 4 (WVIS) ma zejść do ~31 tys.

---

## 3. Testy

- `test-wall-custom.py golden` → **PASS**, REF `15424d43…` bez zmian.
- `test-dock-step3.py` → PASS (open/close/hover/start).
- `test-dock-menu.py` → PASS.
- `test-dock-running.py` → PASS (badges/restore), także po kroku 3.
- `test-dock-input.py` → PASS (taby, WCLS, `launch 4→5`).
- `test-dock-multi.py` → PASS (5 aplikacji).
- Zrzut `perf-4apps.png`: pasek widoczny, mimo `dock_draws=0` po ustabilizowaniu.

Build: `KERNEL.SYS` 232 884 B (TOS=1), 179 298 B (TOS=0), `PLASMA.APP` 1 760 B.

---

## 4. Co dalej (kroki 4–7)

4. **WVIS** — WM powiadamia aplikację o `clip_count 0↔>0`; animacje
   (PLASMA) wstrzymują zegar, po odsłonięciu rysują od razu. Model: app GFX
   499→31 tys. px/s. Ryzyko średnie (nowy komunikat ABI).
5. **Damage na komendę/grupę** — mniejsze prostokąty dla MODPLAY/SYSINFO/
   TASKMGR; w modelu WM 0,53→0,40 MB/s.
6. **GUI_FRAME** — damage + `wm_force_compose` zamiast pełnego ticku przy
   każdej klatce; ważne przy kilku widocznych animacjach.
7. **Flush 32-bit** bez `volatile` po stronie front/cień + `kdbg_flush_area`;
   po krokach 1–4.

Rekomendacja: najpierw **WVIS** (krok 4), potem **per-command** (5), a 6–7
na końcu; po każdym kroku powtórzyć golden + kliknięcia + pomiar.
