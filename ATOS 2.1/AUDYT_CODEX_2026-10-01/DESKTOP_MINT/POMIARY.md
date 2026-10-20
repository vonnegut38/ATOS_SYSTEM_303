# Pomiary i walidacja

Badano odizolowaną kopię ATOS, własny obraz dysku oraz lokalny `hatari-tf536` 2.6.1-devel. Konfiguracja: 68030 32 MHz, 4 MB ST-RAM, 64 MB TT-RAM, VOFA/ET4000, IDE, 800×600×8, bez fast-forward, headless. ROM pozostawał ten sam; podmieniano wyłącznie testowy KERNEL.SYS. Bazowy kernel pochodzi z kopii źródeł sprzed równoległej zmiany `surface_fill` o 22:23. Poprawiony obejmuje cały patch, nie tylko cache.

Czas to zegar hosta od uruchomienia procesu Hatari. Breakpointy `:once :trace` zapisują zrzuty przy wejściu do `kernel_main`, `kgfx_enter_desktop` i pierwszego `kernel_wm_app_dirty`. Ostatni punkt następuje po `kgfx_draw_batch` na powierzchni aplikacji, **przed końcowym skomponowaniem i przesłaniem obrazu do VRAM**. Nie jest to czas pełnej gotowości pulpitu ani opóźnienie interakcji. Odczyt plików co 0,1 s oraz zapis screenshotów wnoszą narzut.

| Wersja / przebieg | Kernel [s] | Wejście w pulpit [s] | Pierwszy batch aplikacji zakończony [s] | Od wejścia w pulpit do batcha [s] |
|---|---:|---:|---:|---:|
| Bazowa 1 | 9,71 | 11,17 | 19,41 | 8,24 |
| Bazowa 2 (`before-3`) | 9,99 | 11,55 | 17,69 | 6,14 |
| Bazowa 3 (`before-4`) | 9,96 | 11,87 | 18,17 | 6,30 |
| Poprawiona 1 | 9,12 | 9,95 | 10,46 | 0,51 |
| Poprawiona 2 | 9,26 | 10,09 | 10,61 | 0,52 |
| Poprawiona 3 | 10,13 | 11,07 | 11,59 | 0,52 |
| **Mediana bazowa** | **9,96** | **11,55** | **18,17** | **6,30** |
| **Mediana poprawiona** | **9,26** | **10,09** | **10,61** | **0,52** |

Do zakończenia pierwszego batcha: mediana 18,17 → 10,61 s, około 42% krócej. Sam odcinek inicjalizacji/rysowania od wejścia w pulpit: 6,30 → 0,52 s, około 12 razy krócej. **Nie oznacza to 12 razy szybszej całej grafiki.** Różnicy czasu do `kernel_main` nie należy przypisywać patchowi, bo zmienione procedury jeszcze wtedy nie działały; pokazuje ona również rozrzut warunków hosta.

Jeden dodatkowy przebieg bazowy (`ready-before-2`) został zakończony sygnałem 9 przed pierwszym punktem pomiarowym. Przyczyna nie została ustalona; przebieg wykluczono i zastąpiono `ready-before-4`. Surowy wynik pozostawiono w evidence. Próby profilera CPU w tej kompilacji Hatari zwracały zero aktywnych adresów — nie użyto ich do wniosków o udziale funkcji w czasie CPU.

Dodatkowe uruchomienie z BIOS video-mode=2 dotarło do pierwszego batcha, ale zapisane ustawienia pulpitu przełączyły końcowy obraz na 800×600×8. Nie traktować go jako potwierdzonego testu graficznego mono. Hostowe testy mono przechodzą, lecz pełny test interakcji w mono na docelowym sprzęcie pozostaje do wykonania.

## Testy poprawki

- Pełne `python3 tools/build_atos2.py`: budowa m68k, audyt ABI assemblera, wszystkie wskazane przez projekt zestawy testów — zakończone kodem 0. W logu m.in. 43 testy Python oraz testy C terminala, grafiki mono, damage, WM, języków i konfiguracji. Ostrzeżenia linkera RWX nadal występują w istniejącej konfiguracji.
- Nowy `test_chunky_raster.c`, kompilowany z `-O2`, AddressSanitizer i UndefinedBehaviorSanitizer: PASS. Sprawdza 256 masek wiersza znaku, zmianę kolorów, tekst przezroczysty/opaque, różne stride i wyrównania, wypełnienia oraz kopie nakładających się obszarów we wszystkich kierunkach. Porównuje pełną pamięć z prostym wzorcem, włącznie z bajtami ochronnymi.
- `patch --dry-run -p1` na osobnej kopii najnowszych trzech zmienianych plików projektu: PASS po uwzględnieniu zmiany DeepSeek.
- Kontrola deasemblacji: zapis CACR=9 w gałęzi 020/030; bazowa pętla inicjalizacji powierzchni rzeczywiście wywoływała funkcję rozmiaru przy każdym bajcie.
- Testy nowych helperów nie zastępują testu wszystkich operacji GUI. Pozostają do weryfikacji na sprzęcie: DMA, dynamiczne ładowanie wielu aplikacji, przełączanie rozdzielczości, kursor/zasłanianie i dłuższa stabilność.

W folderze `evidence` znajdują się surowe JSON-y, zrzuty przed/po i log budowy. Benchmark celowo nie zmienia aktywnego obrazu użytkownika; do odtworzenia wymaga osobnej kopii wskazanej przez ATOS_BENCH_PROJECT.

Próba automatycznego otwierania menu i wysyłania strzałek/Escape zakończyła się obrazem otwartego menu, zarówno przed, jak i po poprawkach. Potwierdza uruchomienie pulpitu i wyświetlenie menu, ale nie potwierdza obsłużenia całej sekwencji wejścia. Nie wyznaczono z niej czasu odpowiedzi ani wyniku PASS dla interakcji; wymaga dalszej kontroli sposobu wstrzykiwania klawiszy i obsługi zdarzeń.
