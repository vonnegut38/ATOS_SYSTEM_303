# Audyt wydajności ATOS 2.1 — 2026-10-22

**Zakres:** cały system (WM, GFX/GUI, aplikacje, IPC, pulpit, dock, emulator).
**Tryb:** analiza źródeł + pomiary liczników jądra w Hatari. **Bez zmian w kodzie.**
**Środowisko pomiaru:** Hatari, Mega ST + TF536, 68030 32 MHz, 64 MB TT-RAM,
VOFA/ET4000, **800×600 8 bpp** (obraz testowy), 4 aplikacje: SYSINFO, TASKMGR,
PLASMA, MODPLAY; brak interakcji użytkownika w oknie pomiaru.

> Uwaga: 1024×768 ma **1,64× więcej pikseli** niż 800×600 — realny obraz
> użytkownika jest jeszcze cięższy niż ten test.

---

## 1. Wnioski w skrócie

1. **Największy koszt to pełne okno + pełny `kvideo_flush` do karty.**
   Aplikacje malują całe okno (TASKMGR, SYSINFO, PLASMA), WM dostaje damage
   całego klienta, a `kvideo_flush()` kopiuje całe uszkodzone wiersze do
   VOFA. Przy animacji PLASMA ~18 Hz to setki wierszy × 18/s. Brak
   dirty-rectów na poziomie aplikacji/powierzchni.
2. **Dock jest rysowany przy KAŻDEJ kompozycji** — także zamknięty — a do tego
   `dock_run_build()` skanuje okna/procesy. Dokłada stały koszt (bar 24
   wiersze + skan), ale **nie jest głównym** składnikiem 73%; to drugi cel.
3. **WM robi pełny chrome okien** przy każdym damage (komentarz w kodzie sam
   przyznaje, że clipping jest „na później”).
4. **GFX/GUI są jednowątkowe i synchroniczne** — każda partia rysowania to
   trap + odpowiedź; TASKMGR robi ~36 wywołań GUI na snapshot.
5. **Emulator** (32 MHz 68030, 8 bpp, VOFA) jest istotnym ograniczeniem;
   część „wolno” to koszt emulacji, nie kodu.

---

## 2. Pomiary

Licznik `kernel_user_hires_clock()`: 50 Hz × licznik 246 → **1 jednostka
≈ 81,3 µs**. `kdbg_total_cycles` obejmuje **cały tick WM** (kompozycję,
dekoracje, dock, kursor, `kvideo_flush`), ale **nie** rysowanie aplikacji
w GFX.

| faza | composes | dock_draws | total_units | recompose | deco | cursor | units/compose | czas WM |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| boot → dock open (2 s) | 1 | 1 | 472 | 40 | 0 | 8 | 472 | ~0,04 s |
| 4 apps start (ok. 10 s) | 33 | 34 | 47 587 | 12 102 | 6 552 | 348 | 1 442 | ~3,87 s |
| **4 apps, 5 s bez inputu** | **91** | **92** | **52 526** | **6 096** | **819** | **717** | **577** | **~4,27 s** |

**4 apps, 5 s bez inputu:**
- 91 kompozycji / 5 s = **~18 Hz** (PLASMA animuje; MODPLAY gra/ładuje),
- `total_cycles` 52 526 × 81,3 µs = **4,27 s** → **~85% jednego CPU w ticku WM**,
- recompose 6 096 (10%), dekoracje 819 (1,3%), kursor 717 (1,2%),
- **pozostałe ~45 000 jednostek (73%)** to przede wszystkim **`kvideo_flush`
  i zapisy do karty VOFA** dla pełnych okien (PLASMA), a także dock
  (`dock_draw`/`dock_run_build`), `dock_hover_poll`, hit-test myszy i
  księgowość WM. **Ten pomiar nie rozdziela flush od docka** — potrzeby jest
  licznik `kdbg_flush_cycles`; ale 73% po odjęciu recompose/deco/cursor to
  głównie koszt blitów do karty, nie sam pasek (24 wiersze z ~400).
- `dock_draws` = 92 przy 91 kompozycjach → **dock rysowany co kompozycję**.

To nie jest jeszcze rozbicie na poszczególne funkcje — brakuje liczników
`dock_cycles`/`flush_cycles`; ale skala i korelacja są jednoznaczne.

---

## 3. Miejsca w kodzie

### 3.1 Pełne okna i `kvideo_flush` — główny koszt
- `system/taskmgr.c`/`sysinfo.c`/`plasma.c`: `paint`/`frame` rysują całe
  okno; WM dostaje damage całego klienta.
