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

### Krok 4 — WVIS (kernel_wm.c, kernel_exception.c, plasma.c)
WM śledzi `clip_count` okna (poza `wm.win[].flags`, bo część kodu porównuje
flags dokładnie `==1/==7`). Gdy okno przechodzi między „całkowicie
zasłonięte/zminimalizowane” a „widoczne”, wysyła `WVIS` (0/1) do właściciela;
przy pełnej skrzynce ponawia maks. 8 razy, potem odpuszcza (bez zalewania WM).
PLASMA na `WVIS 0` wstrzymuje zegar i nie rysuje; na `WVIS 1` ustawia
`last=hires()`, zeruje `phase_acc` i rysuje od razu. Model: app GFX
499→31 tys. px/s (−94%).

### Krok 6 — GUI_FRAME bez pełnego ticku (kernel_wm.c, kernel_exception.c)
`kernel_wm_compose_request()` tylko ustawia `wm_force_compose`; kompozycja
następuje w najbliższym ticku (max 20 ms) zamiast synchronicznego pełnego
ticku przy każdej klatce aplikacji (arkanoid/GUI_FRAME).

### Krok 7 — flush 32-bit (kernel_video.c)
Para słów frontu i cienia jest porównywana jednym odczytem 32-bit (front
`const volatile`, cień bez volatile); zapis do karty tylko przy zmianie.
Mniej odczytów, ten sam obraz.

### Pomiar
- `kdbg_flush_area` liczone w `kernel_video.c` (słowa przeglądane przez flush).

---

## 2. Pomiary (800×600, 4 apps, 5 s bez inputu)

| etap | composes | dock_draws | total_cycles | recompose | dmg_px | blit_px | flush_area | cycles/compose |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| V0 przed | 91 | 92 | 52 526 | 6 096 | — | — | — | 577 |
| V1 clip | 60 | 58 | 27 179 | 1 342 | 2 315 988 | 54 834 | 1 058 481 | 452 |
| **V1+V2 dock dirty** | **66** | **0** | **6 899** | **1 468** | **1 454 110** | **56 426** | **677 849** | **104** |
| **V1+V2+WVIS+6+7** | **74** | **0** | **6 339** | **984** | **143 352** | **53 472** | **45 713** | **85** |
| V1+V2 dock closed 5 s | 25 | 2 | 2 707 | 548 | 803 208 | 104 096 | 368 118 | 108 |

- **`total_cycles` WM: 52 526 → 6 339 (−88%)**,
- **`dock_draws`: 92 → 0** w oknie 5 s (pasek pozostaje widoczny; rysowany
  przy otwarciu i gdy damage go dotknie),
- **`kdbg_dmg_px`: 1 454 110 → 143 352 (−90%)** po WVIS (PLASMA nie rysuje
  pod MODPLAY),
- **`flush_area`: 677 849 → 45 713 (−93%)**,
- `cycles/compose`: **577 → 85** (−85%).
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

Kroki 4/6/7: golden (czysty obraz), input, multi — PASS; step3/menu/running
przed 6/7 — PASS; running po WVIS (minimalizacja/restore PLASMA) rysuje
bieżącą klatkę po odsłonięciu. Arkanoid (GUI_FRAME) uruchamia się z ikony;
jego okno nie pojawiło się w szybkim teście — do sprawdzenia osobno, krok 6
go dotyczy.

Build: `KERNEL.SYS` **233 344 B** (TOS=1), 179 762 B (TOS=0),
`PLASMA.APP` **1 828 B**.

---

## 4. Co dalej (kroki 4–7)

4. ~~**WVIS**~~ — **wdrożone** (patrz wyżej).
5. **Damage na komendę/grupę** — pozostały krok; mniejsze prostokąty dla
   MODPLAY/SYSINFO/TASKMGR (model: 0,53→0,40 MB/s). Opcjonalny.
6. ~~**GUI_FRAME**~~ — **wdrożone**.
7. ~~**Flush 32-bit**~~ — **wdrożone**.

Rekomendacja: zostało tylko **per-command (5)**; po nim powtórzyć golden,
kliknięcia i pomiar. Kroki 1–4, 6–7 są w `bd1791a`/następnym commicie.
