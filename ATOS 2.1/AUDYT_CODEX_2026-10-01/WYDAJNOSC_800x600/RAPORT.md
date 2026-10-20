# Przyczyny wolnego startu ATOS 2.1 w 800x600x256

Badanie aktualnego kodu z 1 października 2026 r. wykazało konkretne błędy ścieżki kolorowej. Przygotowana poprawka zachowuje start w 800×600×256; nie uzyskuje przyspieszenia przez przełączenie startu na mono. Kod i obrazy robocze DeepSeeka pozostają niezmienione. Wszystkie modyfikacje i uruchomienia wykonano na osobnej kopii.

## Potwierdzone przyczyny

1. **Brak aktywacji cache znaków w kolorze.** `system/text_terminal.c`, `term_render_chunky_polled()`, kończyła renderowanie bez `d->valid=1`. Wersja mono ustawia ten znacznik. Reproduktor na aktualnym kodzie pokazał: pierwszy render 2000 znaków, kolejny niezmieniony render 2000, zmiana jednego znaku również 2000. Po poprawce wyniki to 2000, 0, 1. Pełne rysowanie konsoli oznacza 256000 bajtów zapisów pikseli; jeden znak wymaga 128 bajtów. Ten stosunek opisuje pracę renderera, a nie przyspieszenie całego systemu.
2. **Kopiowanie VRAM do tego samego VRAM.** `kgfx_console_present()` rysuje bezpośrednio do bufora karty i ustawia go jako źródło `kvideo_flush()`. Flush następnie ponownie czyta obraz karty oraz sprawdza shadow. Przy nieważnym cache znaków obejmuje to pełną ramkę 480000 bajtów przy każdej prezentacji konsoli. Poprawka pomija transfer, kiedy źródło i cel są identyczne. Zmiana źródła na inny bufor nadal wymusza pełne odtworzenie obrazu i shadow; obejmuje to test regresji.
3. **Wielokrotne przewijanie ekranu przez loader.** `system/boot_main.c` wypisuje ponownie tabelę sprzętu i wiele pustych linii, mimo że ROM pozostawił kursor w dolnym wierszu statusu. Każda kolejna linia przewija VRAM. Profil CPU przed wejściem do jądra przypisał około 28,3% cykli do `console_next_row`. Poprawka zastępuje zduplikowaną tabelę krótkim ` OK` w istniejącym wierszu. Informacje o sprzęcie pozostają w POST i jądrze, komunikaty błędów ładowania zostają zachowane.
4. **Wywołanie funkcji geometrii przy każdym zapisie czyszczenia.** `ui/console.c`, `console_clear()`, obliczała limit przez `shifter_frame_bytes()` wewnątrz warunku pętli. W 800×600×8 oznaczało to około 120000 wywołań przy jednym czyszczeniu. Limit jest teraz obliczany raz. Profil przypisał około 7,0% cykli do czyszczenia oraz 6,1% do funkcji rozmiaru ramki; to udziały w profilu przed jądrem, nie obiecane przyspieszenie końcowe.

## Błędy naprawione razem z wydajnością

- `kvideo_damage()` przeliczała piksele na słowa 16-bitowe zawsze jak dla mono: 16 pikseli na słowo. W 8 bpp jedno słowo odpowiada dwóm pikselom. Po uruchomieniu aktualizacji częściowych stary kod kopiował niewłaściwy fragment wiersza. Poprawka dobiera przesunięcie do formatu; test obejmuje wszystkie 800 pozycji x, dolną prawą krawędź i ochronę bajtów poza ramką.
- W `kernel_console.c` początek pola nazwy modułu był wyznaczany dopiero po dopełnieniu wiersza spacjami, czyli na kolumnę 80. Kolejne nazwy modułów nie mieściły się w polu. Pozycja jest teraz zapamiętywana przed dopełnieniem. Sprawdzane są nagłówek, nazwa modułu i usunięcie końcówki dłuższej nazwy.

## Metoda pomiaru

Lokalny zmodyfikowany Hatari, Mega ST, 68030 32 MHz, 4 MB ST-RAM, 64 MB TT-RAM, TF536, IDE, VOFA/ET4000, fast-forward wyłączony, tryb bez okna. Obraz dysku skopiowano z projektu. Konfiguracja BIOS testowego obrazu ustawiała jawnie IDE 0, opóźnienie boot 1 s i wymagany tryb wideo. Nie zmieniano obrazu używanego przez użytkownika.

