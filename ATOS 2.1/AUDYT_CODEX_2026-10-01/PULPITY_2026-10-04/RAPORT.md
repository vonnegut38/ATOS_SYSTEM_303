# ATOS 2.1 przełączanie pulpitów i czarny ekran

Audyt aktualnej wersji z 4 października 2026. Zinterpretowano zgłoszenie jako pusty drugi pulpit tworzony przez górną ikonę i Add Desk. Znaleziono dwa błędy, które odtworzono testami rzeczywistych funkcji. Przygotowano kandydat poprawki w oddzielnej kopii, bez podmiany źródeł i obrazu użytkownika.

## Najważniejsza przyczyna

`wm_rebuild_clip()` oblicza widoczne fragmenty tylko dla `wm.view_session`. Okna innych sesji dostają pustą listę. Podczas przełączenia `kernel_wm_frame_tick()` ustawia nowy `view_session` dopiero tuż przed kompozycją, lecz nie przebudowuje list. `kernel_wm_refresh_bar()` odmalowuje wyłącznie pasek, bez odtworzenia pulpitu.

`wm_recompose()` wybiera już nową sesję, ale korzysta z fragmentów obliczonych dla starej. Nowy desktop może istnieć i mieć poprawną powierzchnię, a mimo to nie trafi na ekran. Brakuje także gwarancji pełnego odświeżenia przy samym przełączeniu. Zależnie od wcześniejszych operacji efekt może być czarnym obszarem lub starym obrazem.

Test tworzy dwie zachowane powierzchnie należące do sesji 0 i 1. Po przebudowie list dla pierwszej sesji wykonuje sekwencję przełączeń jak w badanym kodzie. Przed poprawką druga sesja nie rysuje żadnej powierzchni (wynik 0 zamiast PID 2). Po poprawce każda zmiana 0→1→0→1→0 wybiera prawidłową powierzchnię.

To dowód konkretnego błędu mechanizmu; nie jest jeszcze potwierdzeniem, że wyjaśnia wszystkie przypadki czarnego ekranu na obrazie użytkownika.

## Druga przyczyna niepoprawny moment uruchomienia desktopu

Add Desk zapisuje żądanie przełączenia i niezależną flagę autostartu. `apply_request()` odkłada przełączenie, gdy `kernel_input_pending()` zwraca prawdę. Natomiast `ksessions_take_autostart()` bezwarunkowo zużywa flagę. Główna pętla może wtedy uruchomić DESKTOP.APP, zanim zmieni się sesja. Nowy proces może trafić do starej sesji i zostać odrzucony jako drugi właściciel tego samego pulpitu. Po późniejszym przełączeniu nowa sesja pozostaje bez desktopu.

Test z wciąż oczekującym `request=2`: przed poprawką autostart zwraca 1 za wcześnie, a po faktycznym przełączeniu już 0. Po poprawce zwraca odpowiednio 0 i 1.

## Dostarczony patch

`pulpity-kandydat.patch` obejmuje trzy pliki:

- `kernel_wm.c`: synchronizacja sesji kompozytora przed hit-testami i odrysowaniem, przebudowa clip, pełne damage i wymuszenie kompozycji przy zmianie sesji. Odświeżenie paska po zmianie sesji wykonuje też synchronizację. Pełne odświeżenie występuje przy zmianie pulpitu, nie przy każdym ruchu myszy.
- `kernel_sessions.c`: autostart ma zapamiętaną sesję docelową; czeka, aż nie ma oczekującego przełączenia, i nie uruchamia procesu w niewłaściwej sesji. Jeśli przełączenie nie doszło do skutku lub zostało zastąpione innym, start jest anulowany. W interfejsie nadal potrzebny jest komunikat o takim niepowodzeniu.
- `wm_core.c`: jawna inicjalizacja nowego pola `view_session` i właścicieli sesji w slotach okien. Nie należy polegać na przypadkowym wyzerowaniu pamięci lokalnego stanu WM.

Nowe testy i istniejący `test_wm_core.c` przeszły; KERNEL.SYS skompilował się na m68k. Logi dołączono. Nie przeprowadzono w tej turze próby całego systemu w Hatari i klikania górnej ikony. Patch jest kandydatem do testu integracyjnego, nie gotowym dowodem usunięcia wszystkich błędów wielu pulpitów. Nie dołączono binarnego kernela do automatycznej instalacji.

