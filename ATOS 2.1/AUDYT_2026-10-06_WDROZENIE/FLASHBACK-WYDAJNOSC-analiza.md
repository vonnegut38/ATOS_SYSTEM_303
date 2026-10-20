# FLASHBACK/REminiscence na ATOS — analiza wydajności i droga do 25–30 fps

Data: 2026-10-09. Wszystkie liczby są **zmierzone** na naszej konfiguracji
(Mega ST + TF536 68030 @32 MHz + 8 MB ST-RAM + 64 MB TT-RAM + ET4000/VOFA),
nie oszacowane.

---

## 1. Ile naprawdę mamy (pomiar)

Licznik klatek jądra, dwa odczyty w odstępie 11,1 s:

```
klatek: 343 -> 464      =  10,9 FPS
prostokatow/klatke: 4,0                      <- dirty rects dzialaja
pikseli/klatke:     1088 z 57344 (1,9%)      <- transport kopiuje grosze
```

Profil faz wbudowany w port (zegar `atos_hires` = 12288 taktów/s, więc
**1 takt ≈ 81,4 µs**; w komunikacie portu jednostką są takty):

```
PROF frames=300  total=1170 taktow = 95,2 ms   transport=77 taktow = 6,3 ms   max=19010 = 1,55 s
PROF frames=300  total=1111 taktow = 90,4 ms   transport=76 taktow = 6,2 ms   max=1266  = 103 ms
```

Rozkład:

| składnik | czas | udział |
|---|---|---|
| **transport** (`collectRects` + `ATOS_GAME_FRAME` + kopia w jądrze + damage + flush) | **6,3 ms** | **6,6%** |
| **silnik gry** (logika + rysowanie do bufora 256×224) | **~89 ms** | **~93%** |

**Wniosek: nasz system nie jest wąskim gardłem.** Transport jest już
przyrostowy (4 prostokąty i 1,9% pikseli na klatkę) i kosztuje 6 ms. Cała
reszta to praca silnika.

## 2. Czego chce sama gra

`engine/game.cpp:507`:

```c
void Game::updateTiming() {
    static const int frameHz = 30;
    int32_t delay = _stub->getTimeStamp() - _frameTimestamp;
    int32_t pause = (...) ? 20 : (1000 / frameHz);   // 33 ms
    pause -= delay;
    if (pause > 0) _stub->sleep(pause);
    _frameTimestamp = _stub->getTimeStamp();
}
```

Gra **sama celuje w 30 fps** (33 ms) i dosypia resztę. Skoro mamy 95 ms, to
`pause <= 0` — gra **nie śpi**, tylko nie wyrabia. Do 30 fps trzeba silnik
przyspieszyć **~2,9×**, do 25 fps **~2,6×**.

## 3. Co już jest zrobione dobrze (nie ma czego optymalizować)

* **transparentne dirty rects** — silnik znaczy brudne bloki 8×8, port scala je
  w prostokąty (`collectRects`) i wysyła jednym `ATOS_GAME_FRAME` (4 prostokąty,
  1088 pikseli na klatkę),
* **skalowanie 2× w jądrze**, nie w silniku (`scale=2`, `copy2x`),
* **pamięć**: port bierze **12 MB szybkiego TT-RAM** (`port/libc.c:152`,
  `atos_alloc(n,1)`, log: `heap 12288 KB`) — silnik pracuje w 32-bitowej pamięci,
* **`getTimeStamp()`** wołane jest 13× w całym kodzie (nie per obiekt) — więc
  nie jest źródłem narzutu.

## 4. Gdzie szukać 2,6–2,9× (kolejność wg spodziewanego zysku)

### 4.1 Najpierw profil silnika (bez tego reszta to zgadywanie)

W `Game::mainLoop()` (`engine/game.cpp:397`) są cztery duże fazy na klatkę:

