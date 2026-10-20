# ATOS 2.0 — Desktop, ETAP 1: Fundament wydajności grafiki

Dokument podsumowuje ETAP 1 planu Desktop (1–12). Zakres etapu: **przegląd
istniejącej grafiki, benchmark, pomiar wąskich gardeł, uporządkowanie `gfx_*`,
weryfikacja klipingu i regionów damage oraz osobny test Shifter/ET4000**.
Zgodnie z ustaleniami nie przepisano działającego kodu, nie dodano ikon,
menedżera plików ani AES/VDI i nie zmieniono SHELL-a ani BIOS-u.

---

## 1. Inwentaryzacja: trzy równoległe ścieżki graficzne

W drzewie istnieją dziś **trzy** niezależne ścieżki rysowania. To jest główny
powód, dla którego „uporządkowanie `gfx_*`" jest osobnym zadaniem.

| # | Ścieżka | Pliki | Kto używa | Rysowanie |
|---|---------|-------|-----------|-----------|
| A | Kompozytor planarny (EMUTOS VDI) | `system/kernel_graphics.c`, `system/emutos_vdi_mono.c`, `system/kernel_blitter.c` | konsola tekstowa, `SYSTEM.GFX` (opcode 2–14), `kgfx_draw_batch` | `surface_fill` → `emutos_vdi_planar_fill`; fill idzie też przez **blitter** (`kblit_fill_planar`); line/copy po stronie CPU |
| B | Menedżer okien (chunky + planarny) | `system/kernel_wm.c`, `system/wm_core.c`, `system/wm_blit.c` | tryb desktop (`kernel_wm_frame_tick`, 1 kompozycja na tick 50 Hz) | `wm_fill_rect`/`wm_copy_rect` — własne pętle CPU; clip + damage w `wm_core.c` |
| C | Abstrakcja `video_driver_t` | `include/video_driver.h`, `system/video_driver.c`, `video/shifter.c`, `video/et4000.c` | **tylko testy hosta** | `fill_rect`/`draw_line`/`blit` **per-pixel**; w runtime używany wyłącznie jako deskryptor powierzchni (`video_screen_attach`/`video_screen_surface` w `kernel_wm.c`) |

Wniosek: warstwa C **nie jest** ścieżką rysowania. Jej `fill_rect`/`blit`
nie są wywoływane przez kompozytor — kompozytor czyta z niej jedynie
`video_surface_t`. To jest miejsce, w którym docelowa architektura
(`Desktop → WM → Graphics API → Compositor → Video Driver`) jeszcze nie
domyka się z rzeczywistością.

### 1.1 Wyjście: Shifter vs VOFA/ET4000

- **Shifter**: `SCREEN_BASE` (ST-RAM), planar 1/2/4bpp, brak kopii.
- **VOFA/ET4000**: `system/kernel_video.c` — front to `desktop_framebuffer`
  w ST-RAM, a `kvideo_flush()` kopiuje **bajt po bajcie** do VRAM karty,
  śledząc zabrudzone linie (`kvideo_damage`). Aktywacja:
  `kvideo_activate_vofa(VOFA_MEM)` (`kernel_video_boot.c`).

Realny stan ET4000/W32i: **brak akceleracji sprzętowej** (BitBLT, wypełnianie,
kursor sprzętowy) — `video/et4000.c` to pętla per-pixel, a `vofa_et4000.c`
programuje tylko rejestry trybu.

Znaleziony problem wydajnościowy ET4000: `kernel_wm_frame_tick()` na końcu
każdej skomponowanej klatki woła `kvideo_damage(0,0,640,400)` i
`kvideo_flush()` (`kernel_wm.c:290-291`), mimo wcześniejszego per-rect
`kvideo_damage`. Czyli **przy aktywnym WM kopia na kartę jest zawsze
pełnoekranowa**, a nie przyrostowa.

---

## 2. Benchmark `GFXBENCH.APP`

Pliki:

- `system/gfx_bench.c` — aplikacja (C, ABI `gui_app_runtime`),
- `atos2-tools/scenario_gfx_bench.py` — uruchomienie w **okienkowym** Hatari,
  wpisanie `run gfxbench.app` + ENTER, odczyt konsoli i zrzut ekranu,
- wynik instalowany jako `\SYSTEM\APP\GFXBENCH.APP` (`tools/build_atos2.py`),
- zrzuty: `atos2-tools/shots/gfxbench-shifter.png`, `gfxbench-et4000.png`.

### 2.1 Metodologia

