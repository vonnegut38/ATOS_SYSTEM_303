# ATOS 2.1 — propozycje przyspieszenia przesuwania i zmiany rozmiaru okien

Analiza aktualnych źródeł, 3 października 2026. Zakres: interaktywne przesuwanie i resize, nie skalowanie bitmap zawartości. Nie zmieniono źródeł ani obrazu dysku. Wykonano test zakresu skanowania na rzeczywistej funkcji `kvideo_damage()`. Nie wykonano benchmarku czasu w Hatari lub na Atari.

## Wniosek

Jest duży potencjał przyspieszenia bez zmiany wyglądu. Obecny interfejs już używa lekkiej ramki podczas przesuwania/resize i zatwierdza geometrię po puszczeniu przycisku. Problemem nie jest więc wyłącznie koszt rysowania zawartości: reprezentacja zmian w warstwie wideo zamienia cienką ramkę w skanowanie jej wnętrza. Po zakończeniu przesunięcia system dodatkowo odświeża cały ekran, a aplikacja ponownie rysuje zachowaną zawartość.

Komentarze w kernel_wm.c mieszają opis outline i live move. Aktualna gałąź interaktywna używa XOR-ramki; nie należy oceniać jej kosztu wyłącznie na podstawie komentarza o „live move”.

## 1. Największy pierwszy cel: zachować rozłączne fragmenty w wideo

`outline_video_damage()` zgłasza cztery cienkie krawędzie. Jednak `kvideo_damage()` w kernel_video.c przechowuje tylko jeden zakres `[row_left,row_right)` na wiersz. Lewa i prawa krawędź okna rozszerzają ten zakres na całe wnętrze. `kvideo_flush()` czyta i porównuje wszystkie słowa w tym zakresie, nawet gdy większość nie wymaga zapisu do karty.

Test 800×600×256, ramka 600×400 przy (20,40), następnie (26,44):

| Zakres | Obecny zakres skanowania | Dokładna suma rozłącznych słów obejmujących krawędzie |
|---|---:|---:|
| Jedna ramka | 240 000 B | 2 792 B |
| Stara i nowa ramka | 244 776 B | 5 580 B |

Dla drugiego przypadku zakres skanowania jest 43,87 razy większy. Nie jest to zmierzony wzrost FPS ani ilość zapisów VRAM: shadow ogranicza zapisy, lecz nie eliminuje czytania i porównań wnętrza. Podane bajty opisują zakres bufora obrazu, nie sumę wszystkich odczytów front+shadow. Ramka jest przerywana, więc nawet dokładne prostokąty krawędzi pozostają zachowawcze.

**Propozycja:** kilka rozłącznych przedziałów na wiersz, np. 8, ze scalaniem zachodzących zakresów. W przypadku przepełnienia scalać przedziały o najmniejszym dodatkowym koszcie albo awaryjnie poszerzyć zakres; nigdy nie gubić zmian. Dwa przedziały nie wystarczą zawsze: stara i nowa ramka mogą dać cztery pionowe krawędzie, a dochodzi kursor i zmiany aplikacji.

Koszt orientacyjny 8 przedziałów × dwa indeksy uint16 × 600 wierszy = 19 200 B plus liczniki; dziś dwa indeksy na wiersz to 2 400 B. Alternatywa to bitmapa zmienionych bloków z kontrolowaną nadmiarowością. Porównać pamięć i czas, nie wybierać na podstawie samej liczby struktur.

**Priorytet P1.** Najlepszy stosunek potencjalnego efektu do zakresu zmiany. Test zgodności: wynik obrazu identyczny z pełną prezentacją, także przy nakładających się ramkach, kursorze i przepełnieniu listy.

## 2. Po przesunięciu nie odświeżać całego ekranu

W kernel_wm.c po `wm_move()` i `notify_geometry()` wywoływane jest `kernel_wm_damage_all()`. Tymczasem `wm_move()` już dodaje stary i nowy prostokąt oraz odbudowuje clipping.