| faza | linia | co robi |
|---|---|---|
| `memcpy(_vid._frontLayer, _vid._backLayer, _vid._layerSize)` | ~429 | **pełna** kopia warstwy tła na klatkę |
| `pge_getInput()` + `pge_prepare()` + pętla `pge_process()` | ~430–447 | logika i animacja obiektów |
| `col_prepareRoomState()` | ~432 | przygotowanie kolizji dla pokoju |
| `updateScreen()` (przez `_stub`) | koniec pętli | transport (6,3 ms) |

Wystarczy owinąć każdą z nich dwoma odczytami `atos_hires()` i wypisać sumy
(tak jak zrobiłem dla transportu). **Jeden przebieg** wskaże, czy 89 ms to
jedna dominująca faza, czy rozkłada się równo — a to decyduje, czy 30 fps jest
w ogóle osiągalne mikrooptymalizacjami.

### 4.2 Kandydaci (z znanej konstrukcji REminiscence)

1. **`col_prepareRoomState()`** — budowanie stanu kolizji pokoju. W REminiscence
   to klasycznie najcięższa funkcja klatki; jeśli dominuje → da się liczyć
   **przyrostowo** (tylko gdy zmieni się pokój/obiekty) i to może dać 2×+.
2. **Pełna kopia warstwy tła** (`memcpy` `_layerSize` na klatkę). Przy pokoju
   256×224 to ~57 KB, przy większym więcej. Silnik **już** wie, które bloki są
   brudne (dla ekranu) — tę samą informację można użyć, żeby **kopiować tylko
   zmienione pasma** zamiast całej warstwy.
3. **Pętle rysowania obiektów** (`pge_process` → `Video::drawObject`/`drawChar`)
   — pisane bajt po bajcie z warunkami na piksel. Przejście na kopiowanie
   **słowami/długimi** + rozwijanie pętli daje zwykle 1,5–2× na tych pętlach.
4. **Flagi kompilatora**: obecnie `-O2` (`Makefile`: `COMMON := ... -O2 ...`).
   Warto zmierzyć `-O3` (+`-finline-functions`) — typowo 5–15% na takim kodzie.
   Ryzyko: większy kod, możliwe pogorszenie cache — dlatego **mierzyć**, nie
   zakładać.

### 4.3 Czego NIE da się zrobić

* **Zwiększyć zegara** — 32 MHz to nasza konfiguracja emulacji, nie realna
  dźwignia,
* **Przenieść rysowania do karty** — silnik rysuje do bufora 256×224 w TT-RAM;
  to już jest szybka pamięć,
* **Odciążyć transport** — ma 6,6% udziału; nawet sprowadzenie go do zera da
  95 → 89 ms, czyli 11 → 11,8 fps.

## 5. Uczciwa ocena szans

* jeśli **jedna faza dominuje** (np. `col_prepareRoomState` 50–60 ms) → realne
  jest **25–30 fps** przez jej przepisanie ✓,
* jeśli 89 ms rozkłada się równo po wszystkich fazach → mikrooptymalizacje dadzą
  może **15–18 fps**, a 25–30 wymagałoby zmian algorytmicznych albo innego
  silnika ✗,
* dla porównania: **oryginalny Flashback na 8 MHz ST** chodził w ~10–15 fps —
  więc 11 fps na *reimplementacji* w C++ nie znaczy, że system jest zły; znaczy,
  że silnik jest ciężki.

## 6. Plan działania (proponowana kolejność)

1. **Profiler faz silnika** (4 fazy z §4.1) — jedna runda, ~15 min pracy.
2. Na podstawie wyniku: **przepisać dominującą fazę** (najpewniej kolizje albo
   kopię warstwy).
3. **Zmierzyć `-O3`** vs `-O2` (jedna zmiana w `Makefile`, jedna runda).
4. Dopiero potem mikrooptymalizacje pętli rysowania (2× na hot loopach).
5. Po każdej zmianie: ten sam pomiar (`PROF` + licznik klatek jądra), żeby
   zysk był **liczbą**, nie wrażeniem.

## 7. Co zostaje w kodzie po tej analizie

* `port/systemstub_atos.cpp`: **profiler faz** (`fb_prof_*`, raport co 300
  klatek przez `atos_puts`) — zostaje jako narzędzie, nie zaśmieca logiki.
