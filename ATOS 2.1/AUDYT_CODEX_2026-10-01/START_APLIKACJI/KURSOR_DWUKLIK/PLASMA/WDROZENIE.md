# ATOS 2.1 — wdrożenie poprawki PLASMA (odpowiedź na RAPORT.md)

## 1. Zastosowanie patchu

Wersja bazowa w projekcie **zgadza się z manifestem**:

```
system/plasma.c  sha256 = 3ed718aff8378080f856d8bc984e9256bfee6a3c5ba9fb415ace31a1a48bd11e
baseline_sha256         = 3ed718aff8378080f856d8bc984e9256bfee6a3c5ba9fb415ace31a1a48bd11e
```

`patch -p1 --dry-run` przeszedł czysto na obu plikach, więc zastosowałem
`plasma.patch` (`system/plasma.c` + zależność `Makefile` od `system/icons_color.h`).
Późniejszych zmian w źródłach nie nadpisywałem — jedyna zmiana w `desktop_wm.c`
podczas weryfikacji była tymczasowa i została cofnięta (plik bit w bit równy
wersji sprzed testu).

Wynikowy `system/plasma.c` jest **identyczny** z dostarczonym plikiem, a
zbudowany `PLASMA.APP` jest **bajt w bajt** równy dostarczonemu:

```
701bdc3ee84d182282fe7b2c327b281d03f53a00789371598dc081bcc7678216  build/system/PLASMA.APP
701bdc3ee84d182282fe7b2c327b281d03f53a00789371598dc081bcc7678216  .../PLASMA/PLASMA.APP   (= binary_sha256 z manifestu)
```

## 2. Test dostarczony przez audyt

Na wersji bazowej `test_plasma.py` **nie kompiluje się** (brak `prepare()`,
`colour()`, `add_fill()`, `FRAME_COUNTS`). Po patchu:

```
PASS: every phase, every pixel, colour and mono
batches per frame: 9
idle animation: batches=91 sleeps=50 blocking input waits=0
PASS: advances without mouse/keyboard and closes on WCLS
```

Test sprawdza **każdą fazę i każdy piksel** obu wariantów wobec niezależnego
wzoru referencyjnego, a potem pracę bez wejścia (`blocking input waits=0`).

## 3. Weryfikacja w Hatari (TF536/ET4000, 800×600×256)

Uruchomienie **rzeczywistą ścieżką użytkownika** (dwuklik na ikonie PLASMA), nie
auto-startem:

- Pulpit przed uruchomieniem: 459 881 px teal, 3 223 px czarnych — czysty.
- Okno PLASMA otwiera się **w kolorze** (niebieskie/białe/czerwone/zielone/żółte
  komórki), marginesy czyszczone czernią.
- **Cztery zrzuty co ~2 s bez żadnego wejścia**: różnica pikseli w obszarze okna
  rośnie (69 660 / 67 680 / 69 960 px) — animacja posuwa się samoczynnie, zegar
  ją napędza, nie wiadomość myszy.
- Bramka projektu: **43 testy zielone**. Boot po instalacji czysty (459 881 px
  teal, 4 618 px czarnych, 0 faultów).

## 4. Uwaga praktyczna dla odtwarzających

Przy pierwszej próbie umieściłem auto-start (jak w raporcie) zaraz po
`load_dyn_icons()`, czyli **w trakcie inicjalizacji pulpitu**. Ekran wyszedł
częściowo czarny (pulpit bez tła, ikony widoczne; 296 541 px czarnych zamiast
4 618). Po przeniesieniu uruchomienia na rzeczywistą ścieżkę kliknięcia obraz
jest poprawny. Czyli to **wyścig startowy pulpitu**, a nie defekt PLASMA —
ale przy powtarzaniu metody z raportu warto auto-start wstawić **po** starcie
pętli pulpitu, nie w środku jego inicjalizacji.

## 5. Czego nie zrobiono

- **Pomiaru FPS i użycia CPU** — raport sam tego nie mierzył; `FRAME_COUNTS=1229`
  przy 12 288 jednostkach/s to 10,0 Hz (100 ms) docelowego harmonogramu, nie
  zmierzona osiągnięta prędkość.
- **Mono w emulatorze** — pokryte tylko na poziomie funkcji (test hosta).
- **Dalszej optymalizacji prezentacji** (jeden blit 8bpp zamiast 9 batchy
  prostokątów) — raport kwalifikuje ją jako osobną, mierzoną pracę; przy
  `batches per frame: 9` to nadal główny koszt IPC, ale usunięcie zatrzymania go
  nie wymagało.
