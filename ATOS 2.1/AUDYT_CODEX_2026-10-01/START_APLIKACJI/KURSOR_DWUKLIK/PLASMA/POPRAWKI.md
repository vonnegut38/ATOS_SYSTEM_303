# PLASMA — poprawki po wdrożeniu: czarne marginesy i pomiar wydajności

## 1. Czarne marginesy z prawej i z dołu — NAPRAWIONE

**Przyczyna.** Płótno efektu miało 32×22 komórki = **320×220 px**, a okno 340×260
(klient 340×244). Startowy `add_fill(0,0,WW,CLIENT_H,0)` wypełniał **cały klient**
indeksem palety **0 = czarny**, więc poza płótnem zostawał czarny pas 20 px z prawej
i 24 px z dołu.

**Naprawa.** Płótno jest teraz **dokładnie obszarem klienta**:

```c
#define CW 34u
#define CH 24u
#define CELL 10u
#define PW (CW*CELL)   /* 340 */
#define PH (CH*CELL)   /* 240 */
#define WW PW
#define WH (PH+16u)    /* + pasek tytułu = 256 */
#define CLIENT_H PH
```

Zmierzone na zrzucie z Hatari: okno **340×256**, brak czarnych pasów przy prawej
i dolnej krawędzi. Test `test_plasma.py` zaktualizowany do 340×240 — przechodzi
(`PASS: every phase, every pixel, colour and mono`, `batches per frame: 10`,
`idle animation: batches=101 sleeps=50 blocking input waits=0`).

## 2. „Za wolno" — co naprawdę jest ograniczeniem

Zmierzyłem czas klatki wewnątrz aplikacji (znaczniki `hires()` kodowane jako piksele
w oknie, odczytywane ze zrzutu). Wyniki dla 800×600×256, okno 340×240:

| wariant | klatka | Uwaga |
|---|---:|---|
| patch Codexa: lista prostokątów, ~700 pozycji, 9 batchy | **~111 ms** | 10 fps |
| bloki 2×2 (204 pozycje) | ~120 ms | **bez zysku** |
| jeden bufor 8 bpp + prezentacja jednym wywołaniem | ~120 ms | **bez zysku** |

Kluczowy wniosek: **liczba prostokątów i liczba wywołań IPC nie są ograniczeniem.**
Zejście z ~700 pozycji do 204 nie zmieniło FPS, więc per-item overhead nie jest
wąskim gardłem. Ograniczeniem jest **zapis pikseli**: 340×240 = 81 600 bajtów na
klatkę do powierzchni okna, plus kompozycja i flush do VRAM. W tym emulatorze
kosztuje to ~1 µs/bajt, co daje ~80–90 ms na sam zapis klatki i ~10–12 fps pułapu
dla tego rozmiaru płótna.

**Czego próbowałem i odrzuciłem:** nowy opcode 37 (`kgfx_present_8bpp`) — aplikacja
buduje bufor 8 bpp i kernel kopiuje go do powierzchni jednym wywołaniem. Działa
poprawnie (zweryfikowane), ale jest **nie szybszy**: aplikacja zapisuje 81 600 B do
bufora, a kernel kopiuje je drugi raz do powierzchni, więc praca się **podwaja**
wobec ścieżki batch, która pisze do powierzchni od razu. Usunąłem ten opcode, żeby
nie zostawiać martwego kodu. (Wniosek: przy tym układzie warstw „jeden blit" nie
jest optymalizacją, dopóki bufor nie jest powierzchnią okna.)

**Co realnie może przyspieszyć:** tylko zmniejszenie liczby zmienianych pikseli na
klatkę, czyli **mniejsza płótno** (FPS skaluje się odwrotnie do powierzchni), albo
mniejszy zakres odświeżania. Przy 340×240 w tym emulatorze ~10 fps to sufit; na
prawdziwym 030 z VGA ten sam kod ma ~30× tańszy zapis pamięci, więc zapas jest
duży — ale to nie jest coś, co da się pokazać w Hatari.

## 3. Stan po zmianach

- `system/plasma.c`: patch Codexa + geometria „klient == płótno".
- Bramka projektu: **43 testy zielone**; `test_plasma.py` przechodzi.
- Boot w Hatari czysty (459 881 px teal, 4 618 px czarnych, 0 faultów).
- Rozmiar `PLASMA.APP`: 1 840 B.
- `desktop_wm.c` bit w bit równy wersji sprzed weryfikacji (tymczasowa zmiana F9
  cofnięta); brak jakiegokolwiek kodu TEMP w źródłach.

## 4. Wdrożone: płótno 260×180 i prędkość fali liczona z czasu

Na życzenie: `CW=26, CH=18` → **260×180 px**, okno 260×196. Płótno nadal wypełnia
cały klient (bez czarnych pasów).

**Zmierzone (kodowanie fps/ms w pierwszych wierszach płótna, odczyt ze zrzutu):**

| płótno | okres klatki | klatka | FPS |
|---|---:|---:|---:|
| 340×240 | 1229 (100 ms) | 111 ms | 10 (ograniczone kosztem) |
| 260×180 | 1229 (100 ms) | 13 ms | **10 (ograniczone tempem)** |
| 260×180 | 490 (40 ms) | 16 ms | **20** |
| 260×180 | 246 (20 ms) | 16 ms | **27** |

Przy 340×240 ograniczeniem był koszt klatki (111 ms > 100 ms okresu). Przy
260×180 klatka spadła do **16 ms**, więc o tempie decyduje już tylko ziarnistość
budzenia (`pause_tick` = 1 tick); dlatego skróciłem okres do `FRAME_COUNTS=246`
i wyszło **27 fps** — 2,7× więcej niż na starcie.

**Prędkość fali nie zależy od FPS.** Faza przesuwa się teraz z **czasu**
(`PHASE_COUNTS 1229`, `PHASE_STEP 15` — dokładnie ta sama prędkość co dawniej),
więc wyższe odświeżanie daje **płynniejszy** obraz, a nie szybszy. Gdybyś chciał
szybszą falę, to jedna stała (`PHASE_STEP`).