Punkty pomiarowe to wejście do `kernel_main` i `kgfx_enter_desktop`, ustalone z pliku ELF danej wersji, nie ze starych zakodowanych adresów. Breakpointy zapisują zrzuty ekranu, a skrypt rejestruje czas ich pojawienia się co 0,1 s. **Wejście do `kgfx_enter_desktop` nie jest końcem malowania pulpitu ani testem reakcji myszy.** Wyniki służą porównaniu wersji w tej konfiguracji. Nie należy utożsamiać ich bezpośrednio z czasem 45–55 s z wcześniejszego dziennika ani z czasem na fizycznym Atari.

Profilowanie CPU wykonano osobno; jego czasy ścienne nie wchodzą do porównania, bo profiler zwiększa narzut. Udziały cykli wyliczono z adresów instrukcji i symboli ROM. Wariant mono tej wersji tabel VOFA faktycznie raportuje 640×400, mimo historycznego opisu 640×480.

## Zmierzone wyniki

| Wersja kolorowa | Wejście do jądra | Wejście w pulpit |
|---|---:|---:|
| Przed poprawkami, próba 1 | 14.13 s | 23.91 s |
| Przed poprawkami, próba 2 | 13.86 s | 23.46 s |
| Przed poprawkami, próba 3 | 14.01 s | 23.60 s |
| Po komplecie poprawek, próba 1 | 9.59 s | 12.32 s |
| Po komplecie poprawek, próba 2 | 8.46 s | 11.16 s |
| Po komplecie poprawek, próba 3 | 8.55 s | 11.27 s |

Mediana czasu do wejścia w pulpit: **23.60 → 11.27 s**, czyli około **52% krócej**. Zakres po poprawkach: 11,16–12,32 s. Liczby dotyczą powyższego punktu pomiarowego, nie gotowości całego pulpitu.

Sama poprawka ścieżki jądra, przed zmianą ROM/BOOT.SYS, dała w pojedynczej próbie 16,08 s. Pełny komplet usuwa również znaczną część kosztu loadera. Pojedyncze próby mono dały 9,40 s przed zmianami i 9,38 s po komplecie poprawek. Nie jest to seria pozwalająca ilościowo ocenić regresję mono.

## Walidacja

Pełny `python3 tools/build_atos2.py` po poprawkach zakończył się powodzeniem: audyt ABI, 43 testy Pythona oraz zestawy C, w tym działający już po zmianach DeepSeeka `check-video`. Do głównej bramki dodano test cache kolorowego terminala. Testy grafiki i konsoli uruchomiono z AddressSanitizerem i UndefinedBehaviorSanitizerem.

Przed poprawką odtworzono błąd cache znaków i błąd aktualizacji częściowej 8 bpp. Po poprawce sprawdzono również ponowne pełne rysowanie po unieważnieniu cache, pomijanie transferu do tego samego VRAM oraz pełny transfer po przełączeniu źródła. Na dodatkowym zrzucie wykonanym osiem sekund po wejściu w pulpit kolorowy widać tło, menu FILE/OPTIONS i pasek aplikacji; zachowano 800×600×256. Dodatkowa kontrola mono pokazała czarny zrzut końcowy zarówno w wersji bazowej, jak i poprawionej. Nie przypisuję tego poprawce, ale oznacza to, że ten test bez okna nie potwierdza poprawności końcowego pulpitu mono. Przed integracją należy sprawdzić również zwykłe uruchomienie z oknem i myszą. Nie potwierdzono działania na fizycznym sprzęcie.

## Integracja przez DeepSeeka

Pakiet obejmuje dziewięć plików: pięć plików implementacji, Makefile, dwa istniejące testy i nowy `tests/test_terminal_chunky.c`. `manifest.json` identyfikuje źródła przed i po zmianach. Najpierw porównać bieżące pliki i wykonać `git apply --check ATOS-wydajnosc.patch`; dopiero potem nałożyć poprawkę.

Następnie zbudować przez `python3 tools/build_atos2.py` i wykonać próby na osobnym obrazie. **Pełny efekt wymaga aktualizacji ROM, BOOT.SYS i KERNEL.SYS. Samo podmienienie KERNEL.SYS pozostawi koszt przewijania i czyszczenia w starym loaderze/ROM.** Nie zastępować aktywnego obrazu pod działającym Hatari. Skrypt `bench_atos_boot.py` modyfikuje konfigurację BIOS wskazanego testowego obrazu, dlatego uruchamiać go wyłącznie na kopii projektu, przez zmienną `ATOS_BENCH_PROJECT`.

Kolejny etap optymalizacji powinien wynikać z pomiarów: w profilu istotny pozostaje czas wykrywania IDE. Nie zmniejszałem timeoutów, nie powiększałem cache dysku i nie wyłączałem testów integralności. Oddzielnie trzeba zmierzyć czas do zakończenia pierwszego rysowania pulpitu oraz płynność przeciągania okien i obsługi wejścia.
