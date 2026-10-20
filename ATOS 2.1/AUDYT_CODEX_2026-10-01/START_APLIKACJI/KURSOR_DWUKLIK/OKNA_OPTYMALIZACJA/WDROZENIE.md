# ATOS 2.1 — wdrożenie optymalizacji okien (odpowiedź na PROPOZYCJE.md)

Audyt z 3 października 2026. Wdrożone punkty 1–5 w kolejności z raportu. Bramka
projektu: 43 testy zielone; boot w Hatari bez błędów; brak zmian wyglądu.

## 1. Rozłączne zakresy zmian w warstwie wideo — ZROBIONE (P1)

**Było:** `kernel_video.c` trzymał JEDEN zakres `[row_left,row_right)` na wiersz,
a `kvideo_damage()` rozszerzał go w obie strony. Lewa i prawa krawędź ramki
rozciągały zakres na całe wnętrze okna: `kvideo_flush()` czytał i porównywał
wszystkie słowa w tym zakresie.

**Jest:** do `KVIDEO_SPANS` (8) **rozłącznych** przedziałów `[lo,hi)` na wiersz,
trzymanych posortowanych i scalanych przy dodaniu:

```c
#define KVIDEO_SPANS 8u
static uint16_t *row_span_lo,*row_span_hi;   /* [mode_h*KVIDEO_SPANS] */
static uint8_t  *row_span_n;                 /* licznik na wiersz, 0 = czysty */
static void span_add(uint16_t y,uint16_t first,uint16_t last);
```

`span_add()` scala nachodzące i stykające się przedziały. Gdy wszystkie 8 są
zajęte, **mostkuje parę o najmniejszej przerwie** (`hi[best]=hi[best+1]`) —
poszerza skan o tę przerwę, ale nigdy nie gubi zmiany (awaryjne poszerzenie
dokładnie tak, jak proponował raport). `kvideo_flush()` przechodzi po wszystkich
przedziałach wiersza; pełne odświeżenie to jeden przedział obejmujący cały wiersz.

Koszt pamięci: 8×2×2 B × 600 wierszy = 19 200 B + 600 B liczników (było 2 400 B).

**Wynik (realny `kvideo_damage()`, 800×600×256, ramka 600×400 z (20,40) → (26,44)):**

| Zakres | przed | po | stosunek |
|---|---:|---:|---:|
| Jedna ramka | 240 000 B | **2 792 B** | 86,0× |
| Stara i nowa ramka | 244 776 B | **5 580 B** | 43,9× |

Test `test_outline_cost.py` po naprawie: `ratio=1.00` — skan jest **dokładnie**
sumą rozłącznych słów krawędzi (`outline-cost.after.log`).

## 2. Brak rutynowego pełnego odświeżenia po przesunięciu — ZROBIONE (P1)

Usunięte `kernel_wm_damage_all()` po zatwierdzeniu przesunięcia. `wm_move()`
już dodaje **stary i nowy** prostokąt ramki, a `wm_recompose()` rysuje w tych
prostokątach tło **oraz wszystkie okna wraz z dekoracjami** — więc odsłonięty
obszar odtwarza się w tym samym przebiegu. Pełne odświeżenie zostało na ścieżkach
zmiany trybu i unieważnienia bufora (`kernel_wm_reconfigure()`,
`kernel_wm_console_hide()`).

**Weryfikacja pikselowa w Hatari:** okno plików przesunięte skryptem F5;
porównanie klatki po przesunięciu z klatką po pełnym `paint()` pulpitu (F9) w
obszarze starej pozycji: **różnica 134 pikseli, wyłącznie kursor** (parkowany
przez F9 w inne miejsce). Zero resztek po starym pasku tytułu.

## 3. MOVE ≠ RESIZE u odbiorców — ZROBIONE (P1)

**Desktop** (`desktop_wm.c`): przy `WMOV` porównuje nowy rozmiar z bieżącym.
Przy zmianie wyłącznie x/y aktualizuje tylko współrzędne do hit-testów i kończy —
**zero `GFX_BATCH`, zero ponownego layoutu**. Repaint tylko przy realnej zmianie
rozmiaru. Rysowanie okien jest lokalne dla powierzchni (kompozytor sam umieszcza
klient), więc piksele klienta są niezmienione.

**Atari Commander** (`ac.c`): usunięte **synchroniczne zapytanie o tryb wideo
przy każdym WMOV** i bezwarunkowy `paint()`. Tryb jest pytany tylko przy realnej
zmianie rozmiaru (zmiana rozdzielczości, która nie zmienia rozmiaru okna, i tak
nie unieważnia layoutu lokalnego). Przy czystym przesunięciu aktualizowane są
tylko `wx/wy`.

## 4. Resize jako jedna operacja końcowa — ZROBIONE (P2)

Nowe `wm_resize_commit()` w `wm_core.c`: ramka, obszar roboczy i **jedno**
odbudowanie listy clippingu oraz **jedno** zgłoszenie damage (stary + nowy).
Wcześniej `wm_resize()` + `wm_resize_client_to_frame()` odbudowywały clipping
dwa razy i dwa razy zgłaszały ten sam prostokąt. Okno powłoki nadal używa
zwykłego `wm_resize()` (stała bitmapa 80×25).

## 5. Ograniczenie prezentacji ramki — ZROBIONE (P2)

Ramka i resize przerysowują się najwyżej raz na `WM_OUTLINE_PERIOD = 246`
jednostek hires (~20 ms przy 12288/s). Liczy się **najnowsza** pozycja; puszczenie
przycisku zatwierdza geometrię **ze swoich współrzędnych** i zawsze zdejmuje
narysowaną ramkę. Scalany jest wyłącznie ruch — żadne DOWN/UP nie jest łączone
ani opóźniane (kolejka zdarzeń z poprzedniego audytu pozostaje nietknięta, bez
sleepu w pętli wejścia).

## Weryfikacja w Hatari (TF536/ET4000, 800×600×8bpp)

- Otwarcie okna (F7), przesunięcie (F5) — treść i dekoracje poprawne, stara
  pozycja odtworzona, zero artefaktów; porównanie z pełnym odświeżeniem wypada
  identycznie poza kursorem.
- Dwuklik na ikonie „My Atari” nadal otwiera okno MY ATARI — DRIVES (regresji
  nie ma; transport kliknięć z poprzedniej naprawy działa).
- Boot czysty, bramka 43 testy zielone.

## Czego nie zrobiono

- **Punkt 6** (przesuwanie z widoczną zawartością / live move) — raport sam
  kwalifikuje go jako opcję późniejszą i ostrzega, że samo `wm_move_live()` bez
  przemieszczenia wspólnej części obrazu jest błędne. Nie ruszałem.
- **Plan pomiarów** (p50/p95 opóźnienia ramki/kursora, liczba flush na krok,
  liczba skanowanych i zapisanych słów, powierzchnia damage w kolorze i mono) —
  wymaga scenariusza zbierającego próbki. Liczniki są już częściowo w miejscu
  (`kvideo_dbg_calls/copies`, `wm_damage_pixels`, `kdbg_wm_composes`,
  `kdbg_recompose_cycles`, `kdbg_deco_cycles`). To osobny krok, nie zmiana kodu
  ścieżki.
- Nie porównywałem pamięci i czasu wariantu „8 przedziałów” z bitmapą zmienionych
  bloków (raport proponował porównanie) — wybrałem przedziały, bo skan w teście
  wychodzi dokładnie równy sumie krawędzi, a koszt pamięci jest znikomy wobec
  256 KB shadow.