- `system/kernel_video.c`: `kvideo_flush()` kopiuje **całe uszkodzone
  wiersze** do karty (`card_put16/32`); przy 8 bpp i pełnym oknie to setki
  wierszy na klatkę. To tutaj znika większość „73%”.
- `system/kernel_wm.c`: `wm_recompose()` blituje cały uszkodzony klient do
  shadow (recompose 6 096 jednostek = 10%), ale flush do karty jest poza
  tym licznikiem.

### 3.2 Dock — rysowany zawsze
- `dock_draw()` zawsze robi `dock_run_build()` i `kvideo_damage()` całego
  paska — nawet gdy pasek jest **zwinięty** (rysuje hamburger).
- `dock_hover_poll()` (wołany co tick) → `dock_hover_target()` →
  `dock_part_at()` → `dock_run_build()` **przy każdym ticku, gdy pasek jest
  otwarty** — nawet gdy mysz nie jest nad paskiem.
- `dock_run_build()` skanuje `wm.win[]` i wywołuje `kernel_user_pid_name()`
  (pętla po 12 slotach) dla każdej pozycji.
- Skutek: stały koszt 18–50 Hz niezależny od tego, czy coś się w pasku zmieniło;
  bar to 24 wiersze, więc udział w flushu jest mały, ale draw/build/scan
  dokładają się co kompozycję.

### 3.3 Aplikacje malują całe okno
- `system/taskmgr.c`: `paint()` rysuje całe 480×304; `flush()` wysyła cały
  `GFX_BATCH`; `GFX_CONTENT` całości.
- `system/sysinfo.c`: `paint()` rysuje całe okno przy każdej zmianie zakładki;
  `disk_view`/`devices_view` od nowa.
- `system/plasma.c`: `frame()` wypełnia **całe** okno (pasy), ~18 Hz.
- WM dostaje damage całego klienta → `wm_recompose()` blituje pełną
  powierzchnię (~0,5 MB przy 1024×768 8 bpp).

### 3.4 Kompozytor/dekoracje
- `kernel_wm_frame_tick()`: kompozycja tylko przy damage (dobrze), ale
  dekoracje rysują **cały chrome** okna przy każdym damage (`win_dirty`),
  bez przycinania do uszkodzonego fragmentu (komentarz: „left for a follow-up”).
- `kvideo_flush()` na końcu — plus osobne `kvideo_damage` dla paska.
- Kursor: hide/compose/show przy każdej kompozycji.

### 3.5 GFX/GUI i IPC
- Rysowanie idzie przez `GFX_BATCH` (dobrze — nie po jednym trapcie), ale
  usługa GFX jest **jednowątkowa**; każda partia = trap 42 + odpowiedź.
- `GUI` ma jedną kolejkę i `WFOC`/`WCLS`; TASKMGR `snapshot()` to ~36
  synchronicznych `gui_call` (12× info/name/res/cpu).
- Input cap 4 (po poprawkach) chroni odpowiedzi, ale motion nadal może
  powodować backpressure.

### 3.6 Emulator
- 68030 32 MHz, 8 bpp, VOFA ET4000, 1024×768 użytkownika; Hatari bez
  fast-forward. Pełnoekranowe blity i rysowanie tekstu są kosztowne.
- Test 800×600 jest **optymistyczny** względem 1024×768.

---

## 4. Propozycje (priorytet / ryzyko / zysk)

### P0 — Dirty-rect na poziomie aplikacji i powierzchni (średnie)
1. Nowy flush `GFX_CONTENT_RECT` / damage powierzchni: aplikacja zgłasza
   tylko zmieniony prostokąt.
2. TASKMGR: diff wierszy — rysuj tylko zmienione wiersze (CPU/state/RAM).
3. SYSINFO: zmiana zakładki = content rect; REFRESH tylko zmienione pola.
4. `kvideo_flush()`: flush tylko uszkodzonych **fragmentów wierszy** (już
   ma spans; ograniczyć pełne wiersze), zwłaszcza dla pełnego okna PLASMA.
5. WM: damage per rect, nie cały frame; blit tylko uszkodzonego fragmentu.
- **Zysk:** największy — usuwa większość 73% (zapisy do karty) i koszt
  pełnych powierzchni. **Ryzyko:** średnie — API, kilka aplikacji, testy
  golden/klików.

### P1 — Dock: rysować tylko gdy trzeba (niskie ryzyko)
1. **Brudny pasek**: `dock_dirty` ustawiany przy open/close, hover, zmianie
   listy uruchomionych, menu, zmianie ikon. `dock_draw()` tylko gdy
   `dock_dirty` **lub** damage kompozycji przecina prostokąt paska.
