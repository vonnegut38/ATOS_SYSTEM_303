# SYSINFO v9 — konfiguracja Atari i proste benchmarki

Uruchomienie: `run sysinfo.app`. Nowa aplikacja jest w `C:\SYSTEM\APP\SYSINFO.APP` i zajmuje 8888 bajtów na dysku. Używa naszego terminala 80×25: czarne tło, białe litery i ramki, odwrócone aktywne zakładki. Działa jako proces użytkownika i może kontynuować test w tle po przełączeniu konsoli. Nadal obowiązuje jeden wspólny desktop aplikacji; przed startem trzeba zamknąć Commandera.

## Pliki

- `atari_sysinfo_v9.img` — osobna kopia obrazu v8 z nowym `SYSTEM/KERNEL.SYS` oraz dodanym `SYSTEM/APP/SYSINFO.APP`.
- `sysinfo-bios-v9.bin` — ROM 256 KiB z rozszerzonym raportem sprzętu, sprawdzony w konfiguracji STE/68000 w Hatari. To obraz ROM-u emulatora, nie automatyczny instalator do pamięci flash Atari.
- `00-sysinfo-v9.cfg` — gotowa konfiguracja Hatari: STE, 68000, 8 MHz, 4 MiB, mono, nowy ROM i dysk. Ścieżki są absolutne dla tego komputera.
- `sysinfo-v9/KERNEL.SYS`, `sysinfo-v9/SYSINFO.APP` — osobne pliki do aktualizacji kopii systemu v8. Samo skopiowanie aplikacji do starszego kernela nie wystarcza.
- `atari_sysinfo_v9_sources.tar.gz` — pełne źródła bez katalogu build; katalog roboczy: `work/sysinfo-v9`.
- `sysinfo-v9/` — logi testów i audytu; `SHA256-sysinfo-v9.txt` — sumy artefaktów.

Starszy program testowy `SYSTEM/TEST/SYSINFO.APP` został zachowany bez zmian. Zwykłe `run sysinfo.app` wybiera nową aplikację z `SYSTEM/APP`; pełna ścieżka pozwala wybrać konkretny plik.

## Obsługa

| Klawisz | Funkcja |
|---|---|
| F1 | Sprzęt, pamięć i obraz |
| F2 | Dysk startowy ACSI i jego partycje |
| F3 | Wyniki benchmarków |
| F5 | Wszystkie dostępne testy |
| F6 | CPU |
| F7 | ST-RAM, następnie TT-RAM |
| F8 | Odczyt dysku |
| F9 | Odświeżenie raportu |
| Esc | Przerwanie testu; poza pomiarem wyjście |
| F10 | Wyjście |
| PgUp / PgDn | Kolejna strona partycji |

Zakładki i pozycje dolnego paska można kliknąć. Esc pozwala wyjść także wtedy, gdy macOS przechwytuje F10. Alt+Shift+F10 nadal służy do przełączania aktywnych konsol; dodatkową konsolę otwiera polecenie `tts2` lub `tts3`.

## Znaczenie danych

