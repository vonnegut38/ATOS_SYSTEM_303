# ATOS 2.1 — uruchamianie aplikacji, 2026-10-01

## Wynik

W badanej kopii nie odtworzyłem wielosekundowego zapętlenia loadera Commandera. Załadowanie i utworzenie procesu zajmuje niewielką część czasu; większość obserwowanego opóźnienia następuje przy inicjalizacji i rysowaniu interfejsu. Natomiast SYSINFO uruchamiane z aktywnego pulpitu ma potwierdzony problem funkcjonalny: przechodzi do raportu tekstowego i kończy działanie, zamiast utworzyć okno. Czekanie na jego okno nic nie da.

Nie jest to twierdzenie, że wszystkie aplikacje działają szybko. Do odtworzenia zgłoszonego długiego czekania nadal potrzebne są nazwa konkretnego programu, sposób uruchomienia i przybliżony czas. Testy opisane niżej dotyczą Commandera i SYSINFO na kontrolowanej konfiguracji.

## Warunki i zgodność wersji

Osobna kopia źródeł i dysku; Hatari 68030 32 MHz, 4 MB ST-RAM, 64 MB TT-RAM, VOFA/ET4000 800×600×256, IDE, bez fast-forward. Poprzednie poprawki cache instrukcji i zerowania powierzchni są już obecne.

Sprawdzono również KERNEL.SYS w aktywnym obrazie użytkownika, wyłącznie odczytem: 128 350 bajtów, SHA-256 `aa29ad1819757933253864e62c24373aef810ff1668f3729e9e233b8e4a3c9ca`. Jest identyczny z aktywnym plikiem build i testowanym kernelem. Nie znaleziono tutaj starego kernela pozostawionego na dysku.

Testowy DESKTOP.APP miał tymczasowe skróty wywołujące istniejące `launch_run` i wstrzykujące zdarzenia myszy przez istniejący interfejs testowy. Nie instalowano tych skrótów w aktywnym systemie. Symulacja dwukliku przechodziła przez normalną obsługę ikon, lecz nie mierzy opóźnienia rzeczywistej myszy/hosta.

## Pomiary

Czas hosta od wysłania bodźca do testowego emulatora, z breakpointami zapisującymi screenshoty. Rozdzielczość odczytu około 50 ms; wysłanie bodźca obejmuje 150 ms oczekiwania narzędzia, dlatego wspólne wartości 0,15–0,16 s nie rozdzielają dokładnie krótkich etapów. To pojedyncze przebiegi diagnostyczne, nie mediany benchmarku.

| Próba | Wejście w loader | Utworzenie procesu | Deklaracja okna | Koniec sekwencji pierwszego malowania |
|---|---:|---:|---:|---:|
| Commander, oryginalna aplikacja, bezpośredni launcher | 0,15 s | 0,16 s | 0,21 s | 0,79 s |
| Commander, eksperymentalne łączenie batchy | 0,15 s | 0,15 s | 0,21 s | 0,80 s |
| Commander, symulowany dwuklik, eksperymentalne batche | 0,31 s | 0,36 s | 0,58 s | 1,23 s |

Ostatni punkt to wejście do `kernel_wm_set_content`, wywoływane przez Commandera po wysłaniu początkowych batchy; nie gwarantuje zakończenia ostatniego flush do VRAM. Zrzuty wykonane po dodatkowych trzech sekundach pokazują kompletne okno Commandera. Dane dla dwukliku pochodzą z wariantu eksperymentalnego, nie należy ich przedstawiać jako pomiaru oryginalnego programu.

Kilka prób przygotowawczych odrzucono: dwa procesy emulatora zakończyły się sygnałem 9 z nieustaloną przyczyną; pierwsza kopia launchera nie znajdowała względnej ścieżki do pliku binarnego; wcześniejsza próba nie wymuszała poprawnej konfiguracji koloru. Nie użyto tych prób do oceny wydajności. Późniejsze testy korzystały z osobnej kopii właściwego pliku wykonywalnego emulatora i jawnego BIOS.CFG w testowym obrazie.

## SYSINFO: problem potwierdzony w działaniu

`system/sysinfo.c:app_main` prosi usługę GUI o wyłączne przejęcie klawiatury przez `rpc(gui,6,...)`. Pulpit już ją posiada. Po odmowie SYSINFO wykonuje `console_report()` i przechodzi do zakończenia programu. Ten program korzysta ze starszego modelu pełnoekranowej aplikacji tekstowej, a nie z okna WM jak Commander.

