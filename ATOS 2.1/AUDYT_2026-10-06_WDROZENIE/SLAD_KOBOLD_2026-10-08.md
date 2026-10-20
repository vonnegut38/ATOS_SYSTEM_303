# ŚLAD RZECZYWISTYCH WYWOŁAŃ KOBOLD-a (do momentu wyjścia)

Pomiar na prawdziwym jądrze (obraz `work/kobold.img` + kernel z autostartem).
Rejestr: warstwa, numer funkcji, PC wywołującego, 4 argumenty, **wynik**.

```
wywołań GEM: 122        (identyczne zatrzymanie jak przed poprawkami wind_*)
nieobsługiwanych: 14
   0. VDI    fn=115   pc=0x00000000  wynik=0    arg=0,0,0,0        <- vqin_mode
   1. AES    fn=13    pc=0x040004AA  wynik=0    arg=0,0,0,0        <- appl_find
   2. AES    fn=130   pc=0x040004AA  wynik=0    arg=4,0,0,0        <- zapytanie fontu
   3. AES    fn=130   pc=0x040004AA  wynik=0    arg=6,0,0,0
   4. AES    fn=130   pc=0x040004AA  wynik=0    arg=7,0,0,0
   5. AES    fn=130   pc=0x040004AA  wynik=0    arg=11,0,0,0
   6. GEMDOS fn=281   pc=0x04000550  wynik=-32  arg=1,1026,58180,1025   <- numer spoza GEMDOS
   7. GEMDOS fn=309   pc=0x040005E4  wynik=-32  arg=0,32,1026,54966
   8. AES    fn=12    pc=0x0401780E  wynik=0    arg=0,16,0,0       <- appl_write
   9. BIOS   fn=5     pc=0x0401872A  wynik=0    arg=257,1024,6928,1026  <- Setexc!
  10. GEMDOS fn=338   pc=0x040184F0  wynik=-32  arg=1026,14478,1027,9702
  11. BIOS   fn=5     pc=0x0401872A  wynik=0    arg=257,0,0,1026        <- Setexc!
ZAKONCZENIE: wyjątek wektor 5 (dzielenie przez zero), exit=-5, GEM=122
```

## Co z tego wynika (i czego NIE wynika)

1. **Program kończy wyjątek, nie decyzja o wyjściu**: `kernel_fault_vector = 5`
   (dzielenie przez zero), `kdbg_tos_divzero = 1`, kod wyjścia `-5`. Nie ma tu
   normalnego `Pterm`.
2. **`BIOS Setexc` (funkcja 5) zwraca 0** — KOBOLD instaluje wektor (argument
   257 = 0x101, czyli **systemowy wektor `etv_term`**!) i dostaje jako „poprzedni
   wektor" **NULL**. To dokładnie klasa awarii, którą wskazywały raporty.
3. Nieobsłużone: VDI `vqin_mode` (115), AES `appl_find` (13), AES `appl_write`
   (12), zapytania 130, oraz BIOS `Setexc` (5).
4. **Samo nieobsłużone wywołanie nie dowodzi wyjścia** — 14 wywołań zwróciło
   wartości, których program nie skomentował; wyjście przyszło z dzielenia przez
   zero później. Dopiero wskazanie **dzielnika** rozstrzygnie, które API go
   dostarczyło.

## Błędy mojej instrumentacji (uczciwie)

* Dla VDI nie zapisuję PC wywołującego (`pc=0`), bo ścieżka VDI nie ustawia
  `tos_last_caller_pc` — do poprawy.
* Numery GEMDOS 281/309/338 **nie są numerami GEMDOS** — czyli dla części wywołań
  wyciągam słowo funkcji z niewłaściwego miejsca ramki (do zbadania: wariant
  długiej ramki 68010+ albo wywołanie przez `trap #1` z innym układem stosu).
* `kdbg_term_normal`/`kdbg_term_fault` nie zostały odczytane (symbol nie trafił
  do odczytu) — zapis zakończenia mam z `fault_vector` + `exit_code`.

---

# KAMIEŃ MILOWY: odczyt katalogu standardowym API — OSIĄGNIĘTY

Pomiar na prawdziwym jądrze, sonda `TOSDIR.PRG` (nowy program `system/tosdir.c`),
która używa dokładnie tych samych wywołań co program TOS
(`Dsetpath` 0x3B, `Dgetpath` 0x47, `Fsetdta` 0x1A, `Fsfirst` 0x4E, `Fsnext` 0x4F,
słowo funkcji na stosie, przez `trap #1`):

```
Dsetpath("\\SYSTEM\\TEST")     -> 0
Dgetpath(...)                  -> 0, zwrocil "\\SYSTEM\\TEST"
Fsetdta(my_dta)                -> 0
Fsfirst("\\SYSTEM\\TEST\\*.*") -> 0, wpisow: 6
```

## Co zostało naprawione, żeby to zadziałało