**Propozycja:** po naprawieniu kompletności regionów i dekoracji usunąć rutynowy pełny refresh. Najpierw pozostać przy poprawnym starym+nowym obszarze, a potem ograniczyć do nowego okna oraz odsłoniętych części starego położenia. Pełne odświeżenie zostawić na zmianę trybu, utratę ważności bufora lub jawną ścieżkę awaryjną.

W 800×600×256 pełny ekran to 480 000 pikseli. Zysk zależy od rozmiaru okna: przy prawie pełnoekranowym oknie mniejszy, przy małym znacznie większy. Nie usuwać tej funkcji w ciemno, jeśli dotąd maskowała błędy clippingu lub dekoracji.

**Priorytet P1 po testach poprawności regionów.**

## 3. Rozdzielić MOVE od RESIZE u odbiorców

`WMOV` obsługuje oba przypadki. Desktop aktualizuje współrzędne okna i wywołuje `fb_paint()`, `tr_paint()`, `ma_paint()` itd. także wtedy, gdy zmieniło się tylko położenie. Atari Commander (`system/ac.c`) dodatkowo odpytuje tryb wideo i na końcu wykonuje `paint()` przy każdym WMOV.

Kompozytor ma już zachowaną powierzchnię klienta. Samo przesunięcie nie zmienia jej pikseli.

**Propozycja:** zachować WMOV, ale odbiorca porównuje w/h oraz wersję trybu ekranu. Dla zmiany wyłącznie x/y aktualizuje dane do hit-testów i kończy obsługę bez GFX_BATCH, bez ponownego layoutu. Dla resize lub zmiany trybu wykonuje layout i pojedynczy repaint. Informację o zmianie trybu można rozsyłać osobno zamiast dodatkowego synchronicznego zapytania przy każdym ruchu.

Warunek: współrzędne rysowania aplikacji muszą być lokalne dla powierzchni. Sprawdzić każdego odbiorcę; aplikacje rysujące zależnie od pozycji ekranowej mogą potrzebować wyjątku.

**Priorytet P1.** Prosty cel do pomiaru: MOVE ma generować zero batchy treści aplikacji. Zmiana aktywności może nadal wymagać dekoracji WM.

## 4. Resize: jedna operacja końcowa, bez dublowania pracy

Obecnie podczas resize przesuwana jest tylko ramka. Po release kod usuwa ją i flushuje, zmienia rozmiar, odbudowuje clipping przez `wm_resize()`, po czym `wm_resize_client_to_frame()` odbudowuje go ponownie, zgłasza damage i wysyła WMOV. Aplikacja dopiero wtedy rysuje nowy układ.

**Propozycja:** operacja zatwierdzenia resize ustala frame i client razem, odbudowuje regiony raz i zgłasza jedną zmianę geometrii. Zmiany ramki, kursora i gotowej zawartości prezentować w kontrolowanej kolejności, ograniczając pośrednie flush.

Przy powiększeniu nie pokazywać nieprzygotowanego obszaru powierzchni. Praktyczny pierwszy wariant: zachować stary poprawny obraz, nowy obszar wypełnić neutralnym tłem, a aplikacja odrysowuje nowy rozmiar raz. Bardziej dopracowany wariant: numer generacji resize i publikacja nowej powierzchni po zakończeniu batcha. To wymaga uzgodnienia protokołu, nie jest drobną podmianą funkcji.

Dla zmniejszenia często wystarczy przycięcie istniejącej powierzchni i odświeżenie elementów, których układ faktycznie zależy od rozmiaru. Nie zakładać, że każda aplikacja może obyć się bez layoutu.

**Priorytet P2.**

## 5. Ograniczyć prezentacje ramki i zachować szybki release

