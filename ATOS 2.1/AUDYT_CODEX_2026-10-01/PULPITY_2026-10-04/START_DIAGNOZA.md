# Start: dlaczego BIOS-u nie widać — diagnoza

## Wymaganie (od użytkownika)

Jedna droga dla 68030:

- **Karta VGA znaleziona** (ET4000 lub inna) → przełączyć na nią, pokazać BIOS,
  wstać domyślnie w **800x600x256**.
- **Brak karty** → Shifter, **mono 640x400**.
- Kolorowe tryby ST — na później.

## Warunek, który o wszystkim decyduje (w emulatorze)

`atari-030-port/hatari-src/src/vofa.c`:

```c
vofa_drive = ((bpp == 1 || bpp == 8) && width >= 640 && height >= 400);
```

Dopóki to jest 0, emulator wyświetla **Shifter** — i to jest ta czarna plansza.
Karta jako źródło obrazu wchodzi **tylko** przy 1/8 bpp i co najmniej 640x400.

## Co zmierzone

Pełny log `VOFA-MODE` z jednego rozruchu (19 linii, 30 s):

```
pierwsze 3:  w=0    h=0   bpp=0 drive=0
ostatnie 3:  w=800  h=600 bpp=8 drive=1
niezerowych: 16 z 19   -> karta wchodzi dopiero od 4. linii (~6 s)
```

Czyli **karta JEST podnoszona i to w 800x600x256** — ale dopiero ok. 6 s i robi to
**jądro**, nie ROM. Wniosek: `vofa_rom_init()` w ROM-ie **kończy się niepowodzeniem**
(bo gdyby się udało, przełączenie nastąpiłoby w pierwszych liniach logu).

A skoro ROM pada, to:

1. konsola ROM-u zostaje na Shifterze,
2. **tam** trafia POST z logo i paskiem postępu,
3. o ~6 s jądro samo aktywuje kartę i ekran się przełącza,
4. na karcie jest to, co narysowało jądro — a pierwszy pełny render robi dopiero
   pulpit, stąd czerń do momentu biurka.

To wyjaśnia **wszystko**: i czarny start, i to, że logo widać w buforze konsoli, a
nie na ekranie, i to, że biurko pojawia się normalnie.

## Czego to NIE jest

- **Nie zatrzask failover** — `video_pending=0`, `video_driver=0` (auto),
  `video_mode=2` na obrazie.
- **Nie regresja 2.1 vs 2.0** — ATOS 2.0 zachowuje się identycznie (`w=0`), mimo
  że `hw/machine.c`, `hw/nova.c` i `hw/vofa_rom.c` (poza moim jednym przypadkiem)
  są w obu drzewach takie same. Problem jest **wcześniejszy niż oba drzewa**.
- **Nie plik konfiguracyjny** — ten sam wynik z `-c atos2.cfg` i bez niego.

## Błąd metody, który sam popełniłem

Bisekcję `case CFG_MODE_640` w `vofa_rom.c` przeprowadziłem z `grep -m3` — czyli
widziałem **tylko trzy pierwsze linie logu**, a to zawsze są zera. Test był
bezwartościowy i wyciągnąłem z niego błędny wniosek „mój przypadek jest niewinny".
Trzeba było czytać cały log od razu.

## Następny krok (konkretny i mały)

ROM już utrzymuje `vofa_rom_status` z kodem wyniku:

```
0x101 = start, 0x102 = pre-check przeszedł,
0x300|result = wynik (1=OK, 0=ABSENT, -1=I/O, -2=CLOCK, -3=MEMORY),
0x400 = przejście na kartę zatwierdzone
```

Odczytanie tej jednej liczby **po** rozruchu mówi dokładnie, na którym warunku
`vofa_et4000_init()` się zatrzymuje. Kandydaci w kodzie: próba obecności pod
`VOFA_MEM+0x80000` (512 KB w głąb VRAM) oraz test kontrolera atrybutów
(`WR(0x3c0,0x0a);WR(0x3c0,0x55);RD(0x3c0)` ma zwrócić 0x0a).

Trzeba ją przenieść do jądra — handoff ma wolne bajty — i wypisać, tak jak
`kfat_counts` i `kmem_list_walks`, które już tym kanałem działają.

## Stan

Bramka **45 testów** zielona w obu profilach, boot zweryfikowany, zero kodu TEMP.
Nic nie zostawiam w połowie zmienione.
