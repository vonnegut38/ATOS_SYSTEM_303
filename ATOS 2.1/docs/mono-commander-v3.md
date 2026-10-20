# Mono Commander 3 — cache odczytu dysku

Konfiguracja Hatari: `commander-mono-v3.cfg` (STE, 68000, 8 MHz, 4 MiB, mono). Wskazuje osobny obraz `atari_commander_mono_v3.img` i dotychczasowy `commander-bios.bin`. Po przeniesieniu plików popraw ścieżki w CFG. Uruchomienie menedżera: `run command.app`.

## Zmiana i bezpieczeństwo

- Cache 64 KiB: 128 sektorów po 512 bajtów, 32 zestawy po 4 sektory z wyborem najdawniej używanego. Zastępuje jeden bufor 4 KiB, który przy żądaniach pojedynczych sektorów przechowywał zwykle tylko ostatni sektor.
- Nadal osobny bufor DMA 4 KiB w ST-RAM; dane cache preferują TT-RAM, gdy jest dostępna. Znaczniki zajmują dodatkowo 768 bajtów na 68000. Brak pamięci na cache nie blokuje uruchomienia ani dostępu do dysku.
- Każdy zapis jest synchroniczny: sukces oznacza potwierdzenie zapisu przez sterownik. Nie ma odroczonych zapisów ani niezapisanych sektorów w cache.
- Przed zapisem unieważniane są dotychczasowe kopie objętych nim sektorów. Nowe kopie powstają dopiero po sukcesie. Błąd odczytu lub zapisu opróżnia cały cache, również gdy DMA zdążyło częściowo zmienić dysk.
- Ponowna inicjalizacja urządzenia usuwa stare wpisy. Kopiowanie, mkdir, kasowanie i przywracanie metadanych po błędzie korzystają z tej samej warstwy zapisu.
- Nie dodano odczytu z wyprzedzeniem. Odczyt obejmuje wyłącznie żądane sektory; nie zwiększa opóźnienia pierwszego wejścia do katalogu spekulacyjnym transferem.

Przejrzano eksperymentalny cache DeepSeeka w `Documents/Deepseek_test`. Nowa implementacja jest własna, mniejsza i nie przejmuje błędu publikowania nowych danych w cache przed potwierdzeniem zapisu. Zachowuje dotychczasowy sterownik sprzętowego zapisu ACSI.

To nie zapewnia atomowości FAT przy utracie zasilania w środku operacji. Nadal obowiązują ograniczenia wersji 2, w tym brak trwałego dziennika transakcji, nazwy 8.3 i limit kopiowania 1 MiB. Cache nie ma niezapisanych danych, ale nie usuwa wcześniejszych ograniczeń systemu plików. Wymiana nośnika poza systemem podczas pracy nie jest obsługiwana.

## Wyniki testów

`gmake check-block-cache` (ASan/UBSan): konflikty wpisów i LRU, zakresy odczytu, zapis natychmiastowy, częściowo wykonany zapis zakończony błędem, nieudany odczyt, ponowna inicjalizacja i zmiana urządzenia, tryb braku pamięci; po 10 000 mieszanych transferów w trybie cache i bez niego.

Rzeczywisty kod FAT połączony z rzeczywistym cache: kopiowanie, tworzenie/usuwanie katalogów, zgodność obu kopii FAT, BPB 512/1024 bajty, brak miejsca, cykle, błędy na każdym etapie zapisu i przywracanie metadanych. W wariancie z cache wymuszone błędy fizycznego zapisu zmieniają pierwsze 127 bajtów sektora przed zwróceniem błędu.

Przeszły również `gmake check check-fat-write check-commander check-gui-apps`. Nowy kernel skompilowano dla 68000. Nagłówki, sumy kontrolne i wywołania niezależne od adresu załadowania aplikacji zostały sprawdzone.

### Porównanie odczytów katalogów

Test na hoście uruchamia rzeczywisty kod FAT i oba warianty warstwy blokowej na tym samym obrazie v2, otwartym tylko do odczytu. Listuje maksymalnie 32 wpisy w katalogach `/`, `/SYSTEM/APP`, `/SYSTEM/TEST`.

