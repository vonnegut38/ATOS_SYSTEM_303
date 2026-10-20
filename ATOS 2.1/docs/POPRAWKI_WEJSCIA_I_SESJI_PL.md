# ATOS 2.0 — poprawki wejścia i izolacji sesji

Aktualizacja 24.09.2026, po zgłoszeniu samoczynnego zamykania COMMAND.APP.
Projekt: `/Users/xxx/Documents/Codex/ATOS 2.0`.
Ten raport uzupełnia i w zakresie opisanych poprawek zastępuje poprzedni stan etapu sesji. Oryginalnego commander-v10 nie zmieniano.

## Przyczyny i zmiany

**Ruch myszy jako Escape/F10.** `kernel_user_keyboard_event` po pustej kolejce klawiatury czytał bezpośrednio kolejny surowy bajt IKBD. Tym samym zabierał INPUT.SYS współrzędne myszy, także 0x01 i 0x44. Test produkcyjnej funkcji odtworzył błąd przed poprawką (SIGABRT na asercji braku klawisza). Usunięto ten odczyt awaryjny: wyłącznym właścicielem surowego strumienia jest INPUT.SYS. Pusta kolejka oznacza brak zdarzenia klawiatury, nie awarię sterownika. Dotyczy to także nieobecnego INPUT.SYS — ten profil nie oferuje wtedy surowego zastępstwa klawiatury. Shell wymaga gotowej usługi INPUT.

**Kliknięcie przenoszone na późniejszą pozycję.** Agregacja pakietów pamiętała przycisk, ale sumowała dalszy ruch. Dodatkowo przepełniona skrzynka GUI przepisywała pozycję starszego kliknięcia. Teraz przekazywany jest jeden kompletny pakiet myszy w kolejności, a bit naciśnięcia oznacza przejście przycisku, nie każdy pakiet z trzymanym przyciskiem. Oczekujące zdarzenie zachowuje swoją pozycję do przekazania aplikacji. Pełna skrzynka wstrzymuje pobieranie wejścia zamiast przepisywać wcześniejsze kliknięcie. Przeciąganie rozpoczęte w innej sesji jest przechwytywane do zwolnienia przycisku.

**Wspólny stan odbioru wejścia.** PID był per sesja, ale tryb ciągłego odbioru, opóźnienie startowe i licznik odrzuceń były globalne. Zostały rozdzielone na sesje. Operacje GUI 45/47 oraz 46/48 identyfikują sesję klienta IPC. Idempotentne zwolnienie nieistniejącego właściciela nie zeruje aktywnego właściciela innej sesji. Zakończenie/ubicie procesu usuwa tylko jego stan; wyjście usługi SYSTEM.GUI zwalnia wszystkie jej sesje.

**Ramki aplikacji w tle.** Autoryzacja GFX sprawdza właściciela sesji klienta, a nie tylko widocznej sesji. `kgfx_terminal_submit` zachowuje najnowsze komórki także w tle, bez zmiany widocznego ekranu. Wejście i wyjście aplikacji w tle nie przełącza ekranu pierwszoplanowego.

**Zatrzymany zegar SYSINFO.** API 58 i raport korzystały z `kernel_wall_ticks` należącego do TOS; w natywnym profilu nie był inkrementowany. Korzystają teraz z istniejącej usługi zegara przez `kernel_user_clock()` (50 Hz). SYSINFO zakończyło pomiary CPU, ST-RAM i odczytu dysku z czasami ok. 2 s na etap. TT-RAM zostało prawidłowo oznaczone jako niedostępne w tym profilu.

**Instalator.** Usunięto podwójną blokadę flock+lockf, która na macOS powodowała fałszywe odmowy nawet po zamknięciu emulatora. Używana jest blokada flock zgodna z lokalnym źródłem Hatari. Instalacja przy zamkniętym obrazie przeszła, a osobno potwierdzono zajętą blokadę podczas pracy Hatari. Skrypt aktualizuje teraz również SYSINFO.APP w SYSTEM/APP.

## Dowody i testy

- `python3 tools/build_atos2.py --install`: PASS; log `work/final-session-check.log`.
- `gmake check`: PASS, zestawy C oraz 19 testów Python.
- Dodatkowe testy terminala, konsoli, sesji i Commandera: PASS (ASan/UBSan).
- Nowe testy: `test_keyboard_framing.py`, `test_gui_input_backpressure.py`, `test_gui_session_owners.py`.
- Rozszerzone testy: `test_mouse_edges.py`, `test_sessions.c`, `test_session_redraw.py`, `test_sysinfo_api.py`.
- Negatywna reprodukcja starego odczytu klawiatury: `work/keyboard-before.log`.
- Odczyt plików bezpośrednio z FAT obrazu potwierdził zgodność byte-for-byte KERNEL.SYS, COMMAND.APP i SYSINFO.APP z build/system.

