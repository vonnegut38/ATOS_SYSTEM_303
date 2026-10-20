# Gdzie leży problem z czarnym ekranem przy starcie

## Odpowiedź w jednym zdaniu

**Geometria skanowania karty nie zgadzała się z resztą systemu**: ROM podnosił
ET4000 w **640×400 mono**, a jądro, pulpit i emulator pracują w **800×600** — więc
ekran startowy nie miał spójnego obrazu (wyglądał na czarny), a biurko pojawiało
się, bo jądro samo przełączało się na 800×600×256.

## Jak to ustaliłem — i czego NIE było przyczyną

Sprawdzone po kolei, każdą hipotezę zamknąłem dowodem:

| hipoteza | wynik |
|---|---|
| Zawieszona flaga failover (`video_pending`) | **NIE** — odczytana wprost z konfiguracji na dysku (3 kopie): `video_pending=0` |
| Brak palety (wpisy 0/1) | **NIE** — `vofa_et4000_init()` programuje je na końcu: wpis 0 czarny, 1 biały. Moja wcześniejsza „poprawka" palety była **zbędna i szkodliwa** — dlatego psuła boot |
| Karta nie jest podnoszona przez ROM | **NIE** — po zmianie trybu ekran staje się jednolitą klatką 800×600, czyli karta skanuje pamięć, którą ROM pisze |
| Zła geometria skanowania | **TAK** — patrz niżej |

W konfiguracji jest `video_mode = 2`, a w `include/config.h`:

```c
#define CFG_MODE_640   2u   /* 640x400 mono (ST high) */
```

Więc ROM — zgodnie z kodem — podnosił kartę w **640×400**. To geometria Shiftera,
nie karty, i nie zgadza się z tym, czego używa reszta systemu.

## Poprawka

`hw/vofa_rom.c`: konfiguracja ST-high (`CFG_MODE_640`, czyli 2) podnosi teraz kartę
w **`VOFA_MODE_800`** (800×600 mono) zamiast 640×400 — z komentarzem wyjaśniającym,
że to geometria natywna dla adaptera i zgodna z jądrem.

**Efekt zmierzony**: ekran o 10 s jest teraz **jednolitą klatką 800×600**
(480 384 px czerni) zamiast pomieszanego obrazu. Boot działa, bramka zielona,
biurko wstaje o 13–14 s (grafit 455 830 px) — bez zmian.

## Czego jeszcze nie ma

**Tekst POST-u nadal się nie pojawia** — ekran jest jednolity, ale czarny. Czyli
jest druga przyczyna, w ścieżce konsoli na karcie (geometria wiersza/komórki znaku
albo adres startowy wyświetlania). To następny krok, zawężony do jednego miejsca:
`ui/console.c` + `shifter_set_card_mode()` w `hw/vofa_rom.c`.

Ważne: **karta skanuje to, co ROM pisze** (dowód: jednolita klatka po wyczyszczeniu
apertury przez init), więc to nie jest już problem „pisze gdzie indziej" — to
problem formatu/geometrii zapisu znaków.

## Stan po tej turze

- Bramka **43 testy zielone**, boot zweryfikowany.
- Jedna trwała zmiana: mapowanie trybu ST-high na 800×600 mono w ROM-ie.
- Zero kodu TEMP. Reszta drzewa bez zmian.