1. **Limit listingu 16 → 384 wpisy** (`TOS_DIR_MAX`). Katalog większy niż 16
   pozycji był **ucinany** — dla menedżera plików to dyskwalifikacja.
2. **Filtr atrybutów `Fsfirst`** — dotąd ignorowany; teraz respektowany
   (katalogi tylko gdy `0x10`, ukryte gdy `0x02`, systemowe gdy `0x04`,
   etykieta wolumenu `0x08` nigdy nie trafia na listę).
3. **`Dsetpath` (0x3B) — nie istniał** (zwracał −32); teraz waliduje katalog
   przez `kfat_stat_path` i zapamiętuje ścieżkę bieżącą.
4. **`Dgetpath` (0x47) zwracał zawsze pusty string**; teraz zwraca bieżący
   katalog programu (potwierdzone: `\SYSTEM\TEST`).
5. **`Fdatime` (0x57) — nie istniał**; teraz zwraca poprawną datę/godzinę
   (bez znaczników czasu w warstwie FAT zwracamy stałą ważną datę, a nie błąd,
   żeby listingi pokazywały sensowne wartości).
6. Ścieżka bieżąca jest **zerowana na start każdego programu** (stan sesji),
   łącznie z cache katalogu i filtrem atrybutów.

## Czego to NIE dowodzi

* Nie dowodzi, że KOBOLD dojdzie dalej — jego zatrzymanie to **dzielenie przez
  zero** (wektor 5, exit −5) i tego jeszcze nie usunąłem; nie znamy **dzielnika**.
* Nie ruszono surowego I/O, DMA, PSG ani wektorów — zgodnie z ustaloną
  kolejnością czekają na ten wynik.
* Odczyt katalogu działa dla ścieżek bezwzględnych i względnych, ale nie
  sprawdziłem jeszcze listingu katalogu z **podkatalogami** (atrybut 0x10)
  na dużej zawartości.

---

# RUNDA: Setexc, vq_mouse, form_center + test kończący

## Wdrożone w tej rundzie

1. **`BIOS Setexc` (funkcja 5)** — nie istniał (zwracał 0). Teraz:
   * zwraca **prawdziwy poprzedni wektor** i instaluje nowy, gdy handler ≠ −1,
   * dla wektora, którego slot jest pusty, zwraca **działający stub `rte`**
     (`asf_tos_just_rte`) — program, który zapamięta „poprzedni handler"
     i zawoła go w łańcuchu, **nie skacze już pod adres 0**,
   * obsługuje też wektory systemowe `0x100+` (`etv_*`, w tym `etv_term`).
