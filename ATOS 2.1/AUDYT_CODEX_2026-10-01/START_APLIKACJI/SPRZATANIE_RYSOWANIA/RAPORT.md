# ATOS 2.1 — ponowna analiza rysowania i sprzątania

Analiza aktualnych źródeł z 2 października 2026, po wdrożeniu poprzednich poprawek przez DeepSeek. Zbadano desktop, renderer, kompozytor, listy obszarów odświeżania i operacje wypełniania. Pracowano na oddzielnej kopii; nie zmieniono roboczych źródeł ani obrazu dysku użytkownika.

## Najważniejszy wniosek

Pozostające artefakty mają dalsze przyczyny w geometrii odświeżania i kolejności rysowania. Poprzednia poprawka GUI_ICON_RGB jest obecna. Samo przyspieszanie wypełniania lub zwiększanie częstotliwości repaint nie naprawi gubionych prostokątów, za wąskiego czyszczenia podpisów ani fragmentów pomijanych przez clipping.

Dostarczono dwa patche do przeglądu i testy odtwarzające błędy. Nie są kompletną naprawą wszystkich opisanych problemów. W szczególności przepełnienie listy widocznych fragmentów okna wymaga osobnej poprawki; dołączono test, który celowo nadal wykazuje ten problem.

## Potwierdzone testami problemy

| Problem | Aktualna wersja | Kopia z patchami |
|---|---|---|
| Przepełnienie kolejki odświeżania | 56 z 80 zgłoszeń niepokrytych przez kolejkę | 0 zgubionych |
| Podpis po przesunięciu ikony | 192 próbne piksele pozostają | 0 |
| Sąsiednia komórka kasuje długi podpis | tak | nie |
| Wysokość dokumentu po podniesieniu okna | pozostaje przy złym oknie | zachowana przy właścicielu |
| Przepełnienie listy widocznych fragmentów | 92 400 widocznych pikseli pominiętych w układzie testowym | nadal wymaga naprawy |

Wyniki dotyczą testów funkcji na komputerze gospodarza, nie sesji użytkownika. Test pulpitu używa rzeczywistej funkcji `paint_icons()` z instrumentowanym backendem i prostokątnym modelem podpisu. Nie jest zrzutem ekranu ani testem rzeczywistego kształtu liter. Test kompozytora korzysta z rzeczywistego `wm_core.c`.

## 1. Kolejka odświeżania po zapełnieniu ignoruje zmiany

`system/wm_core.c`, `wm_damage_add()`; limit `WM_DAMAGE_MAX=24`.

Jeżeli nowy prostokąt nie zostanie scalony z dotychczasowymi i lista jest pełna, funkcja po prostu go nie zapisuje. Zawartość została już zmieniona, lecz kompozytor może nigdy nie otrzymać żądania jej odtworzenia. Objaw zależy od następnych operacji, dlatego artefakt może zniknąć po przesunięciu innego okna lub pełnym refresh.

**Patch 01:** dopiero przy przepełnieniu scala oczekujące prostokąty i nowy w jeden obejmujący je obszar. Normalne małe aktualizacje zachowują dotychczasowe oszczędne scalanie. Awaryjne odrysowanie większego obszaru jest poprawniejsze niż trwała utrata informacji. Nie zwiększać wyłącznie limitu: to odsuwa błąd, nie usuwa go.

## 2. Czyszczenie ikon jest za wąskie i ma złą kolejność

`system/desktop_wm.c`, `paint_icons()`; siatka `CELL_W=84`, podpis do 12 znaków × 8 pikseli = 96 pikseli.

Dotychczasowe sprzątanie czyści 84 piksele od x−8. Ostatnie 12 pikseli długiego podpisu zostaje po przesunięciu. Dodatkowo pętla wykonuje kolejno „wyczyść A, narysuj A, wyczyść B, narysuj B”. Czyszczenie B może wymazać końcówkę podpisu A, ponieważ siatka jest węższa od podpisu.

**Patch 02:** najpierw czyści wszystkie stare i aktualne obszary, potem rysuje wszystkie ikony. Przyjmuje zachowawczą szerokość 112 pikseli zgodną z obecnym obszarem zgłaszanym przez renderer. Gdy całe tło zostało już odtworzone, nie czyści ponownie każdej komórki. To usuwa zbędne wypełnienia po pełnym refresh. Nie zmienia rozstawu siatki; długie podpisy nadal mogą się wizualnie nakładać. Docelowo trzeba ustalić limit szerokości podpisu, skracanie albo większą komórkę.