- Każdy prymityw jest wołany **pełną ścieżką publiczną**: `send` (syscall 36)
  → broker `SYSTEM.GFX` → syscall 42 → `kgfx_*` → `receive` (syscall 38).
  To odzwierciedla koszt, jaki poniesie menedżer okien na jedno wywołanie.
- Budżet: **50 ticków = 1,0 s** czasu gościa na prymityw; licznik = liczba
  pełnych żądań.
- Zegar: **syscall 58** (`kernel_user_clock`, poll Timera C bez przełączenia
  zadania). Ścieżka `ticks()`/syscall 10 to `kernel_user_yield` i kosztuje
  ~2 ms na wywołanie — użycie jej zafałszowałoby wszystkie wyniki.
- Wiersz **POLL** mierzy sam koszt syscallu 58; w tabelach podano też `net`
  (czas minus POLL).
- **Ograniczenie**: Timer C jest **pollingowany** (`timer_poll` dolicza
  najwyżej jedno przepełnienie na wywołanie), więc pomiar ma zauważalny rozrzut
  (±50% między przebiegami). Benchmark wiarygodnie rozdziela „zdominowane przez
  dispatch" od „zdominowane przez rasteryzację", ale nie różnicuje różnic
  sub-milisekundowych. Dla ETAPU 2 potrzebny jest licznik cykli w kernelu
  i/lub API wsadowe.

### 2.2 Wyniki — Shifter (68030 @16 MHz, STE, 640×400 mono)

| Prymityw | iter | ticki | µs/żądanie | net µs |
|----------|-----:|------:|-----------:|-------:|
| POLL (syscall 58) | 2591 | 50 | 386 | — |
| CALL pixel(0,0) | 290 | 50 | 3448 | 3062 |
| PIXEL (300,200) | 254 | 51 | 4016 | 3630 |
| HLINE (640 px) | 278 | 50 | 3597 | 3211 |
| VLINE (400 px) | 200 | 50 | 5000 | 4614 |
| FILL32 (32×32) | 277 | 50 | 3610 | 3224 |
| FILL128 (128×128) | 238 | 50 | 4202 | 3816 |
| FRAME (200×100) | 186 | 50 | 5376 | 4990 |
| TEXT1 (1 glif) | 225 | 50 | 4444 | 4059 |
| COPYMONO (320×64, wyr.) | 117 | 50 | 8547 | 8161 |
| COPYSHFT (300×64, przes.) | 440 | 50 | 2273 | 1887 |
| FILLHALF (640×200) | 131 | 50 | 7634 | 7248 |
| CLEAR (640×400) | 116 | 50 | 8621 | 8235 |

### 2.3 Wyniki — profil ET4000 (TF536 68030 @32 MHz, MegaSTE, VOFA ET4000)

Profil uruchamiany przez `atari-030-port/hatari-tf536` z
`--graphics-card vofa --gfx-chip et4000 --ttram 64 --tf536 on`.
Uwaga: to **szybsza maszyna** (32 MHz), więc liczby są ~2× mniejsze; nie jest
to porównanie „ta sama maszyna, inne wyjście".

| Prymityw | iter | ticki | µs/żądanie | net µs |
|----------|-----:|------:|-----------:|-------:|
| POLL | 5287 | 50 | 189 | — |
| CALL | 596 | 50 | 1678 | 1489 |
| PIXEL | 577 | 50 | 1733 | 1544 |
| HLINE | 537 | 50 | 1862 | 1673 |
| VLINE | 251 | 50 | 3984 | 3795 |
| FILL32 | 515 | 50 | 1942 | 1753 |
| FILL128 | 379 | 50 | 2639 | 2449 |
| FRAME | 281 | 50 | 3559 | 3370 |
| TEXT1 | 485 | 50 | 2062 | 1873 |
| COPYMONO | 224 | 50 | 4464 | 4275 |
| COPYSHFT | 759 | 50 | 1318 | 1128 |
| FILLHALF | 213 | 50 | 4695 | 4506 |
| CLEAR | 163 | 50 | 6135 | 5946 |

### 2.4 Wnioski liczbowe

1. **Stały koszt żądania to kilka milisekund** (Shifter ~3,0 ms net dla
   `CALL`). Przy 50 Hz (20 ms/klatka) daje to **~6 żądań na klatkę** na
   16 MHz i **~13 na klatkę** na 32 MHz. To jest wąskie gardło numer jeden.
2. **Rasteryzacja jest tania w porównaniu z dispatch-em.** Różnica
   `FILLHALF`−`CALL` to ~4,2 ms na 128 000 px, czyli ~33 ns/px
   (w trybie mono 1 word na 16 px). Wypełnienia korzystają z blittera
   (`kblit_fill_planar`, CAPS=61 = PRESENT|ENABLED).