| Próba | Wersja 2 | Nowy cache |
|---|---:|---:|
| Pierwsze 3 strony po montowaniu | 8 odczytów sektorów | 6 odczytów sektorów |
| 300 stron po rozgrzaniu cache | 800 odczytów sektorów | 0 odczytów sektorów |
| Suma wyników 300 stron | `97408e2c` | `97408e2c` |

To pomiar liczby wywołań fizycznego odczytu przez warstwę blokową, nie czasu działania w Hatari ani szybkości rysowania. Nie jest gwarancją zerowych odczytów dla większych katalogów lub po wyparciu danych z cache.

Odtworzenie: `gmake bench-directory-cache IMAGE=/pelna/sciezka/atari_commander_mono_v2.img`.

### Hatari — sprawdzone 16.09.2026

Hatari 2.6.1, STE 68000 8 MHz, 4 MiB ST-RAM, bez TT-RAM, mono 640×400. Konfigurację v3 wczytano przez natywne Preferences i wykonano restart.

- `run cachetst.app`: `CACHETST: PASS, TICKS = 63`. Ten sam program i ustawienia na kopii obrazu v2: `CACHETST: PASS, TICKS = 66`. Test obejmuje 300 stron katalogów bez rysowania. To pojedyncze krótkie pomiary zegara gościa; różnica 3 ticków jest niewielka i nie stanowi pomiaru czasu ściennego ani pełnej zmiany katalogu w GUI. Nie należy z niej wyprowadzać ogólnego procentu przyspieszenia systemu.
- `run fatwrite.app`: `FATWRITE: ALL PASS`. Utworzono własny katalog MCPROBE, skopiowano do niego KERNEL.SYS, porównano zawartość bajt po bajcie, sprawdzono odmowę nadpisania i usunięcia niepustego katalogu, usunięto kopię i katalog.
- `run comtest.app`: `COMTEST: MOUSE/PAGE/DRAW PASS`, kod wyjścia 0. Porównanie pojedynczej strony z poprzednim API: 3 / 3 ticki. Test myszy używa syntetycznych komunikatów KMOU, nie fizycznych kliknięć hosta.
- Audyt obrazu po testach zapisu: 93 pliki, obie kopie FAT zgodne i clean, bez nowych osieroconych klastrów i krzyżowych łańcuchów. Oryginalne pliki v2 zachowały sumy kontrolne, z wyjątkiem celowo zmienionego KERNEL.SYS. Dodano CACHETST.APP, dane testowe MCPROBE zostały usunięte.
- Końcowy kernel 70251 bajtów: potwierdzono start z napisem `BLOCK CACHE: 128 SECTORS / ST-RAM`, otwarcie Commandera, wejście do SYSTEM klawiszem Enter i powrót Backspace. Przy pełnym odrysowaniu widoczna była przejściowa czarna klatka; cache nie usuwa tego opóźnienia grafiki. Emulator pozostawiono z otwartym Commanderem.
- Przy próbie wykryto nieaktualny, stały napis startowy „8 SECTORS”. Poprawiono go tak, aby odczytywał rzeczywistą pojemność cache (128 sektorów) lub wskazywał wyłączenie przy braku pamięci. Warstwa cache nie została przy tym zmieniona.

Powłoka potrafi pokazać `PROGRAM EXIT: 0` zanim wypisze buforowany wynik aplikacji. Weryfikacja opierała się na końcowych komunikatach PASS, nie tylko na tym wstępnym napisie. Przy zbyt wczesnym wpisywaniu następnego polecenia część znaków ginęła; polecenie testu GUI powtórzono po ustaniu wyjścia poprzedniej aplikacji. Ten wcześniejszy problem wejścia/wyjścia konsoli pozostaje poza zmianą cache.

## Pliki i źródła

Wersja v2 i eksperymenty DeepSeeka pozostają niezmienione. Aktualne źródła v3 są w `work/cache-v3` obok katalogu `outputs` oraz w `atari_commander_v3_sources.tar.gz`. Pierwotny katalog projektu z 2026-09-03 nie został nadpisany. To z kopii v3 należy kontynuować prace nad cache.

Logi testów: `commander-v3/cache-v3-cache-check.log`, `cache-v3-benchmark.log`, `cache-v3-regression.log`. Oddzielne binaria: `commander-v3/KERNEL.SYS`, `commander-v3/CACHETST.APP`.