## 3. Lista widocznych fragmentów także gubi prostokąty — osobna naprawa

`include/wm_core.h`: `WM_CLIP_MAX=16`. `wm_rebuild_clip()` i `wm_subtract()` kończą zapisywanie po osiągnięciu limitu bez zgłoszenia przepełnienia i bez poprawnej ścieżki awaryjnej.

Test tworzy okno tła i siedem zasłaniających je okien: trzy pionowe pasy i cztery poziome. Powinno pozostać 20 widocznych fragmentów, tablica mieści 16. W tym układzie 92 400 widocznych pikseli wypada z listy. To dowód błędu algorytmu; nie oznacza, że użytkownik ma właśnie taki układ okien.

**Do wykonania:** wykrywać przepełnienie podczas odejmowania i zapewnić dokładną ścieżkę zastępczą, np. składanie pasami według granic okien lub inny region bez utraty fragmentów. Samo pełne zgłoszenie damage NIE rozwiązuje problemu: `wm_recompose()` nadal korzysta z niepełnej listy clip. Samo zwiększenie tablicy wymaga udowodnionego limitu i kontroli stosu/pamięci m68k. Test `test_clip_capacity.c` ma przejść dopiero po tej naprawie.

## 4. Podniesienie okna gubi jego content_h

`window_copy()` kopiuje położenie, źródło obrazu, tytuł i pozycję przywracania, ale nie `content_h`. `wm_raise()` korzysta z tej funkcji przy zamianie kolejności okien. Informacja o wysokości dokumentu zostaje związana z pozycją w tablicy zamiast z oknem. Może to rozregulować przewijanie i odrysowanie po zmianie aktywnego okna.

**Patch 01** dodaje kopiowanie tego pola. Test dwóch okien o różnych wysokościach dokumentu wykazuje błąd przed poprawką i poprawne zachowanie po niej.

## 5. Kolejność warstw i pozostałości po komunikatach

Dotychczas `paint()` rysuje menu, pole RUN i tekst statusu, a na końcu `paint_icons()`, które kasuje komórki. Ikony i ich tło mogą więc naruszyć wcześniej namalowany popup. Skrócenie tekstu statusu nie usuwa końcówki poprzedniego komunikatu. Zamknięcie lub skrócenie RUN również wymaga odtworzenia jego starego obszaru.

**Patch 02 — kandydat:** odtwarza poprzedni obszar RUN oraz pas statusu przed ikonami, a ikony rysuje przed menu i RUN. Wyłącza hover-repaint podczas INFO, aby ruch myszy nie zamalowywał części komunikatu. Cały desktop nadal wymaga próby w emulatorze; test automatyczny dotyczy przede wszystkim geometrii i kolejności `paint_icons()`.

Pozostaje problem układu: status przy y=20 ma 16 pikseli wysokości, a ikony zaczynają się przy y=28. Trzeba przeznaczyć statusowi własny pas albo przenieść go do dolnej belki. Zmiana kolejności warstw nie usuwa tego nakładania geometrycznego.

## 6. Dekoracje okien: ryzyko utraty części liter i przycisków

`system/kernel_wm.c`, `deco_begin()`, `deco_all()` oraz pętla dekoracji.

`deco_begin()` buduje widoczność z przecięcia damage z ramką, ograniczając fragmenty kolejnym limitem. `deco_fill()` odtwarza nawet fragment tła, lecz `deco_all()` dopuszcza znak dopiero, gdy cały prostokąt znaku mieści się w jednym widocznym fragmencie. Częściowe odświeżenie może więc zamalować kawałek tła pod literą, a nie przywrócić litery. Przyległe fragmenty pokrywające razem cały znak też nie przechodzą tego warunku. Komentarz o pełnym rysowaniu chrome nie zmienia działania tych helperów.

To wniosek z przepływu kodu, bez osobnego testu interaktywnego w tej turze. Zalecenie: rysowanie glyphów i znaków gadżetów z clippingiem do fragmentów. Alternatywnie bezpiecznie poszerzać odświeżanie do całego gadżetu, nadal odejmując okna leżące wyżej. Nie rysować pełnych dekoracji bez kontroli zasłaniania.

