# Mono Commander 2 — mono 640×400

Uruchomienie: `run command.app`. W Hatari wczytaj `commander-mono-v2.cfg` z katalogu outputs i zatwierdź restart. Konfiguracja wskazuje `atari_commander_mono_v2.img` i wcześniejszy `commander-bios.bin`; po przeniesieniu zestawu popraw ścieżki w konfiguracji. Obraz zawiera nowy kernel i COMMAND.APP — samo podmienienie aplikacji w starym kernelu nie wystarczy.

## Zmiany

- Czarne tło, białe ramki i litery. Zaznaczony plik ma jasne tło i czarny tekst.
- Jeden odczyt strony katalogu zamiast oddzielnego przeszukiwania od początku dla każdego wiersza. Nowy syscall 52 zwraca do 32 kolejnych wpisów, z walidacją rozmiaru bufora użytkownika.
- Strzałki odrysowują poprzedni i nowy wiersz — 4 polecenia rysowania. Tab wymaga 6 poleceń, wliczając zmianę ramki aktywnego panelu. Wpisywanie nazwy odświeża tylko dialog (8 poleceń). Pełny widok nadal rysowany jest przy zmianie katalogu i zamykaniu dialogu.
- Ruch myszy przesuwa sam kursor, nie odrysowuje paneli. Kolejne zdarzenia ruchu bez przycisków są łączone.
- Kolejka wejściowa zostawia klawisze do późniejszego odebrania, gdy aplikacja jest zajęta, zamiast od razu je odrzucać. Zachowane są osobne znaczniki lewego i prawego kliknięcia także przy połączonym naciśnięciu i puszczeniu.

## Obsługa myszy

- Lewy klik: wybór pliku/katalogu i aktywnego panelu.
- Dwuklik katalogu lub `..`: wejście do katalogu albo powrót wyżej.
- Prawy klik wewnątrz panelu: katalog nadrzędny.
- Kliknięcie PgUp/PgDn pod panelem: poprzednia/następna strona.
- Dolne przyciski: Kopiuj, Katalog, Usuń, Koniec.
- W dialogach można kliknąć Tak/Utwórz lub Anuluj. Nazwę nowego katalogu wpisuje się klawiaturą.

Klawiatura nadal działa: Tab, strzałki, Enter, Backspace, Home, PgUp/PgDn; F5 kopiuje, F7 tworzy katalog, F8 usuwa po potwierdzeniu, F10/Esc wychodzi. Esc anuluje otwarty dialog.

## Zakres

Operacje plikowe zachowują zakres wersji 1: aktualnie zamontowany wolumin, nazwy 8.3, kopiowanie pojedynczych plików do 1 MiB bez nadpisania, usuwanie plików i pustych katalogów. Brak rekursywnego kopiowania i kosza. Zapis nadal synchroniczny. To natywna aplikacja pełnoekranowa; nie jest jeszcze oknem zarządzanym przez MonoDesk. Szczegóły transakcji FAT i ograniczeń odporności na przerwanie zasilania opisano w mono-commander.md.

## Weryfikacja

- `gmake check-commander`: rzeczywiste procedury aplikacji z atrapami syscalli, ASan/UBSan. Sprawdza motyw, liczbę poleceń odświeżania, jeden odczyt strony, wybór myszą, dwuklik, prawy przycisk, przewijanie, przyciski operacji oraz anulowanie. Mutacje w tym teście trafiają do atrapy.
- `gmake check`: test rzeczywistego callbacku syscalla 52 dla 1600 kombinacji pozycji i rozmiaru strony, z kontrolą granic bufora; istniejące testy kursora, ROM i wybudzania.
- `gmake check-fat-write check-mono-font check-gui-apps`: regresja operacji FAT, fontu, nagłówków, sum kontrolnych i wywołań PC-relative.
- Hatari: sprawdzony czarny motyw i zmiany zaznaczenia klawiaturą. Narzędzie sterujące myszą hosta i względne współrzędne Hatari nie dawały zgodnego położenia, więc nie potwierdzono niezależnie pełnej ścieżki fizycznej myszy.
- `run comtest.app`: test natywny w Hatari, z syntetycznymi komunikatami KMOU wprowadzanymi do rzeczywistych procedur aplikacji. Używa rzeczywistego kernela, IPC, grafiki i dysku; porównuje też wynik syscalla 52 z pojedynczymi odczytami 13. Nie tworzy ani nie kasuje danych użytkownika.

Wynik Hatari 2.6.1 / STE 68000 8 MHz / 4 MiB: `COMTEST: MOUSE/PAGE/DRAW PASS`, kod wyjścia 0. Odczyt 17 wpisów nową i starą metodą dał identyczne wyniki. W tym krótkim pomiarze zegara systemowego obie metody zajęły 3 ticki; nie jest to dowód przyspieszenia czasowego na tym katalogu. Potwierdzona jest redukcja liczby przeszukiwań z 17 do 1 i poleceń odświeżania zaznaczenia z pełnego widoku do 4. Nie podajemy pomiaru FPS ani procentowej poprawy czasu reakcji.

Audyt obrazu v2 względem v1: 92 pliki, obie kopie FAT zgodne i clean, bez nowych osieroconych klastrów i krzyżowych łańcuchów. Z istniejących plików zmieniono tylko KERNEL.SYS i COMMAND.APP; dodano COMTEST.APP.