## Rozbudowa nie jest jeszcze spójna we wszystkich warstwach

1. **Operacje aplikacji w tle są pomijane.** W kernel_exception.c po obsłudze rejestracji desktopu jest ogólna bramka odrzucająca dalsze operacje GFX klienta z innej sesji, zwracając sukces. Dotyczy to także potencjalnie poprawnego rysowania do jego prywatnej powierzchni i geometrii okna. Aplikacja może uznać repaint za wykonany, choć obraz nie powstał. Docelowo rozdzielić zapis do własnej powierzchni od prezentacji na aktywnym ekranie. Nie usuwać całej bramki bez audytu globalnych operacji palety, trybu i nakładek.
2. **Pozostają globalne stany pojedynczego pulpitu.** `wm_desktop_session` jest jedną liczbą, a widoczność konsoli, popupy, nakładka i przechwycenie drag/resize nie są pełnym stanem per sesja. Przełączenie musi anulować przechwycenie i nakładki starej sesji albo je bezpiecznie zapisać. Nie wystarczy dodać `session` do okien.
3. **Dekoracje i lista zadań wymagają tego samego filtrowania co treść.** Core używa `win_shown()`, lecz nie wszystkie pętle kernel_wm.c korzystają z tego samego warunku. Należy przejrzeć rysowanie tytułów/gadżetów, wyliczanie zasłaniania dekoracji, task list, cykliczną zmianę aktywnego okna i fokus. W przeciwnym razie niewidoczne okna innego pulpitu mogą wpływać na aktualny ekran.
4. **Limit zasobów jest globalny.** Są trzy sesje, sześć slotów procesów użytkownika i sześć powierzchni aplikacji oraz osiem slotów WM. Każdy kolejny DESKTOP.APP zużywa zasoby. Add Desk nie pokazuje błędu uruchomienia — wynik trafia przede wszystkim do liczników diagnostycznych. Należy wyświetlić błąd i pozwolić ponowić start, zamiast pozostawiać pustą sesję. Samo zwiększenie limitów wymaga kontroli pamięci i innych stałych tablic.
5. **Tożsamość sesji musi pozostać stabilna.** Desktop powinien być uruchamiany z jawnym identyfikatorem docelowej sesji, a nie wyłącznie przez domniemanie „bieżąca sesja”. Dostarczona poprawka ogranicza obecną pomyłkę czasową; docelowy interfejs spawn powinien potwierdzać także sesję procesu i jego gotowość do prezentacji.

## Zalecana organizacja

Przełączenie powinno być jedną kontrolowaną operacją: zakończ przechwycenie starego pulpitu → zdejmij nakładki/kursor → wybierz sesję i jej fokus → odbuduj widoczność → skomponuj zachowane powierzchnie → pokaż pasek i kursor. Bufory nie muszą być kasowane ani aplikacje uruchamiane od nowa przy każdym powrocie.

Add Desk potrzebuje stanów: żądanie, przełączono, uruchamianie, desktop gotowy albo błąd. Sam ustawiony bit w masce sesji nie oznacza, że jest już pulpit do pokazania. Do czasu pierwszej gotowej klatki można pokazać komunikat o uruchamianiu zamiast pustego ekranu.

## Instrukcja dla DeepSeek

1. Sprawdzić source-manifest.json i zastosować patch do zgodnej kopii. Nie nadpisywać nowszych zmian.
2. Uruchomić `python3 test_desks.py /ścieżka/do/projektu`, dotychczasowe testy WM i przebudować kernel.
3. Na oddzielnym obrazie: Add Desk przy bezruchu i przy ciągłym ruchu myszy, powroty 1↔2↔3, tworzenie przy zajętych slotach, okna na każdym pulpicie, zamknięcie aplikacji w tle, aktywna plasma oraz przełączenie podczas menu i przeciągania.
4. Sprawdzić licznik udanych spawnów, rzeczywistą sesję PID nowego desktopu, wynik jego rejestracji, session okna, view_session i clip_count. Rozdzielić awarię startu od awarii kompozycji.
5. Dopiero po tych testach podmienić właściwy kernel. Potwierdzić, że powrót na pierwszy pulpit zachowuje jego okna, a drugi pokazuje własne i nie przejmuje fokusu pierwszego.
