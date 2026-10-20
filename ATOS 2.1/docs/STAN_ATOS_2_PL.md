> Aktualizacja po zgłoszeniu samoczynnego wyjścia: patrz [POPRAWKI_WEJSCIA_I_SESJI_PL.md](POPRAWKI_WEJSCIA_I_SESJI_PL.md). Poniższy opis dokumentuje wcześniejszy etap.

# ATOS 2.0 — przekazanie po etapie sesji tekstowych

Stan: 24.09.2026. Katalog projektu: `/Users/xxx/Documents/Codex/ATOS 2.0`.
Oryginał `commander-v10` pozostał bez zmian. Dokumenty w `docs/audit/` są historycznym audytem fazy 0; niniejszy plik opisuje późniejszą implementację.

## Co działa i co zmieniono

1. Natywny rozruch z własnego BIOS-u `build/bios_mste.bin` i osobnego HDD `work/atos-v2.img`. BIOS.CFG ma `auto_tos=0`. Usunięto wymuszony autostart GEM, przejście do tts3 i inicjalizację desktopowego compositora z profilu startowego. Od 27.09.2026 domyślny `TOS=0` nie linkuje persony TOS (−23 888 B text, −27 076 B BSS); `TOS=1` przywraca zgodność, a `TOS.SYS` jako moduł odrzucono (patrz `TEXT-SYSTEM-BACKLOG.md` §3).
2. Shell korzysta z callbacków `runtime_handoff`, przekazanych do `kernel_load_acsi`. To istotna poprawka: wcześniej API modułów zachowywało funkcje BIOS-u, przez co tekst shella omijał bufory `kconsole` i znikał po przełączeniu sesji.
3. Konsola pracuje w 80×25. Naprawiono odczyt poza buforem w `term_scroll`. Wszystkie operacje terminala używają stałego kroku 80 komórek; mniejsza geometria zmienia obszar aktywny, nie układ bufora. Czyszczenie zeruje również nieaktywny obszar znakami spacji.
4. Prezentacja tekstowej aplikacji przez opcode 19 trafia do pełnego ekranu, z pominięciem powierzchni okna WM. Kernel zachowuje 3 niezależne ramki po 2000 komórek (łącznie 12 KB). Przełączenie sesji odtwarza ostatnią ramkę i unieważnia cache renderera; nie wymaga kolejnego zdarzenia klawiatury w aplikacji. Wyjście z drugiej aplikacji nie blokuje odtworzenia pierwszej.
5. Nakładka numeru sesji jest rysowana także nad Commanderem. Menu „Nowy” jest widoczne; automatyczna próba wyboru myszą nie potwierdziła utworzenia sesji. Traktować jako otwarty test/usterkę do ręcznej weryfikacji, nie jako zaliczone kryterium.
6. Uaktualniono atrapy testów kursora, nakładki, wybudzania i konsoli oraz rozmiar bufora testu SysInfo względem rzeczywistego ABI. Nie wyłączano sanitizerów ani kontroli granic.

## Uruchomienie i odbiór

W Hatari wybrać **File → Load Config… → atos2.cfg**, potem **Reset**. Ten sposób został wykonany i zweryfikowany wizualnie. Parametry: Hatari 2.6.1, STE + 68030, 16 MHz, 4 MB ST-RAM, mono Shifter 640×400, bez MMU. ET4000/VOFA jest nieobecne, kernel przechodzi na Shifter. Nie jest to test prawdziwego Atari ani kwalifikacja wszystkich wariantów 030.

Alternatywny launcher z terminala: `./run-atos2.sh`. Ma jawne ścieżki do własnego ROM-u, HDD i konfiguracji. W środowisku roboczym Codexa bezpośredni start procesu GUI kończył się kodem 134, dlatego runtime zweryfikowano przez interfejs macOS. Sam launcher nie ma potwierdzonego udanego uruchomienia w tej sesji.

Powtarzalny test klawiaturą:

1. W świeżym shellu wpisać `tts2`, wykonać `pwd`, następnie `tts1`.
2. W tts1 uruchomić `run command.app`.
3. Zmienić zaznaczenie i aktywny panel (strzałki, Tab).
4. **Shift+Alt+F10** przełącza między utworzonymi sesjami. W tts2 można wykonać `cd system`, `pwd`.
5. Ponownie **Shift+Alt+F10** — Commander zachowuje panel, zaznaczenie i ramkę.
6. **F10 bez modyfikatorów** kończy Commandera. `tts3` tworzy trzeci shell; `exit` zamyka niepierwszą sesję, o ile nie ma procesów.