2. **Pełny snapshot wektorów użytkownika** (2..47 oraz 0x60..0x7F) zapisywany na
   starcie programu i **przywracany na wyjściu** — nic nie może zostać wskazywać
   w zwolnioną pamięć TPA (klasa „JSR pod 0").
3. **`vq_mouse` (VDI 124)** — zwracał `(0,0)`, czyli „kursor się nie ruszył".
   Teraz zwraca prawdziwy stan (przyciski w `intout[0]`, `ptsout[0..1]` = x,y).
4. **`form_center` (AES 54)** — liczył tylko prostokąt **obiektu 0**; przy
   korzeniu bez rozmiaru zwracał `w=0,h=0`, a program liczący
   „kolumny = szerokość / komórka" dzielił przez zero. Teraz liczy **obwiednię
   całego drzewa** (bounded walk po łańcuchu `ob_next`).

## Test kończący rundę (obowiązkowy) — przebieg

Sekwencja autostartu: `TOSDIR` → `TOSDIR` → `TOSMONO` → `TOSDIR`.
`TOSDIR` kończy się teraz standardowym `Pterm0`, więc można go uruchamiać wielokrotnie.

```
t(s)  | wywołań GEM(1) | fault | exit | wpisów z odczytu katalogu
   6  |       0        |   0   |   0  |   6
 120  |       0        |   0   |   0  |   6
```

* **`fault = 0` przez całą sekwencję** ✓ — trzy uruchomienia tego samego
  programu i jedno innego, bez wyjątku, bez restartu pulpitu.
* **Odczyt katalogu = 6 wpisów** ✓ — dysk działa po wielokrotnym uruchomieniu.
* (1) `tos_aes_log_n` liczy wywołania **persony TOS**; desktop jest aplikacją
  natywną, więc 0 jest tu oczekiwane.

## Czego ten przebieg NIE sprawdził (błąd pomiaru, nie systemu)

* **Wejście**: klawisze wstrzyknąłem **po** zakończeniu sekwencji (t≈120 s),
  gdy żaden program TOS już nie działał — licznik `tos_evkbd_n` słusznie został 0.
  Trzeba wstrzykiwać klawisze **w trakcie** działania trzeciego programu.
* **Zegar**: odczyt `0x4BA` zwrócił stałą `5377100`, co dla licznika 200 Hz jest
  nierealne — adres odczytu wymaga sprawdzenia (nasz kernel trzyma `_hz_200`
  jako long pod 0x4BA, ale odczyt przez debugger może trafiać w inny obszar).

---

# RUNDA: pościg za dzielnikiem — wyniki i uczciwy stan

## Wdrożone

1. **Dziennik AES/VDI rozszerzony**: do **16 zadeklarowanych wyników** na wywołanie
   (wpis 32 słowa). Dzięki temu widać nie tylko `intout[0]`, ale wszystkie pola,
   które program mógł wykorzystać jako dzielnik.
2. **`aes_obfix` zgodny z GEM**: przekształca współrzędne znakowe na piksele
   **tylko dla obiektów tekstowych** (G_BUTTON/G_STRING/G_TITLE/G_TEXT/
   G_BOXTEXT/G_FTEXT/G_FBOXTEXT/G_BOXCHAR/G_ICON/G_CICON), a obiekty pikselowe
   (G_BOX/G_IBOX/G_IMAGE) **zostawia bez zmian**. Poprzednio mnożył *wszystko*
   przez 8/16, co mogło zawinąć szerokość do zera.

## Co pokazał pomiar (po zmianie)

```
wywołań GEM: 122   fault=5 (dzielenie przez zero)   divzero=1   exit=-5
ostatnie 14 wywołań: AES op=114 (rsrc_obfix), wynik 1, out=[1,0,0,0,0,0,0,0]
rejestry w chwili faultu: d0=184 d1=208 d2=1 d3=184 d4=8 d5=208 d6=0 d7=0
```

**Rejestry są identyczne przed i po poprawce `aes_obfix`** — czyli ta poprawka
jest słuszna zgodnościowo, ale **nie jest dzielnikiem**. Dzielnikiem jest
**`d7 = 0`** (instrukcja `divu/ddiv`), a `d4 = 8` to szerokość komórki znaku.

## Następny krok (dokładnie wskazany)

Mam **PC faultu = `0x0401AD02`** i bazę TPA, więc źródło `d7` da się odczytać
z deasemblacji:

* tekst programu leży pod `0x04000100`, plik ma 28-bajtowy nagłówek →
  **offset w pliku = `0x1AC1E`** (109 598),
* trzeba wyciąć `KOBOLD_3.PRG` z obrazu (`work/kobold.img`, ścieżka
  `\KOBOLD\KOBOLD_3.PRG`) i zdeasemblować okno wokół tego offsetu,
* widać wtedy, **z jakiego adresu ładowane jest `d7`** — a to wskazuje, które
  API (font? ekran? rozmiar okna?) ma zwracać wartość niezerową.

Uwaga operacyjna: `/tmp` na tej maszynie jest czyszczone w trakcie pracy, więc
kopie robocze (drzewo testowe, ekstraktor FAT, zrzuty) trzeba trzymać
w katalogu projektu, nie w `/tmp`.

---

# RUNDA: deasemblacja w miejscu dzielenia — korekta i wniosek

## Co zrobiłem

* Dodałem **trwałe narzędzie** `tools/img_get.py` (czytanie plików z obrazu AHDI
  FAT16 — dopełnienie `img_put.py`). `/tmp` jest na tej maszynie czyszczone
  w trakcie pracy, więc narzędzia muszą być w repo.
* Wyciągnąłem `\KOBOLD\KOBOLD_3.PRG` (192 547 B) z `work/kobold.img`.
* Zdeasemblowałem okno wokół `PC = 0x0401AD02` (offset pliku `0x1AC1E`).

## Wynik: to NIE jest instrukcja dzielenia

```
  401ad00:  movew %a5@,%d0
  401ad02:  jsr 0x1857c          <- to zapisano jako PC faultu
  401ad08:  addqw #8,%sp
```

Czyli **PC faultu, którym się posłużyłem, pochodzi z poprzedniej rundy** —
po moich poprawkach (`Setexc`, `form_center`, `aes_obfix`) program może
zatrzymywać się w **innym miejscu**. Rejestry w chwili faultu pozostały
identyczne (`d0=184 d1=208 d4=8 d6=0 d7=0`), ale PC trzeba odczytać
z **bieżącego** przebiegu, a nie z zapisanej wcześniej wartości.

## Wniosek i dokładny następny krok

1. Zrobić przebieg i odczytać **jednocześnie**: `kernel_fault_pc`,
   `kernel_fault_vector`, `kernel_fault_regs`, oraz ostatnie wpisy dziennika
   AES/VDI (już rozszerzonego na 16 wyników).
2. Zdeasemblować **ten** adres (offset w pliku = `28 + (PC − 0x04000100)`).
3. Odczytać, **skąd ładowany jest `d7`** — to wskaże API, którego wynik ma być
   niezerowy (kandydaci z raportu luk: metryki fontu, `form_center`,
   `wind_get`, licznik fontów).

Mam już wszystkie potrzebne narzędzia: trwały ekstraktor obrazu, rejestr
nieobsłużonych wywołań z argumentami i wynikiem, dziennik 16 wyników na
wywołanie oraz zapis rejestrów w chwili wyjątku.
