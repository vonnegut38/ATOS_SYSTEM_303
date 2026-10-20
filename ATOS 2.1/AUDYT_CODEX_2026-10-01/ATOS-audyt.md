# Audyt postępów ATOS 2.1

Stan sprawdzony 1 października 2026 r. Projekt: `/Users/xxx/Documents/Deepseek_test/ATOS 2.1`.

ATOS wyraźnie rozwinął się w działający prototyp systemu z modularnym jądrem, obsługą dysków, sesjami, pulpitem i własnymi aplikacjami. Kompilacja oraz standardowa bramka testów przechodzą. Nie jest to jednak wystarczający dowód stabilności: dodatkowe testy wykazały błędy obsługi dysku i granic buforów grafiki, a pomijany zestaw testów wideo ulega awarii. Największy pożytek przyniesie teraz stabilizacja i pomiary, przed kolejnym zwiększaniem zakresu funkcji.

## Zakres i wiarygodność oceny

Przeanalizowałem dokumentację przekazania, najnowszy dziennik prac, proces budowania oraz wybrane ścieżki BIOS, pamięci, dysków, POST i kompozytora. Zbudowałem osobną kopię bez katalogów build, obrazów dysków i zrzutów ekranu. Nie zatrzymywałem emulatora, nie instalowałem systemu na obrazie i nie zmieniałem katalogu, w którym pracuje DeepSeek.

Ocena nie jest audytem każdej instrukcji systemu ani certyfikacją działania na Atari. Nie uruchamiałem nowej sesji Hatari ani testów na fizycznym sprzęcie. Czasy startu podawane niżej pochodzą z dziennika DeepSeeka; nie zostały niezależnie zmierzone. Katalog ATOS 2.1 nie zawiera plików śledzonych przez nadrzędne repozytorium Git, dlatego nie da się wiarygodnie przypisać zmian do autorów i commitów ani wyliczyć procentu ukończenia projektu.

## Co rzeczywiście jest zrobione

| Obszar | Dowód i ocena |
|---|---|
| Budowanie | `python3 tools/build_atos2.py` kończy się kodem 0 na kopii źródeł. Audyt ABI asemblera przechodzi. |
| Testy | 43 testy Pythona oraz zestawy C uruchamiane przez skrypt przechodzą. To liczba testów Pythona, a nie wszystkich sprawdzeń projektu. |
| Zapis ACSI | `hw/acsi.c` zawiera rzeczywistą sekwencję WRITE(6). Informacja o no-op w START-TUTAJ jest nieaktualna. Poprawności elektrycznej i timingów nie potwierdza ten audyt. |
| POST i ROM | `ui/post.c:96–102` wywołuje test RAM i checksum ROM; implementacje są w `diag/tests.c`. START-TUTAJ błędnie przedstawia je jako brakujące. |
| Grafika | Istnieją ścieżki mono i 8 bpp, szybsze wypełnianie/kopiowanie, paleta, obsługa pulpitu, okien i sesji. |
| Wydajność | Dziennik opisuje spadek czasu do pulpitu z około 72–75 s do 45–55 s. To użyteczny sygnał, lecz bez powtarzalnego benchmarku nie dowodzi trwałego przyspieszenia na wszystkich konfiguracjach. |
| Kompatybilność TOS | Domyślnie `TOS=0` w Makefile. README testów kompatybilności nadal deklaruje brak planowanych programów PRG. Zielony build nie oznacza pełnej zgodności TOS/GEM. |

## Potwierdzone błędy i luki

### 1. Dostęp poza buforem w kopiowaniu grafiki

Priorytet wysoki. `system/wm_blit.c:97` i dalsze linie: szybkie kopiowanie sprawdza początek prostokąta, ale nie ogranicza całej szerokości do szerokości źródła i celu. Test kopiowania z x=63 z bufora szerokości 64, przy szerokości prostokąta 8, kończy się `AddressSanitizer: global-buffer-overflow`. Funkcja wypełnienia również nie przycina wszystkich krawędzi; dla ujemnego y tworzy nieprawidłowy adres wiersza.

To potwierdzony błąd funkcji dla prostokątów wychodzących poza powierzchnię. Nie twierdzę, że odtworzyłem konkretną awarię myszy z dziennika: część wywołań kompozytora wcześniej przycina obszary.

Poprawka w pakiecie: przycinanie wypełnienia i kopiowania, zachowanie przesunięcia źródła przy ujemnym x celu, sprawdzanie wskaźników danych. Dodatkowo kopiowanie może dojść krótkim prefiksem bajtowym do wspólnego wyrównania i kontynuować 32-bitowo. Nie zmienia to funkcji w bezpieczny odpowiednik memmove dla nakładających się obszarów.

### 2. Fałszywy sukces przełączenia dysku