W pomiarze po wywołaniu launchera: utworzenie procesu, generowanie raportu i wejście do `kconsole_puts_to` nastąpiły do 0,16 s; `kernel_user_end` został osiągnięty przy 0,26 s. Nie powstało okno. To zakończenie działania, nie pętla oczekiwania na ładowanie. Raport trafia do konsoli, która może być schowana za pulpitem.

Naprawa docelowa dla DeepSeek: przenieść SYSINFO do modelu zwykłego okna z własną powierzchnią i zdarzeniami fokusu albo świadomie uruchamiać jego starszy interfejs w odrębnej sesji/widocznej konsoli. Nie odbierać pulpitowi właściciela klawiatury i nie otwierać wszystkich uprawnień globalnego GFX dla dowolnej aplikacji.

## Gdzie pozostaje narzut Commandera i pulpitu

1. `system/ac.c:paint` wysyła osobno tło, toolbar, dwa panele i dolny pasek. Każde żądanie wymaga obsługi usługi GFX i może doprowadzić do kolejnej kompozycji/dekorowania. To wielokrotne malowanie, a nie nieograniczona pętla loadera.
2. `kernel_wm_set_content` zawsze zgłasza pełną ramkę jako damage, również gdy wysokość dokumentu się nie zmieniła. Commander wywołuje to po pełnym malowaniu. Można pominąć unieważnienie przy niezmienionej wysokości i pozycji przewijania; dla zmiany samego suwaka ograniczyć damage do właściwego paska.
3. `kernel_wm_frame_tick` nadal odmalowuje całe dekoracje dotkniętego okna, a pasek zadań po każdej kompozycji. Poprawne przycinanie dekoracji to lepszy następny krok niż przyspieszanie checksumy pliku o kilka kilobajtów.
4. Obsługa ikony po dwukliku wywołuje `icon_activate → launch_run → paint`, po czym wywołuje `paint` ponownie. `launch_run` dodatkowo odświeża stany launcherów, które `paint` odpytuje ponownie. Przed redukcją tych wywołań trzeba zachować poprawne usunięcie zaznaczenia ikony, stan uruchomienia oraz ścieżki błędu/builtin.
5. Rozpoznanie dwukliku używa `hires()` w chwili przetwarzania zdarzenia. Czas zdarzenia nie jest tym samym co czas jego obsłużenia po renderowaniu. Przy kolejce obciążonej rysowaniem warto przenosić znacznik czasu samego wejścia. Nie należy po prostu zwiększać progów, aby zamaskować problem.

## Eksperyment, którego nie zalecam teraz scalać

Przygotowano i skompilowano wariant AC łączący batche, bez przekraczania limitu 80 komend. Test hostowy porównał binarnie cały strumień komend przed/po dla 15 scenariuszy, trzech geometrii i różnej liczby wpisów katalogu, z AddressSanitizer i UndefinedBehaviorSanitizer. Komendy były identyczne: 1246; liczba batchy spadła z 75 do 21.

Jednak czas pierwszego malowania w emulatorze nie poprawił się: około 0,79 kontra 0,80 s. Większa wspólna obwiednia damage może zjadać oszczędność liczby wywołań. Nie dostarczam tego wariantu jako gotowej poprawki wydajnościowej. Jest wyłącznie eksperymentem w kopii roboczej; aktywny AC.APP nie został podmieniony.

## Dalsze ustalenie rzeczywistego długiego czekania

Dla wskazanej przez użytkownika aplikacji zmierzyć kolejno: zdarzenie myszy, `kernel_spawn_user_async`, koniec odczytu/weryfikacji, pierwsze wykonanie procesu, deklarację okna, ostatni batch inicjalizacji i koniec flush. Powtórzyć po zamknięciu i ponownym uruchomieniu oraz przy kilku oknach. Porównać faktycznie używane CPU/MHz, ROM, obraz, driver i miejsce programu na dysku.

Obecne dane nie uzasadniają zmiany schedulera, omijania kontroli obrazu ani wyłączania zabezpieczeń pamięci. Nie zmieniono aktywnych źródeł, programów ani obrazu systemu. Paczka zawiera diagnozę i surowe wyniki, bez patcha przedstawianego jako potwierdzona naprawa.