- ST-RAM i TT-RAM: całkowita pamięć wykryta podczas startu, wolna pamięć oraz największy wolny blok zarządcy pamięci.
- CPU i FPU: wykrycie przez BIOS. Taktowanie pochodzi z BIOS-u, a dla podstawowego ST/Mega ST/STE z 68000 jest oznaczone jako nominalne 8 MHz. Nie ma jeszcze niezależnego pomiaru zegara ani wykrywania wszystkich akceleratorów. TT/Falcon również korzystają z informacji modelu w BIOS-ie; overclocking wymaga osobnej weryfikacji.
- Aktywne wyjście: Shifter albo uruchomiony sterownik VOFA/ET4000. Oddzielnie pokazana jest karta i adapter wykryte przez BIOS, w tym rodzina ET4000/W32 i ATI Mach32/Mach64. Wykrycie Mach32 nie oznacza działającego sterownika wyświetlania dla Mach32 — takiego sterownika ten etap nie dodaje.
- Rozdzielczość wyjścia jest oddzielona od obszaru aplikacji: Shifter mono 640×400; VOFA 640×480 z logicznym widokiem 640×400.
- Dysk: urządzenie startowe ACSI, port DMA i jego ID. Model z INQUIRY, pojemność fizyczna z READ CAPACITY(10); brak obsługi tych poleceń daje jawnie „nieustalona”. Nie utożsamiamy sumy partycji z pojemnością całego urządzenia. Skan wszystkich kontrolerów IDE/SCSI i pozostałych dysków nie jest tu zaimplementowany. Adapter ACSI–SD może podawać własną nazwę zamiast modelu nośnika.
- Starszy ROM nie przekazujący nowych pól daje komunikat o braku informacji o karcie/FPU. Do pełnego raportu użyj dołączonego BIOS-u v9.

## Benchmarki

Każdy test trwa około dwóch sekund czasu licznika Atari, z ograniczeniem ilości pracy. Licznik MFP ma około 50 Hz / 20 ms i jest inkrementowany również w przerwaniach podczas obsługi wywołań systemowych. Wyniki dotyczą całej aplikacji działającej pod naszym systemem: uwzględniają planistę, wywołania systemowe, obsługę zdarzeń i odświeżanie ekranu. Zmiana obciążenia, widoczności aplikacji albo ustawień emulatora zmienia wyniki. Nie są to pomiary maksymalnej przepustowości sprzętu.

- CPU: pętla arytmetyki 32-bitowej, wynik w pętlach/s, nie MIPS ani wyliczone MHz.
- RAM: bufor 32 KiB, odczyt i zapis; wynik sumuje bajty odczytane i zapisane. Testuje tylko własną zaalokowaną pamięć. TT-RAM jest pomijany, jeśli go nie ma lub nie można uzyskać bloku w tym obszarze. Na CPU z cache wynik może obejmować wpływ cache.
- Dysk: sekwencyjne odczyty po 8 sektorów z partycji startowej; po dojściu do końca powrót na początek. Pomija cache blokowy naszego OS, ale obejmuje kopiowanie przez bufor DMA w ST-RAM. Nie wyłącza cache adaptera ani hosta emulatora. Nie zapisuje danych i nie czyści istniejącego cache OS.

## Testy

Przeszły: kompilacja ROM-ów, kernela i aplikacji; kontrole relokowalności GUI; testy video, konsoli, sesji, terminala i Commandera; test SYSINFO z ASan/UBSan; testy cache oraz FAT; 11 testów Python, w tym walidacja wskaźników, wyrównania, zakresu partycji i przepełnienia adresu dla nowych wywołań diagnostycznych.

Log `app-cache-tests.log` zachowuje także pierwszą próbę użycia nieistniejącego celu `check-cache`; właściwy `check-block-cache` został następnie wykonany i przeszedł. Nie jest to błąd testowanego kodu.

W Hatari sprawdzono start nowego ROM-u, raport konfiguracji, poprawną nazwę i dokładną liczbę sektorów dysku, komplet dostępnych benchmarków, pominięcie TT-RAM, Esc, wyjście z kodem 0 i dokończenie testów w tle podczas używania drugiej konsoli. Szczegóły: `sysinfo-v9/hatari-tests.md`. Obsługa kliknięć ma test hosta; fizycznej myszy na Atari nie testowano.

Fizyczna VOFA/ET4000, Mach32 oraz TT-RAM nie zostały sprawdzone na sprzęcie w tym etapie. Kontynuacja wyświetlania na ET4000 bazuje na niezmienionym sterowniku v8 i jego dotychczasowych ograniczeniach. Obraz nadal ma `SYSTEM/VIDEO.CFG = VOFA`; w Hatari bez karty poprawnie wraca na Shifter.