Priorytet wysoki. `system/kernel_block.c:78–87`: `kblock_select()` zwraca sukces dla tego samego identyfikatora także przed inicjalizacją. Po nieudanym przełączeniu identyfikator nowego dysku zostaje zapisany, a `ready` pozostaje z poprzedniego urządzenia. Ponowienie wyboru tego samego urządzenia omija odczyt sprawdzający i zgłasza sukces.

Obie sytuacje odtworzono testami asercji. Poprawka wyłącza gotowość przed zmianą, przywraca ją dopiero po udanym odczycie i pozwala ponowić próbę po błędzie. Licznik odczytów obejmuje teraz również próbę wyboru urządzenia. Osobnym przyszłym zadaniem jest propagowanie błędów przez wywołania ignorujące wynik `kblock_select`, np. w obsłudze FAT i modułów.

### 3. Test eksmisji cache faktycznie nie działał

Priorytet średni. `tests/test_block_cache.c` definiuje N=512, a sprawdzenie eksmisji wykonuje tylko wtedy, gdy rozmiar cache jest nie większy niż N. Aktualny cache ma 1024 sektory, więc blok jest pomijany. Dziennik przedstawia przepisanie testu jako walidację nowej geometrii, lecz aktualny kod jej nie sprawdza.

Poprawka zwiększa dysk testowy do 2048 sektorów i zastępuje ciche pomijanie asercją warunku. Test musi wykonać eksmisję albo jawnie zgłosić, że środowisko testowe jest za małe.

### 4. Osobna bramka wideo jest zepsuta

Priorytet wysoki dla procesu weryfikacji. `gmake check-video` nie jest częścią głównego skryptu. Kończy się SIGSEGV przy zapisie do 0x00C00000 w `vofa_et4000_init`. `tests/test_vofa.c` dostarcza atrapę pamięci przez callbacki, ale `system/vofa_et4000.c:236` czyści VRAM bezpośrednio wskaźnikiem sprzętowym, omijając tę abstrakcję.

Wymagana dalsza poprawka: oddzielić szybkie czyszczenie sprzętowe od testowalnego interfejsu dostępu do VRAM, zweryfikować oba warianty i włączyć `check-video` do bramki. Tego nie maskowałem warunkiem pomijającym test. Zestaw zatrzymał się na pierwszym programie; kolejne testy tego celu nie zostały wykonane. Pakiet poprawek nie naprawia tego problemu.

### 5. Cache może zabrać pół megabajta ST RAM

Priorytet średni, istotny dla małych konfiguracji. `system/kernel_block.c:73` prosi o 512 KB z preferencją TT-RAM. `system/kernel_memory.c:53–54` w razie braku odpowiedniego miejsca w TT-RAM przechodzi na ST-RAM. Komentarz „cache lives in TT-RAM” nie jest gwarancją. Jeśli alokacja się powiedzie, zostaje mniej pamięci na aplikacje i grafikę; jeśli się nie powiedzie, cache jest pomijany.

Dziennik sekcja 5.99 sam podaje brak skrócenia startu po powiększeniu cache. Zalecam rozmiar zależny od dostępnej pamięci i zachowanie rezerwy ST-RAM. Kandydaci do porównania: 0/32/64/128/512 KB. Wariant 512 KB powinien mieć uzasadnienie pomiarem trafień i czasu pracy, nie samą obecnością dużej pamięci w emulatorze.

### 6. Dokumentacja myli diagnostykę z testem całej pamięci

`diag_ram_pattern()` testuje bufor 256 bajtów. Nie sprawdza 4 MB ST-RAM i 64 MB TT-RAM, wbrew hipotezie zapisanej w sekcji 5.99 dziennika. `diag_ttram()` wywołuje odrębną detekcję pamięci — jej czas trzeba mierzyć osobno. Skracanie tego małego testu na podstawie błędnej hipotezy nie jest uzasadnioną optymalizacją.

Bufor nie jest volatile; warto doprecyzować wymagania testu sprzętowego, wpływ cache CPU i gwarancję rzeczywistych odczytów. W aktualnym kodzie maszynowym odczyty są obecne, więc nie zgłaszam, że kompilator usunął cały test.

### 7. Długoterminowa trwałość zapisów wymaga sprawdzenia

`hw/block_device.c:38` implementuje `block_flush()` jako sprawdzenie obecności IDE lub bezwarunkowy sukces dla pozostałych identyfikatorów. Nie wysyła polecenia opróżnienia cache urządzenia. Trzeba określić kontrakt tej funkcji i wymagania dla używanych nośników. To luka kontraktu trwałości, nie dowód utraty danych na aktualnym dysku.

## Plan optymalizacji