## Runtime Hatari

Hatari 2.6.1, STE+68030, 16 MHz, 4 MB ST-RAM, Shifter mono 640×400, własny bios_mste.bin, HDD atos-v2.img. Konfiguracja: atos2.cfg. Kod BIOS-u nie był zmieniany tym etapem.

Potwierdzono wizualnie:

1. Trzy jednoczesne Commandery, różne zaznaczenia i aktywne panele; seria 30 skrótów przełączania z kontrolą stanu po każdych 10 (przed końcową poprawką zegara i właścicieli).
2. Dwie instancje Commandera + SYSINFO w trzeciej sesji na końcowym obrazie.
3. Uruchomienie pomiarów SYSINFO i opuszczenie jego sesji; po powrocie pełne wyniki CPU, ST-RAM i odczytu dysku, z niezerowymi czasami. Nie był wykonywany benchmark zapisu dysku.
4. Zakończenie Commandera w tts1, zachowane SYSINFO w tts3, ponowne uruchomienie Commandera w tts1 i reakcja drugiego Commandera w tts2.
5. Zachowanie ekranów i responsywności po kolejnych seriach skrótów; na końcowym obrazie szybkie serie nie zawsze dawały liczbę przełączeń równą liczbie wysłanych kombinacji. Nie należy opisywać ich jako dowodu bezstratnego odbioru każdego skrótu. Pojedyncze przełączenia potwierdzano osobno.

Na koniec pozostawiono: Commander tts1, Commander tts2 (prawy panel), SYSINFO tts3 (wyniki wykonanych testów, otwarta strona dysku). Widoczna jest tts1. Przełączanie: Shift+Alt+F10. F10 bez modyfikatorów oraz Escape w Commanderze celowo kończą aplikację.

## Dalsze prace / DeepSeek

Nie traktować tego jako pełnej kwalifikacji stabilności systemu.

- Ręcznie sprawdzić naturalny ruch myszy, przeciąganie i wybór „Nowy” w menu sesji. Automatyka macOS miała problemy z fokusem i pozycją kursora oraz wyborem pliku konfiguracji. Testy regresji dowodzą naprawy opisanych dróg błędów, ale nie zastępują długiego testu fizycznej myszy.
- Doprecyzować zachowanie szybkich kolejnych skrótów. `apply_request` czeka na opróżnienie wcześniejszych klawiszy, a kolejne żądania mogą zastępować poprzednie. Zarejestrować zdarzenia make/break i odróżnić scalenie żądań od braków w automatyce hosta. Nie usuwać zabezpieczenia przed autorepeat bez testu.
- Test długotrwały, przeciążenie kolejek INPUT, ubijanie w tle, ponowne utworzenie zamkniętego tts2/tts3 i odzysk pamięci. W tym etapie testowano ponowne uruchomienie aplikacji, nie pełny cykl zamknięcia i odtworzenia terminala w emulatorze.
- Pozostają wcześniejsze B3 (ramki wyjątków/SR 030), kontrakty FAT/VOFA oraz pełne wydzielenie TOS. Ten etap nie wdraża ochrony MMU ani nowego desktopu.
- Podwójny komunikat ukończenia/promptu nadal widoczny; wymaga uporządkowania, nie był maskowany.

Zachować te regresje i wykonywać zmiany pojedynczo. Budować `python3 tools/build_atos2.py`; instalować `--install` dopiero po zamknięciu instancji używającej obrazu. Nie zastępować parsera INPUT.SYS kolejnym surowym czytnikiem IKBD.

## Status 2026-09-27 — regresja „Nowy” i COPY

- Menu sesji **„[+] Nowy”** otwiera **lewy** przycisk (IKBD koduje lewy jako
  bit 1; `kernel_sessions.c:handle_mouse` testuje `press&2`).
  `tests/test_sessions.c` pokrywa otwarcie, wybór wiersza, Escape i izolację
  GUI — test jest teraz uruchamiany przez `gmake check` (`check-sessions`);
  wcześniej istniał, ale nie był wołany.
- Pojedynczego lewego kliku w Hatari nie da się zautomatyzować: `hatari-event`
  udostępnia tylko `doubleclick`/`rightdown`/`rightup`, a double-click otwiera
  menu i od razu je zamyka (drugi down przy `y<16`). Pełne potwierdzenie
  fizyczną myszą zostaje po stronie użytkownika.
- Przy okazji: `kfat_copy_path` **nie nadpisuje** już celu (zwraca
  `-FAT_WRITE_EXISTS`, shell: `ALREADY EXISTS`), zgodnie z kontraktem
  `kernel_fat.h` i Commanderem; testy FAT znów działają i są w `gmake check`.