Gałęzie przesuwania i resize wykonują `kvideo_flush()` przy każdej wykrytej zmianie ramki, przed ograniczeniem częstotliwości głównej kompozycji. W tym samym przebiegu możliwa jest kolejna prezentacja kompozytora. Renderowanie wszystkich pośrednich pozycji nie pomaga, gdy wejście przychodzi szybciej niż można je pokazać.

**Propozycja:** obliczać najnowszą pozycję, a ramkę i kursor prezentować wspólnie najwyżej raz na wybrany okres, np. 20 ms jako punkt wyjścia dla 50 Hz. Release zatwierdzać od razu z jego współrzędnych. Scalać wyłącznie ruch; zachować wszystkie DOWN/UP. Nie wykonywać opóźniającego sleep w pętli wejścia.

Naprawa transportu kliknięć z poprzedniego audytu pozostaje warunkiem: kolejka nie może zamieniać release w stan zagubiony pod ruchem. Sprawdzić też, czy końcowa geometria jest liczona z pozycji release, a nie jedynie z ostatniego obsłużonego pakietu z wciśniętym przyciskiem.

W aktualnym kodzie jest już `outline_xor_active()` do zdejmowania/przywracania ramki wokół kompozycji. Zachować tę ochronę i zweryfikować wszystkie ścieżki prezentacji. Nie wracać do niechronionego XOR na zmieniającym się tle.

**Priorytet P2, po zmniejszeniu kosztu flush.**

## 6. Przesuwanie z widoczną zawartością — opcja późniejsza

Jest wykonalne przy zachowanej powierzchni okna: aktualizacja położenia nie powinna wymagać renderowania treści przez aplikację. Kompozytor kopiuje powierzchnię do nowego miejsca i odtwarza obszar odsłonięty. Przy dużych oknach w 8bpp nadal oznacza to znaczny transfer.

Nie należy po prostu włączyć `wm_move_live()`: ta funkcja zgłasza symetryczną różnicę starego i nowego prostokąta. Bez wcześniejszego przemieszczenia wspólnego fragmentu obrazu część wspólna będzie przedstawiać piksele ze starego położenia. Poprawna droga to skomponowanie całego nowego widocznego klienta albo sprawdzony blit z obsługą overlap i regionów zasłoniętych.

Proponuję najpierw szybki tryb ramki jako domyślny, następnie opcjonalny live move dla małych okien lub konfiguracji potwierdzonych pomiarem. Nie zakładać dostępności sprzętowego blittera bez sprawdzenia faktycznego sterownika/karty.

## Plan pomiarów i odbioru

1. Powtarzalny ruch okna 200×150 i 600×400, resize w obu kierunkach, 800×600×256 i mono, nad pustym pulpitem oraz nad aktywnym oknem.
2. Mierzyć osobno: czas przetwarzania wejścia, XOR, skan front/shadow, zapisy VRAM, kompozycję, reakcję aplikacji po WMOV i czas od release do stabilnego obrazu.
3. Liczniki: liczba flush na prezentowany krok, liczba skanowanych słów i zapisanych słów, liczba batchy aplikacji, powierzchnia damage oraz p50/p95 opóźnienia ramki/kursora.
4. Najpierw wdrożyć rozłączne zakresy wideo i MOVE bez repaint klienta. Porównać. Następnie ograniczyć pełny refresh i uporządkować resize. Nie zmieniać wszystkich warstw naraz.
5. Weryfikować piksele wobec pełnej kompozycji: nakładanie okien, brzegi ekranu, tytuły, kursor, zamknięcie w trakcie zdarzeń, szybki release i przepełnienie regionów. Przyspieszenie nie może opierać się na pomijaniu potrzebnych zmian.

Realistyczna ocena: zdecydowane przyspieszenie jest uzasadnione zakresem zbędnej pracy, szczególnie dla dużej ramki w kolorze. Nie ma jeszcze podstaw do obietnicy konkretnego mnożnika szybkości całego systemu. Wynik 43,87× dotyczy jednego składnika — zakresu skanowania pamięci dla ramki — i powinien wyznaczać pierwszy eksperyment.