2. **Cache listy**: `dock_run_build()` tylko gdy zmienił się `wm.count`/
   stan okien (open/minimize/close) lub pasek otwarty; nie co kompozycję.
3. **Hover early-out**: jeśli mysz poza paskiem — zero `dock_part_at`/
   `dock_run_build`; chowaj tooltip bez skanowania.
4. **Damage tylko zmienionych slotów**, nie całego paska; hamburger tylko
   gdy stan się zmienił.
- **Zysk:** mniejszy niż P0 (bar to 24 wiersze), ale tanie i usuwa stały
  koszt draw/build/scan przy każdej kompozycji. **Ryzyko:** niskie.

### P1/P2 — Kompozytor: chrome i blity
1. Przycinać dekoracje do damage (zamiast pełnego chrome).
2. Pomijać okna, których damage nie dotyczy (już częściowo `win_dirty`).
3. Rozważyć bufor „back buffer”/blit tylko prostokątów; unikać pełnego
   `kvideo_flush` poza damage.
- **Zysk:** 10–20% ticku WM + mniejsze flushe. **Ryzyko:** średnie
  (artefakty ramek — testy golden/klików).

### P2 — PLASMA i animacje
1. Limit FPS (np. 10–12 Hz) lub adaptacyjny (klatka co N ticków).
2. Rysować tylko zmienione pasy (jeśli animacja jest przesunięciem fazy).
3. Zmniejszyć okno/rozdzielczość animacji.
- **Zysk:** mniejsze obciążenie GFX i blitów. **Ryzyko:** niskie.

### P2 — GFX/tekst
1. Profil `kgfx_text`/`wm_fill_rect` (najczęstsze); cache glifów.
2. Upewnić się, że `GFX_BATCH` nie powtarza pełnego `GFX_CONTENT` po każdej
   partii; łączyć.
3. Dodać liczniki `gfx_batch_cycles`, `gfx_commands`.
- **Zysk:** zależny od profilu; prawdopodobnie 10–30% rysowania.

### P3 — TASKMGR/IPC
1. Jeden syscall „bulk process info” zamiast 4×12 (info/name/res/cpu).
2. Nie odświeżać całej listy przy WFOC, jeśli nic się nie zmieniło.
3. Utrzymać brak auto-refresh (albo timer 1 s tylko gdy okno aktywne).
- **Zysk:** mniejszy koszt snapshotu; **ryzyko:** niskie.

### P3 — Emulator (poza kodem systemu)
1. Zmierzyć 800×600 vs 1024×768, 4/8 bpp, `--frameskips`.
2. Sprawdzić, czy wąskim gardłem jest CPU emulacji czy blit VOFA.
- **Zysk:** może być większy niż optymalizacje kodu; **ryzyko:** kompromis
  jakości.

---

## 5. Kolejność wdrożenia

1. **Dirty-rect API + TASKMGR/SYSINFO + flush fragmentów** (P0) —
   największy efekt; dodać `kdbg_flush_cycles`.
2. **Dock dirty/intersection + cache** (P1) — tanie, usuwa stały koszt.
3. **Chrome clipping / kompozytor** (P1/P2).
4. **PLASMA FPS** (P2).
5. **GFX/tekst i bulk IPC** (P2/P3).
6. **Emulator** — równolegle, jako punkt odniesienia.

---

## 6. Plan pomiaru przed/po

- Liczniki: `kdbg_total_cycles`, `kdbg_recompose_cycles`, `kdbg_deco_cycles`,
  `kdbg_cursor_cycles`, `kdbg_dock_draws`, `kdbg_wm_composes`,
  `kdbg_frames` (jednostka 81,3 µs).
- Dodać: `kdbg_dock_cycles`, `kdbg_flush_cycles`, `kdbg_gfx_batch_cycles`,
  `kdbg_gfx_commands`.
- Scenariusze: 0/1/3/5 aplikacji; dock otwarty/zamknięty; PLASMA on/off;
  800×600 vs 1024×768; bez ruchu myszy i z ruchem.
- Cel: **< 30% CPU WM** przy animacji tła i **< 50 ms** na interakcję
  (klik/tab) na 1024×768 w emulatorze.

---

## 7. Ograniczenia audytu

- Pomiary w Hatari, nie na prawdziwym 68030; wartości relative.
- `kdbg_total_cycles` obejmuje też `kvideo_flush`/zapisy do karty, ale
  **nie** rysowanie aplikacji przez GFX (osobny koszt). 85% to udział WM
  w ticku, nie całego systemu; rzeczywisty udział może być inny.
- Test 800×600; 1024×768 = 1,64× pikseli.
- Brak profilu per-funkcja (dock/flush/GFX); to pierwszy krok, nie pełny
  profiler.
