# Mono Commander 4 — częściowe odrysowanie i kosz

W Hatari wczytaj `commander-mono-v4.cfg` i zatwierdź Reset. Konfiguracja wskazuje osobny obraz `atari_commander_mono_v4.img` oraz dotychczasowy `commander-bios.bin`. STE, 68000, 8 MHz, 4 MiB, mono 640×400. Uruchomienie: `run command.app`. Potrzebny jest nowy kernel i nowa aplikacja z obrazu v4.

## F8 i TRASH

- **F8 poza koszem:** po potwierdzeniu przenosi wybrany plik lub cały katalog wraz z zawartością do kosza na tym samym woluminie. Nie kopiuje zawartości plików i nie zwalnia ich klastrów.
- Kosz powstaje automatycznie w katalogu głównym: `\TRASH`.
- Każde usunięcie otrzymuje osobny podkatalog, np. `\TRASH\T0000001\MOJKAT`. Dzięki temu zachowane są oryginalne nazwy i można usunąć kilka plików o tej samej nazwie bez nadpisania wcześniejszych.
- **F6 przywraca:** otwórz w jednym panelu podkatalog kosza (np. `\TRASH\T0000001`), zaznacz właściwy plik/katalog, a w drugim panelu otwórz miejsce docelowe. F6 wyświetla potwierdzenie i przenosi element do drugiego panelu. Nie nadpisuje istniejącego celu. Nie zapisujemy jeszcze oryginalnej ścieżki ani daty usunięcia; miejsce przywrócenia wybiera użytkownik.
- **F8 wewnątrz kosza:** dialog „TRWALE usun wraz z zawartoscia?”; dopiero Y lub przycisk Tak uruchamia trwałe usunięcie wybranego drzewa. Można zaznaczyć cały podkatalog T0000001 w TRASH, aby usunąć dany wpis kosza w całości. Esc/N anuluje.
- Samego katalogu głównego TRASH nie można usunąć przez F8. Brak zbiorczego przycisku „opróżnij cały kosz”; wpisy usuwa się pojedynczo.
- Przeniesienie do kosza nie zwiększa wolnego miejsca. Miejsce zwalnia dopiero trwałe usunięcie. Utworzenie kosza/podkatalogu potrzebuje wolnego klastra i wpisu katalogowego; na całkowicie pełnym dysku przeniesienie może się nie udać.

## Spójność i ograniczenia

Przeniesienie zmienia wpisy katalogowe i odnośnik `..` przenoszonego katalogu. Rozmiary, atrybuty i daty wpisu pozostają zachowane. Sprawdzana jest hierarchia celu, aby nie przenieść katalogu do siebie ani do własnego podkatalogu. Operacja korzysta z dotychczasowego dziennika cofania metadanych i synchronicznego zapisu z cache v3.

Tworzenie TRASH i pustego podkatalogu jest oddzielną transakcją od przeniesienia. Jeśli późniejsze przeniesienie nie powiedzie się, źródło pozostaje na miejscu, a pusty podkatalog może pozostać w koszu. Można go później skasować przez F8.

Przed trwałym usuwaniem wykonywane jest sprawdzenie całego drzewa (m.in. atrybutów tylko do odczytu, długości ścieżek, łańcuchów i limitu przejść). Potem elementy usuwane są od liści, po jednej transakcji na element. **Błąd zapisu podczas trwałego usuwania może pozostawić częściowo opróżnione drzewo.** Wcześniej usunięte elementy nie wracają; pozostałe pozostają dostępne w koszu. Ponowienie może dokończyć operację. Nie ma trwałego dziennika odpornego na utratę zasilania; nie jest to pełna transakcyjność całego drzewa.

Obsługiwane są nazwy 8.3. Ścieżki syscalli/aplikacji nadal mają maksymalnie 63 znaki; resolver dopuszcza do 16 składników, a trwałe usuwanie jest ograniczone do 4096 przejść. Zbyt głębokie/długie drzewo zostanie odrzucone przed rozpoczęciem trwałego usuwania; można przywrócić jego katalog nadrzędny do krótszej ścieżki. Katalogi nie są jeszcze automatycznie rozszerzane po zapełnieniu wpisów. Dotychczasowy limit kopiowania plików 1 MiB nie dotyczy przenoszenia do kosza, ponieważ treść nie jest kopiowana. Nie dodano semantyki przenoszenia otwartych plików znanej z POSIX; istniejące uchwyty odczytu w tym systemie pamiętają ścieżkę.

## Odświeżanie

Wejście do katalogu lub przewinięcie strony odświeża zawartość zmienionego panelu. Pełny panel 16 wierszy wymaga 20 poleceń rysowania zamiast 56 dla obu paneli i całej ramki. Strzałki i Tab nadal korzystają z krótkich aktualizacji zaznaczenia. Pełne odrysowanie pozostaje dla pierwszego widoku, komunikatów i zamykania dialogów.