Na końcu prac pozostawiono otwarte Hatari 2.6.1: Commander w tts1 i gotowy shell tts2. Innej, istniejącej instancji Hatari ET4000 nie używano do tych testów.

## Weryfikacja

- `gmake -j4 all`: PASS, własny BIOS i aplikacje przebudowane. Ostrzeżenie linkera o segmentach RWX nadal występuje; ten etap nie wdraża ochrony pamięci.
- `gmake check`: PASS, podstawowe zestawy C oraz 16 testów Python.
- `gmake check-terminal check-console check-sessions check-commander`: PASS, ASan/UBSan.
- Nowy `tests/test_session_redraw.py`: wykonuje rzeczywiste funkcje prezentacji i przełączania z produkcyjnego pliku wraz z rasteryzatorem; sprawdza 100 powrotów między ramkami, odtworzenie po wyjściu drugiej aplikacji, spójność cache i znaczniki granic ekranu.
- Runtime Hatari: start BIOS/kernel/usług, utworzenie tts2, historia i katalog shella, Commander → shell → Commander, dwa Commandery z różnymi zaznaczeniami, wyjście z drugiego i powrót do pierwszego — potwierdzone wizualnie. Dwa Commandery sprawdzono przed ostatnią zmianą włączającą nakładkę nad aplikacją; na końcowym obrazie powtórzono Commander → shell → Commander.
- `tools/build_atos2.py --install` podczas otwartego HDD odmawia instalacji — blokada obrazu sprawdzona.
- Log końcowej kompilacji i testów: `work/final-check.log`. Starsze `work/check.log` i `work/text-tests.log` dotyczą poprzednich przebiegów.

## Budowanie i instalacja

`python3 tools/build_atos2.py` buduje i wykonuje testy. Aby zaktualizować HDD, najpierw zamknąć tę instancję Hatari, potem uruchomić `python3 tools/build_atos2.py --install`.
Skrypt blokuje obraz przed modyfikacją i aktualizuje także `SYSTEM/APP/COMMAND.APP` — samo podmienienie `SYSTEM/COMMAND.APP` nie wystarcza, ponieważ shell wyszukuje program również w `SYSTEM/APP`.
Nie uruchamiać odziedziczonych celów `image-v10*`: dotyczą starszych nazw obrazów i ścieżek.

## Następne zadania dla DeepSeeka

Przekazać cały katalog ATOS 2.0 wraz z tym plikiem. Pracować małymi zmianami, po każdej zachować zielone testy i powtórzyć powyższy scenariusz. Pierwsze zadanie: ręcznie sprawdzić i ewentualnie naprawić wybór „Nowy” myszą (IKBD, agregacja zdarzeń, przechwycenie kliknięcia), dodając reprodukcję do `tests/test_sessions.c`.

Pozostała część fundamentu nie jest zakończona:

- B3 z audytu: parser ramek wyjątków 68030 i przechwytywanie SR wymagają poprawki oraz testów rzeczywistych błędów. Zielone `gmake check` nie rozstrzyga poprawności starych testów parsera.
- B2: odrębne testy FAT overwrite i wyrównania VOFA nadal mają nierozstrzygnięte kontrakty opisane w audycie. Nie były naprawiane ani kwalifikowane tym etapem.
- B4: odciąć nieaktywne wywołania i zależności TOS od natywnej ścieżki; obecnie wyłączony jest autostart/WM, nie cały podsystem zgodności.
- Zbadać podwójne komunikaty ukończenia/promptu po niektórych poleceniach i zamknięciu aplikacji; występowały w runtime, nie niszczyły zachowanej ramki.
- Dłuższy test obciążeniowy sesji, background output, ubijanie procesów, ponowne otwieranie slotu, trzeci terminal i restart. Hostowy test 100 przełączeń nie zastępuje długiego testu emulatora.
- Ustalić drugi profil sprzętowy i dopiero potem kwalifikować prawdziwe 030/TT-RAM/MMU oraz VOFA.

Nie nazywać tego wydaniem w pełni stabilnym. Jest to działający, przetestowany etap niezależnych sesji tekstowych. Nowy desktop, MagiC/EmuTOS jako rdzeń, port KolibriOS, VM i ochrona pamięci pozostają poza tą zmianą.