* W jądrze: liczniki `kgame_dbg_frames/rects/pixels` i lustro `kdbg_session_out`
  — dzięki nim ten pomiar był w ogóle możliwy.

---

# POMIAR FAZ SILNIKA (wynik zaskakujący)

Dołożyłem do `Game::mainLoop()` pomiar czterech faz zegarem silnika i wypis
razem z raportem portu. Wynik (dwa okna po 300 klatek):

```
PROF frames=300 total=1168 taktow (95 ms)  transport=77 taktow (6,3 ms)  max=18752 | f0=0 f1=0 f2=0 f3=0
PROF frames=300 total=1114 taktow (91 ms)  transport=75 taktow (6,1 ms)  max=1424  | f0=0 f1=0 f2=0 f3=0
```

gdzie: **f0** = `memcpy(_vid._frontLayer, _vid._backLayer, _layerSize)`,
**f1** = `pge_getInput() + pge_prepare()`, **f2** = `col_prepareRoomState()`,
**f3** = pętla `pge_process()` po żywych obiektach.

**Wszystkie cztery są poniżej rozdzielczości zegara (1 ms).**

## Co to znaczy

Cała **logika** klatki (kopia warstwy tła, wejście, przygotowanie obiektów,
kolizje, przetworzenie obiektów) jest **praktycznie darmowa** — razem < 4 ms.
Transport to 6 ms. A klatka trwa 91–95 ms.

Czyli **~85 ms siedzi w rysowaniu/aktualizacji mapy pokoju**, które w
`Game::mainLoop()` wykonuje się **po** pętli obiektów (kod poniżej: `_loadMap`,
`drawMap`/aktualizacja warstwy, rysowanie duszków do `_screen`). To jest
**jedno** miejsce, nie rozproszone koszty — i dlatego:

* **nie warto** ruszać kolizji, wejścia ani kopii warstwy (to < 4 ms),
* **nie warto** ruszać transportu (6 ms),
* **trzeba** zmierzyć i optymalizować **rysowanie mapy pokoju** — to jest te
  85 ms i tam leży całe 25–30 fps.

## Następny pomiar (dokładnie wskazany)

Owinąć timerem **resztę `mainLoop()` po pętli `pge_process`** — czyli
aktualizację/rysowanie mapy pokoju i duszków — na tej samej zasadzie
(`atos_profile_phase(4, ...)`, tablica rozszerzona do 5). Jedna runda da
odpowiedź, czy 85 ms to np. rysowanie tła pokoju, czy scalanie duszków.

## Realistyczna ocena

Skoro 85 ms to **jedna funkcja/etap rysowania**, są dwie drogi:

1. **Optymalizacja tego etapu** (kopie słowami/długimi, pomijanie
   niezmienionych fragmentów, mniej warunków na piksel) — realne 2–3× na
   takiej pętli, czyli **wystarczające dla 25–30 fps** ✓,
2. jeśli ten etap okaże się algorytmicznie „przerysowuje cały pokój co klatkę"
   — wtedy **pomijanie niezmienionych fragmentów** (silnik już zna brudne
   bloki 8×8 dla ekranu!) da największy zysk przy najmniejszym ryzyku ✓.

Obie drogi wymagają tego jednego dodatkowego pomiaru — i to on jest teraz
najcenniejszą rzeczą do zrobienia.

---

# ★ KOREKTA PO POMIARZE ROZGRYWKI (dwie wcześniejsze tezy były błędne)

## Błąd 1: mierzyłem EKRAN TYTUŁOWY, nie grę

Po ENTER okazało się, że gra **nadal** stoi w menu, a licznik `c0`
(`Game::mainLoop`) = 0. Aktywna była `Menu::handleTitleScreen()`, a jej pętla:

```c
	_vid->updateScreen();
	_stub->sleep(EVENTS_DELAY);      // engine/menu.h:43 = 80
	_stub->processEvents();
```