## 7. Ramka XOR podczas przesuwania/resize

Ramka jest rysowana bezpośrednio na framebufferze i usuwana ponownym XOR. To działa tylko wtedy, gdy piksele pod nią nie zostały zmienione pomiędzy tymi operacjami. W głównej ścieżce kompozycji zdejmowany jest kursor, ale nie widać analogicznego zdjęcia/przywrócenia aktywnej ramki przed i po `wm_recompose()`.

Ryzyko: aplikacja odrysowuje się podczas przeciągania, kompozytor nadpisuje część ramki, a następne „usunięcie” XOR tworzy ślad na nowych pikselach. Wymaga testu: przeciąganie nad oknem stale zmieniającym zawartość. Docelowo ramka powinna być ostatnią nakładką, zdejmowaną przed każdą kompozycją i przywracaną po niej. Nie wystarczy bezwarunkowo XOR-ować zapamiętany prostokąt.

## Optymalizacja po zapewnieniu poprawności

1. **Hover powinien odrysowywać stary i nowy obszar ikony.** Teraz każda zmiana hover wywołuje pełne `paint()`, które rysuje górną belkę przez całą szerokość ekranu i wszystkie ikony. `gui_damage()` zwraca jeden prostokąt obejmujący całą listę; małe, odległe operacje rozdmuchują zakres kopiowania. Osobny mały batch dla zmienionych ikon ograniczy kopiowanie i pracę kompozytora. Przy nachodzących podpisach uwzględnić sąsiadów i zachować zasadę „wszystkie tła przed treścią”.
2. **Zamykanie popupu:** odtworzyć jego poprzedni prostokąt oraz przecinające go elementy, zamiast całego ekranu. Warunek: poprawna kolejność warstw i brak przepełnień regionów. Nie optymalizować tego przez pomijanie potrzebnego odświeżania.
3. **Wypełnianie 8bpp ma już szybką ścieżkę.** `chunky_fill()`/`chunky_span()` używają wyrównanych zapisów 32-bitowych. Istniejący test wypełnień, glifów, wyrównania, kopiowania i bajtów ochronnych przeszedł. Obecnie większą korzyść powinno dać ograniczenie liczby i powierzchni wypełnień, nie kolejna zamiana tej pętli. To ocena kodu, nie pomiar czasu.
4. **Kolorowa ikona:** po ustaleniu clippingu można obliczać granice pętli raz, używać wskaźnika wiersza i przygotowanych ciągów nieprzezroczystych pikseli zamiast ponownie sprawdzać współrzędne każdego piksela. Zachować maskę i wspólną paletę; bez PNG w pętli renderowania. Potrzebny benchmark przed/po.
5. **Mierzyć rzeczywistą pracę:** liczbę batchy, sumę powierzchni damage, pole bounding box, liczbę przepełnień damage/clip i czas kompozycji. FPS bez informacji o obszarach nie pokaże, dlaczego pojedynczy hover kosztuje prawie pełny ekran.

## Walidacja i wdrożenie

Przeszły: nowe testy kolejki odświeżania i przenoszenia metadanych, test geometrii ikon po poprawce, dotychczasowy `test_wm_core.c`, `test_chunky_raster.c`, kompilacja KERNEL.SYS i DESKTOP.APP. Logi są obok raportu. Istniejące ostrzeżenia linkera/Makefile pozostają w logu.

Nie wykonano pełnej sesji Hatari ani pomiaru przyspieszenia w tej turze. Nie należy traktować pakietu jako potwierdzenia usunięcia wszystkich artefaktów. Patche stosować na kopii zgodnej z `source-manifest.json`, przebudować, uruchomić na oddzielnym obrazie dysku. Nie instalować poprzednich gotowych binariów.

Kolejność dla DeepSeek: patch kompozytora → przegląd patcha pulpitu → naprawa przepełnienia clip → częściowe dekoracje i XOR → optymalizacja hover/popupów. Scenariusze końcowe: długie podpisy, przesuwanie ikon w obie strony, RUN/Escape, skracanie statusu, INFO z ruchem myszy, nakładające się okna, przewijanie po zmianie aktywnego okna, menu nad oknem i przeciąganie nad animowaną zawartością. Powtórzyć w 800×600×256 i mono.
