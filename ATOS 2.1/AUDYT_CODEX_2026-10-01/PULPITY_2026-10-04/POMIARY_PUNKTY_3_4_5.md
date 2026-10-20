# Pomiary punktów 3, 4 i 5 (2026-10-04)

Wszystkie liczby odczytane z działającego jądra w emulatorze (symbole z `nm` +
`read_u32`), nie z szacunków. Metoda: odczyt w dwóch punktach czasu, różnica.

## Punkt 3 — odczyty sektorowe (zrobione)

**Naprawione i zweryfikowane na żywo.** `kblock_read()` przy pełnym trafieniu w
cache wołał `lookup()` dwa razy na sektor (raz sprawdzenie, raz pobranie). Sloty z
pierwszego przejścia są teraz zapamiętywane.

| miara | przed | po |
|---|---|---|
| sondowania cache na sektor (odczyt z cache) | 2,00 | **1,00** |
| — zmierzone w systemie | | `4x okno plików → sondowania +4, trafienia +4` |
| — test `test_block_cache` | | `3 searches for 3 sectors PASS` |

Test wykrywa regresję: po przywróceniu starej wersji pada na `(n==3u)`.

## Punkt 3 — alokator (ZMIERZONY, świadomie NIETKNIĘTY)

`pool_alloc_reserved()` faktycznie robi dwa pełne przejścia listy bloków
(`stats()`, potem `pool_alloc`), a przy dwóch pulach do czterech. Ale:

```
po starcie         mem_list_walks = 48
3x otwarcie okna   mem_list_walks = 49   (+1)
3x F6              mem_list_walks = 49   (+0)
```

**Cały rozruch to 48 przejść.** To nie jest gorąca ścieżka — zmiana w najbardziej
krytycznym kodzie systemu nie jest uzasadniona. Licznik `kmem_list_walks` zostaje
jako narzędzie.

## Punkt 4 — pętla bezczynności

```
iteracji pętli:     689 w 6 s  = 115/s
cykli na iterację:  188
ramek kompozytora:  22 723 w 6 s
pikseli uszkodzeń:  0
```

Pętla jest **ograniczona i tania**, a kompozytor w spoczynku nie robi **ani jednego
piksela**. Jedna rzecz warta uwagi: 22 723 „ramek" w 6 s to ~33 na jedno przejście
pętli — kompozytor składa klatkę częściej, niż wynikałoby z 115 iteracji/s. Przy
zerowej liczbie uszkodzeń to nie kosztuje pikseli, ale warto to zmierzyć osobno,
jeśli będziemy kiedyś optymalizować spoczynek.

## Punkt 4 — rezerwa 16 MiB (wyjaśniona, nie jest błędem)

```
ST:  total = 4 194 304 B   reserved =   737 536 B   (jądro, 720 KB)
TT:  total = 67 108 864 B  reserved = 16 777 216 B  (dokładnie 16 MB)
     koniec puli = 0x04000000 (64 MB)
```

Kod (`kmem_init`): pula TT startuje na 0x01000000 (16 MB), a `tt_end` jest
przycinane do 0x04000000 — **górne 16 MB z 64 MB zostają dla TPA osobowości TOS**,
zgodnie z komentarzem w źródle. To **świadoma decyzja polityczna, nie defekt** —
ale kosztuje 25% TT-RAM, więc jeśli TPA ma być mniejsze, to jest miejsce do
odzyskania. Do decyzji, nie do „naprawy".

## Punkt 5 — cache dysku i rysowanie uszkodzeń (potwierdzone liczbami)

```
bezczynność:        trafienia 117 / pudła 121  (49%)
4x okno plików:     trafienia +4, pudła +0, odczyty +0   → 100% trafień
```

Cache **działa**: przy powtarzanym ruchu plikowym trafia w 100% i nie wykonuje
żadnego odczytu z dysku. Rysowanie uszkodzeń: **0 pikseli w spoczynku**, więc
kompozytor jest naprawdę sterowany uszkodzeniami.

**Nie ma podstaw do przepisywania któregokolwiek z nich** — to potwierdzenie Twojej
tezy, teraz z liczbami.

## Wnioski

1. **Punkt 3**: cache naprawiony (2,00 → 1,00 sondowania na sektor); alokator
   zmierzony i zostawiony.
2. **Punkt 4**: pętla bezczynności tania; rezerwa 16 MB zamierzona i udokumentowana
   — do decyzji, nie do zmiany.
3. **Punkt 5**: cache dysku i rysowanie uszkodzeń działają i są tanie — **nie ruszać**.