**`EVENTS_DELAY = 80` ms** ✓ — czyli 80 ms snu + ~7 ms pracy = 87–90 ms = **dokładnie
zmierzone 11 fps**. To **nie** brak wydajności, to **własne taktowanie ekranu
tytułowego** w oryginalnym silniku.

## Błąd 2: „85 ms w rysowaniu mapy"

Faza 4 („reszta mainLoop") faktycznie dominuje, ale **nie** składają się na nią
rysowania, które nazwałem — wszystkie zmierzone **osobno** wyszły **0**:

```
f8 prepareAnims=0  f9 drawAnims=0  f: drawCurrentInventoryItem=0
f; drawLevelTexts=0  f< drawStoryTexts=0
```

## Prawdziwy pomiar rozgrywki (wymuszony START, zapytanie do gry)

```
menu    10.4 s: 11.0 FPS      (EVENTS_DELAY=80)
gra     10.4 s: 15.9 FPS      <- ROZGRYWKA
```

Rozkład na klatkę (12288 taktów = 1 s; ~60 ms/klatkę):

| faza | ms/klatkę | udział |
|---|---|---|
| **f4 = reszta `mainLoop`** | **52,7** | **88%** |
| — w tym transport (`ATOS_GAME_FRAME` + jądro) | 10,8 | 18% |
| f0 = `memcpy(frontLayer, backLayer, layerSize)` | 4,5–7,7 | 8–13% |
| f3 = pętla `pge_process()` | 1,8–2,6 | 3–4% |
| f1 = input+prepare | 0,7–0,8 | 1% |
| f2 = `col_prepareRoomState()` | 0,2–0,4 | 0,4% |
| f8…f< (cztery rysowania + teksty) | **0** | 0% |

Skoro cztery rysowania są zerowe, a transport to 10,8 ms, to **~30–40 ms z 52,7 ms
siedzi w `Video::updateScreen()` silnika** (`engine/video.cpp:78`) — czyli w:

* skanie tablicy `_screenBlocks` (licznik „ile klatek blok jest jeszcze brudny"),
* wywołaniach `_stub->copyRect()` dla brudnych pasm.

To jest **jeden** etap — i dlatego cel 25–30 fps jest realny.

## Wnioski praktyczne

1. **Menu**: żadnej optymalizacji nie trzeba. Jeśli chcecie płynniejsze menu,
   wystarczy zmniejszyć `EVENTS_DELAY` (80 → ~25 ms) — jedna stała w
   `engine/menu.h:43`, praca menu to ~7 ms, więc da ~30 fps.
2. **Rozgrywka (16 fps)**: cel to ścięcie 60 ms → 33–40 ms. Kolejność:
   * **`Video::updateScreen()`** (~30–40 ms) — najpierw zmierzyć w nim dwie
     części (skan bloków vs `copyRect`), potem zawęzić przerysowywanie,
   * **transport (10,8 ms)** — skalowanie ×2 w jądrze (`copy2x`) zrobić
     tablicą 256 wpisów bajt→słowo (typowy zysk 2–3× na tej pętli),
   * **f0 (4,5–7,7 ms)** — kopia całej warstwy tła na klatkę; pomijać, gdy
     warstwa się nie zmieniła.
3. **Czego nie ruszać**: kolizje (0,2 ms), input (0,7 ms), pętla obiektów
   (1,8 ms), cztery rysowania (0 ms).

## Narzędzie zostaje w kodzie

* `engine/atos_profile.h` + `port/systemstub_atos.cpp`: 16 faz (zegar
  `atos_hires`, 12288 taktów/s) i 16 liczników wejść w ścieżki klatki,
  raport co 300 klatek przez `atos_puts` (czytelny w lustrze jądra
  `kdbg_session_out`).
* Tymczasowa pomoc (impuls ENTER do pomiaru rozgrywki bez myszy) została
  **usunięta** — w obrazach jest czysty build.

---

# ★★ KOREKTA KOŃCOWA (2026-10-10) — pomiar dotyczył innego obrazu

Runda Codexa z 10-10 (`docs/BOOT-20261010.txt`, `docs/FLASHBACK-START-20261010.txt`)
zmieniła obraz docelowy i **unieważnia wnioski optymalizacyjne z tej analizy**.

## Co było naprawdę zepsute (znaleziska Codexa)

1. **Główny `work/atos-v2.img` miał STARY moduł `SYSTEM/DRV/ET4000.SYS` (5112 B)**,
   który nie znał trybu 640×480×8 (indeks 5) → **po cichu wybierał mono**
   (`seq4=06, pitch=80, bpp=1`) i **zwracał sukces**, a kernel mu wierzył.
   Dlatego na tym obrazie widać było „czerwone piksele" — to nie był błąd
   rysowania, tylko dane chunky 8 bpp skanowane jako 1 bpp.
   Nowy moduł ma **5169 B**; kernel **sprawdza geometrię i rozmiar powierzchni
   przed zapisem rejestrów** i nie ma już fałszywego sukcesu.
2. Flashback **korzysta z istniejącego trybu 256 kolorów** (nie przełącza
   rozdzielczości) — widok 256×224 nadal skalowany 2× i centrowany.
3. Doszedł wbudowany ekran **FLASHBACK / LOADING** z nazwami etapów.
4. Naprawiony poważny błąd nadzoru pulpitu (`kernel_user_take_completion()`:
   tylko wynik == 1 to zakończenie procesu).

## Zmierzony wynik na nowym obrazie (licznik klatek silnika)

**29,80 FPS średnio, 28,70–30,68 FPS** (9 próbek, ~9 s) — czyli gra osiąga
**własny cel 30 fps** (`engine/game.cpp: frameHz = 30`).

## Czego to dotyczy w moim pomiarze, a czego nie

* Moje **16 fps w rozgrywce** i **11 fps w menu** mierzyłem na osobnym
  `work/flash.img` (z nowszym sterownikiem, poprawny tryb 8 bpp). Ten pomiar
  pozostaje prawdziwy **dla tamtego builda** — i pokazywał, że 88% klatki
  siedzi w „reszcie `mainLoop`", a nie w transporcie (6–10 ms).
* Mój wniosek „85 ms w rysowaniu mapy" był **błędny** i został już wcześniej
  skorygowany: liczniki pokazały, że `prepareAnims`, `drawAnims`,
  `drawCurrentInventoryItem`, `drawLevelTexts`, `drawStoryTexts` są **zerowe**.
* Mój wcześniejszy wniosek „czerwony dither to grafika ekranu tytułowego"
  dotyczy **`flash.img`** (tam karta była w 8 bpp: `VOFA-MODE ... bpp=8`,
  a cień i karta były bit-w-bit identyczne). Na **`atos-v2.img`** prawdziwą
  przyczyną był stary moduł ET4000 → mono, i to naprawił Codex.
* **Plan optymalizacji z §4 jest w większości nieaktualny**: nie ma potrzeby
  ruszać `Video::updateScreen()` ani skalowania ×2 w jądrze — gra na nowym
  obrazie osiąga 30 fps. Realnym zadaniem pozostaje to, co Codex sam wskazał:
  **rozdzielenie formatu powierzchni klienta od formatu scanoutu** w WM, żeby
  dowolne zmiany rozdzielczości i przełączanie sesji nie powielały wierszy.

## Stan domyślny (potwierdzony)

* obraz: **`work/atos-v2.img`** — `run-tf536.sh:18`, `run-atos2.sh`,
  `run-flashback.sh:21` już na niego wskazują,
* ROM: **`build/bios_mste.bin`** (10-10 13:38) — **musi być używany razem
  z nowym obrazem** (sam dysk nie zmieni ekranu rysowanego przez stary ROM),
* do pulpitu prowadzi też klikalna aplikacja **`~/Desktop/Flashback.app`**
  (v1.2, `LSMultipleInstancesProhibited=true` — sama pilnuje jednej instancji),
* **tylko jedna instancja naraz**: druga nie zamontuje obrazu i BIOS pokaże
  `NO BOOT DEVICE - RETRY? (SPACE)` (dokładnie to zobaczyłem, próbując
  uruchomić obraz obok Waszej działającej aplikacji).