3. **Anomalie wymagające profilowania w kernelu** (powtarzalne w obu profilach):
   - `COPYMONO` (ścieżka wyrównana, `emutos_vdi_mono_copy_words`) jest
     **~4× wolniejsza** niż `COPYSHFT` (ścieżka przesunięta), mimo prostszej
     pracy. „Szybka ścieżka" jest wolniejsza od „wolnej".
   - `VLINE` (400 px) jest droższy niż `HLINE` (640 px);
     `emutos_vdi_vertical` powinien być tańszy.
   - `COPYSHFT` net (1887 µs) < `CALL` net (3062 µs) — niemożliwe dla
     prawdziwej pracy; potwierdza szum schedulera.
4. **ET4000**: koszt prymitywów jest ten sam co na Shifterze (liczy CPU, nie
   kartę), bo ścieżka `kgfx_*` **nie robi flusha do VRAM** — na kartę trafia
   dopiero prezentacja konsoli/WM. Realny narzut ET4000 to `kvideo_flush`.

---

## 3. Kliping i regiony damage — audyt

Sprawdzone i **działające** na poziomie kompozytora:

- `kernel_graphics.c`: `clip_rect` (57), `clip_copy_rect` (73),
  `clip_line`, `mark_dirty` (91) z `KGFX_DIRTY_MAX=16` i bezpiecznym
  przepełnieniem → pełny ekran.
- `wm_core.c`: `wm_rebuild_clip` (51), lista `clip[WM_MAX][WM_CLIP_MAX]`,
  `wm_damage_add` (185); `kernel_wm_frame_tick` woła `kvideo_damage`
  na każdy region.
- Testy: `check-gui-damage` („clipping, text newlines, conservative windows,
  word copy for 1/2/4 planes and edge guards"), `check-wm-core`,
  `check-wm-blit`, `check-wm` — **wszystkie przechodzą**.

Luki w warstwie `video_driver_t` (dziś nieużywanej do rysowania):

- `sh_fill_rect` **nie klipuje wcale** (brak clamp dla ujemnych x/y —
  pisze poza bufor);
- `sw_fill_rect`/`et_fill_rect` klipują tylko do prawej/dolnej krawędzi;
- brak `set_clip`, brak masked-blit, brak `copy_rect`/scroll, brak
  `draw_char/text`, brak `screen_info`, brak kursora jako bitmapy.

Wniosek: kliping i damage są poprawne tam, gdzie dziś rysuje kernel; warstwa
sterowników wymaga uzupełnienia, zanim stanie się jedyną ścieżką.

---

## 4. Organizacja `gfx_*` — rekomendacje (bez przepisywania)

1. **Ustalić jedną ścieżkę rysowania**: kompozytor A (planarny) ma pozostać
   producentem dla konsoli; B (WM) ma być jedynym kompozytorem okien; C
   (`video_driver_t`) ma stać się **tylko** warstwą wyjścia (surface +
   flush), a nie drugim zestawem prymitywów.
2. **Usunąć martwy kod rysujący** z C albo oznaczyć go jasno jako testowy;
   w przeciwnym razie powstanie czwarta ścieżka.
3. **Rozszerzyć `video_driver_t`** o: `set_clip`, `copy_rect`,
   `masked_blit`, `fill_rect` z pełnym klipem, `screen_info`.
4. **Wprowadzić API wsadowe** jako domyślną drogę dla WM (jedno żądanie = lista
   operacji), żeby zdjąć koszt dispatch-u z każdego prymitywu.
5. **ET4000**: zrobić flush przyrostowy w WM (usunąć pełnoekranowy
   `kvideo_damage(0,0,...)` z `kernel_wm.c:290`); w dalszej kolejności
   użyć akceleracji W32i.

---

## 5. Czego nie zmieniono

- Brak zmian w SHELL, BIOS, AES/VDI, brak ikon i menedżera plików.
- Nie przepisano istniejących prymitywów `kgfx_*` ani `wm_*`.
- Dodano wyłącznie: aplikację benchmarku, wpis w `Makefile`, instalację w
  obrazie i scenariusz harness-u.

## 6. Następne kroki (ETAP 2)

1. Licznik cykli w kernelu (albo licznik w `kgfx_get_info`) do precyzyjnych
   pomiarów bez pollingu Timera C.
2. Profilowanie anomalii `COPYMONO` / `VLINE` na poziomie `emutos_vdi_*`.
3. Przyrostowy flush ET4000 + pomiar `kvideo_flush`.
4. API wsadowe dla WM i pomiar liczby żądań na klatkę.
5. Dopiero potem pierwsze okna/ikony na bazie ustalonej ścieżki.