1. Najpierw połączyć poprawki bezpieczeństwa pamięci i stanu dysku z testami regresji. Naprawić bramkę wideo. Dopiero potem opierać kolejne decyzje na zielonym wyniku testów.
2. Dodać znaczniki czasu po detekcji RAM, inicjalizacji wideo, POST, skanowaniu urządzeń, odczycie BOOT/KERNEL, ładowaniu modułów i pierwszym pełnym rysowaniu pulpitu. Porównywać medianę oraz rozrzut co najmniej pięciu identycznych startów; zapisać wersję emulatora, CPU, RAM, tryb wideo i hash ROM.
3. Mierzyć koszt aktualizacji grafiki: bajty przesłane do VRAM, powierzchnię uszkodzoną, liczbę pełnych przerysowań, kopiowań i opóźnienie reakcji na wejście. W 800×600×8 pełna ramka ma 480000 bajtów. Ograniczenie niepotrzebnych pełnych aktualizacji może dać więcej niż dalsze strojenie pojedynczych zapisów.
4. Sprawdzić jednokrotność inicjalizacji trybu karty i palety oraz granicę pomiędzy ekranem startowym a pulpitem. `desktop_wm.c` przejmuje aktywny tryb; sam nie wymusza domyślnie 800×600×256. Zadanie „boot 640×480, pulpit 800×600×256” wymaga osobnego testu przejścia, a nie tylko zrzutu finalnego ekranu.
5. Zmierzyć koszty wykrywania nieobecnych urządzeń. IDE wykonuje do czterech prób na jednostkę wraz z opóźnieniami. Ewentualna szybka ścieżka znanej konfiguracji powinna zachować pełne wykrywanie w recovery. Nie skracać timeoutów bez próby z realnym nośnikiem.
6. Dobierać cache do budżetu RAM. Zbierać hit/miss, rzeczywiste transfery i największy wolny blok. Testować konfiguracje bez TT-RAM, a nie tylko 4 MB + 64 MB.
7. Rozważyć profil diagnostyczny i zwykły start, ale przenosić self-testy dopiero po pomiarze ich kosztu. Kontrole integralności plików, granic pamięci i błędów I/O powinny pozostać obowiązkowe.
8. Uporządkować proces: objąć ATOS 2.1 własną historią wersji, robić małe zmiany z opisem testu, skonsolidować aktualny status i oddzielić historyczny dziennik od listy otwartych problemów. Rozdzielać duże moduły stopniowo; sama liczba plików nie przyspiesza systemu.

## Wyniki walidacji poprawek

- Bazowy `python3 tools/build_atos2.py`: PASS, kod 0, w tym audyt ABI i 43 testy Pythona.
- Nowe reproduktory na starym kodzie: FAIL dla wyboru dysku przed init, FAIL dla ponowienia nieudanego wyboru, błąd AddressSanitizera dla kopiowania poza źródłem.
- Poprawiony `check-block-cache`: PASS z cache, bez cache i dla operacji FAT; po 10000 mieszanych transferów w obu wariantach cache.
- Poprawiony `check-wm-blit`: PASS z AddressSanitizer/UndefinedBehaviorSanitizer; dodatkowo 4000 porównań wypełnienia/kopiowania z referencją pikselową w mono i 8 bpp.
- Pełny skrypt budowania po poprawkach: PASS, kod 0; ponownie 43 testy Pythona i zestawy C skryptu.
- Dodatkowy `check-video`: FAIL, niezależny problem opisany wyżej.
- `git apply --check` dla patcha na oryginalnym katalogu: PASS w chwili zakończenia audytu. To sprawdzenie możliwości nałożenia, nie instalacja.

Linker emituje ostrzeżenia o segmentach RWX. W tym systemie freestanding samo ostrzeżenie nie przesądza o podatności, ale faktyczny model ochrony pamięci wymaga osobnej oceny. Nie mierzyłem przyspieszenia poprawionego kopiowania na Atari ani emulatorze.

## Pakiet dla DeepSeeka

`ATOS-poprawki.patch` obejmuje cztery pliki: `system/kernel_block.c`, `system/wm_blit.c` i ich dwa testy. `ATOS-manifest.json` zawiera hashe przed i po. `ATOS-dowody-testow.zip` zawiera logi oraz wersje źródeł przed i po zmianach.

Poprawki są przygotowane i sprawdzone w kopii audytowej. Nie zostały nałożone na aktywny projekt. Przed ich integracją DeepSeek powinien ponownie sprawdzić zgodność patcha z bieżącymi zmianami, następnie uruchomić skrypt budowania bez instalacji i testy emulatora na osobnym obrazie. Niezależnie pozostają do rozwiązania: awaria check-video, polityka pamięci cache, propagowanie błędów wyboru dysku i walidacja sprzętowa.