Renderer wyznacza prostokąt zmian, zaokrągla go do słów obrazu i kopiuje do bufora roboczego tylko ten obszar. Gotowy fragment wraca do widocznego ekranu. Adres widocznego ekranu pozostaje stały, więc nie przełączamy Shiftera na nowy bufor po każdej paczce. Dla jednego pełnego panelu kopiowane jest łącznie 23 520 bajtów (w obie strony), zamiast wcześniejszych 32 000 bajtów całego ekranu. Dla małych zmian różnica jest większa. Pełne odrysowanie wymaga teraz dwóch pełnych kopii, co jest świadomym kosztem utrzymania stabilnego ekranu. Nie dodano synchronizacji z VBL; ta zmiana nie gwarantuje braku rozdarć obrazu. Przy braku pamięci na bufor roboczy pozostaje bezpośrednie rysowanie.

Zachowano cache 64 KiB z v3. Nie podajemy procentowego przyspieszenia pełnej zmiany katalogu przed pomiarem w emulatorze.

## Weryfikacja

Przeszły:

- `gmake check-trash`: rzeczywiste FAT + cache z częściowym błędem DMA na każdym etapie przenoszenia, tworzenia kosza i trwałego usuwania; odtwarzanie metadanych; przywracanie; kolizje; ochrona korzenia kosza i przed cyklami; odrzucenie drzewa z plikiem tylko do odczytu przed zapisem. Audyt obu kopii FAT, własności wszystkich klastrów i odnośników `..` po sukcesach oraz błędach.
- `gmake check-commander`: rzeczywiste procedury aplikacji, F8 do kosza, F8 trwałe, F6 przywracanie, ochrona TRASH. Programowy obraz po częściowym odrysowaniu jest porównywany z pełnym odrysowaniem dla tych samych danych, również przy łączeniu zmiany panelu i zaznaczenia.
- `gmake check-gui-damage`: obcinanie prostokątów, tekst wielowierszowy, kopiowanie słów dla 1/2/4 płaszczyzn i kontrola granic. Okna typu GUI_WINDOW mają konserwatywnie pełnoekranowy obszar zmian, żeby zachować starszy renderer.
- `gmake check-block-cache check check-gui-apps`: regresja cache/FAT i wejścia, nagłówki aplikacji, sumy kontrolne oraz wywołania niezależne od adresu załadowania. Kompilacja dla 68000.

Audyt obrazu po instalacji: 95 plików, zgodne i czyste kopie FAT, bez nowych osieroconych klastrów lub krzyżowych łańcuchów. Względem v3 zmieniono KERNEL.SYS i COMMAND.APP; zainstalowano nowe COMTEST.APP i TRASHTST.APP w SYSTEM/APP. Starszy COMTEST w SYSTEM/TEST pozostaje; polecenie z pełną ścieżką jednoznacznie wybiera nowy test.

### Hatari — oczekuje na końcową próbę

Automatyczne sterowanie oknem wyboru konfiguracji ponownie utknęło (przycisk Open nieaktywny, wybór pliku niespójny z kliknięciem). Nie uznajemy jeszcze v4 za przetestowaną w Hatari.

Po wczytaniu CFG:

1. `run trashtst.app` — tylko na świeżym obrazie bez TRASH i V4PROBE. Tworzy własne drzewo V4PROBE/SUB/COPY.SYS, przenosi do kosza, porównuje kopię z kernelem, przywraca, ponownie przenosi i usuwa trwale. Oczekiwany napis: `TRASHTST: ALL PASS`. Pozostawia pusty TRASH. Gdy kosz już istnieje, test odmawia startu (FAIL 1), aby nie dotykać istniejących danych.
2. `run \system\app\comtest.app` — oczekiwany `COMTEST: MOUSE/PAGE/DRAW PASS`; myszy dotyczy syntetyczna ścieżka KMOU.
3. `run command.app` — wejście/wyjście z katalogu, F7 dla katalogu testowego, F8 do kosza, F6 przywrócenie i F8 trwałe wewnątrz kosza.

## Źródła

Aktualna kopia robocza: `work/commander-v4`. Pełne źródła: `atari_commander_v4_sources.tar.gz`. Osobne binaria i logi: `commander-v4/`. Wersje v2/v3 oraz eksperymenty DeepSeeka pozostają niezmienione.

Nowe syscalle: 53 — przenieś A0 do TRASH; 54 — przenieś A0 do A1 (bez nadpisania, ten sam zamontowany wolumin); 55 — usuń trwale drzewo A0, wyłącznie pod TRASH. Walidacja ścieżek wykorzystuje tę samą kontrolę pamięci procesu co istniejące operacje FAT. Starszy syscall 51 zachowuje zachowanie usuwania pliku/pustego katalogu dla zgodności aplikacji; F8 już go nie wywołuje.
