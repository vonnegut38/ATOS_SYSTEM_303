# ATOS 2.1 — ikony, przypisywanie i odświeżanie pulpitu

Analiza: 2 października 2026. Źródło: `/Users/xxx/Documents/Deepseek_test/ATOS 2.1`.

## Wniosek

Zgłoszone objawy mają konkretne przyczyny w kodzie. Nie należy dalej naprawiać ich przez losowe zmiany palety lub wymuszanie ciągłego pełnego odświeżania. Są trzy niezależne grupy błędów: przypisywanie i zapis ikon, obliczanie obszaru rysowania oraz zamykanie nakładek menu.

Analiza obejmuje źródła, podgląd istniejących PNG, test wyodrębnionych funkcji renderera i kompilację m68k. Nie wykonano w tej turze pełnego testu interaktywnego w Hatari ani na Atari. Źródła robocze DeepSeek i obraz jego dysku nie były modyfikowane. Poprawki przygotowano w oddzielnej kopii.

## 1. Dlaczego My Atari nie daje się zmienić — potwierdzone w źródłach

`system/desktop_wm.c`, `icon_rgb_id()` (wersja bazowa około linii 1577): dla `builtin == 5` funkcja natychmiast zwraca `ICON_DISKS_04_*`. Nie sprawdza żadnego przypisania użytkownika.

`icp_assign()` (około 1411): dla dowolnej ikony pulpitu zapisuje wybór pod jednym kluczem `APP`. Ignoruje tożsamość wybranej ikony. Skutek: próba zmiany My Atari może zmienić wygląd wszystkich zwykłych aplikacji, a My Atari zachowuje zasób wpisany na sztywno. Terminal i kosz również mają własne stałe ścieżki omijające ustawienia.

**Do wdrożenia przez DeepSeek:** oddzielić przypisania konkretnego obiektu od domyślnych ikon typu pliku. Najpierw szukać przypisania użytkownika, dopiero potem stosować domyślny zasób. Identyfikować obiekty stabilnie: `builtin:my-atari`, `builtin:trash`, `builtin:terminal`, a skróty przez pełną ścieżkę programu. Indeks tablicy lub etykieta nie powinny być trwałym identyfikatorem. Nie stosować globalnego `APP` jako zamiennika tożsamości skrótu.

## 2. Konfiguracja ICONS.CFG nie może obecnie poprawnie przetrwać restartu

`ic_load()` i `ic_save()` (około 1330–1371):

- Odczyt kopiuje klucz z prawej strony `=`, czyli z miejsca wartości, zamiast z lewej.
- Zapis zawsze dodaje prefiks `D`, niezależnie od tego, czy wpis oznacza katalog, czy rozszerzenie pliku.
- ID zapisuje jako pojedynczy znak `'0' + id`. Dla ID 10 powstaje `:`, dla 49 — `a`. Odczyt nie jest poprawnym parserem liczby dziesiętnej i nie weryfikuje zakresu.
- Klucz ma tylko trzy znaki: katalogi o dłuższych nazwach nie pasują do wyszukiwania, mogą też kolidować.
- Wynik zapisu na dysk jest ignorowany. Interfejs może zamknąć okno mimo błędu zapisu.

Przykład: przypisanie `APP -> 49` zostanie zapisane jako `DAPP=a`. Odczyt utworzy klucz `a`, typ katalog i wartość 49; pierwotne przypisanie APP znika.

**Zalecany format:** wersjonowany, z oddzielnymi rodzajami klucza i symbolicznymi nazwami zasobów, np. `builtin:my-atari=files_00`, po weryfikacji rzeczywistego obrazu. Ustalić jawne pary normal/hover w tabeli zasobów. Nie opierać trwałych przypisań na indeksie wynikającym z sortowania nazw PNG: dodanie pliku przed istniejącymi przesuwa indeksy. Wykonać test zapisz → odczytaj → restart dla każdego rodzaju przypisania. Stary format utracił informację o rodzaju wpisu; nie obiecywać bezstratnej migracji. Zachować kopię starego pliku i jasno komunikować odrzucenie niepoprawnych wpisów.

## 3. Artefakty po menu — potwierdzona przyczyna

`ctx_paint()` rysuje menu do powierzchni pulpitu, a `paint()` odtwarza całe tło tylko przy `bg_dirty`. Większość ścieżek zamknięcia menu wykonuje wyłącznie `ctx_open=0; paint()`. Obszar menu poza komórkami ikon pozostaje namalowany. Analogiczny problem dotyczy okienka INFO. `ctx_arrange()` zmienia pozycje ikon bez unieważnienia dawnych miejsc.

Menu kontekstowe nie rejestruje też `GFX_OVERLAY`, mimo że rozwijane menu FILE/OPTIONS już z tego mechanizmu korzysta. Powinno być wyświetlane nad oknami aplikacji, a po zamknięciu zwolnić nakładkę.

Dodatkowo w gałęzi `ctx_open` pętla przechodzi przez `continue`, pomijając reset `prev_left` przy puszczeniu przycisku. Może to blokować następne kliknięcie. `ctx_activate()` po INFO ustawia stan informacji, ale końcowe `paint()` jej nie rysuje.

**Dostarczono `02-menu-kandydat.patch`:** czyszczenie tła przy zamknięciu, zdjęcie/rejestracja nakładki, odmalowanie INFO, reset stanu przycisku, walidacja numeru wiersza menu i odświeżenie po ARRANGE. Patch przeszedł kompilację aplikacji; wymaga testu integracyjnego. Celowo jest zachowawczy: pełne tło odtwarza przy zamknięciu, nie przy każdym ruchu myszy. Po potwierdzeniu poprawności warto zastąpić pełny repaint odtworzeniem prostokąta starego popupu i ikon przecinających ten prostokąt. Trzeba sprawdzić wzajemne wykluczanie FILE/OPTIONS, menu kontekstowego i INFO — mechanizm nakładki jest współdzielony.

## 4. Renderer: pomijane odświeżanie i zapis poza powierzchnią

`include/gui_damage.h`: warunek odrzuca rodzaje poleceń większe od `GUI_WINDOW`. W praktyce pomija `GUI_BEVEL`, `GUI_ICON` i `GUI_ICON_RGB`. Batch zawierający tylko ikonę może w ogóle nie zostać narysowany. W batchu mieszanym problem bywa maskowany przez duży prostokąt tła, ale obliczone granice nie uwzględniają ikon i etykiet.

`kgfx_icon_color()` w `system/kernel_graphics.c`: zapisuje piksele bez przycięcia do aktywnej powierzchni. `surface_pixel_set()` nie kontroluje granic. Ujemna współrzędna po rzutowaniu na `uint16_t` może stać się bardzo dużą dodatnią wartością. Jest to błąd poprawności pamięci, nie tylko wyglądu. Nie wykazano, że odpowiada za każdy artefakt zgłoszony przez użytkownika.

Szerokość obszaru ikony zależy od etykiety: przy pustej etykiecie wynosiła 16 pikseli od x−8, mimo że obraz ma 32 piksele od x. To istotne w pickerze, który używa pustych podpisów. Analogiczną minimalną szerokość poprawiono dla ikony monochromatycznej.

**Dostarczono `01-renderer.patch`:** obsługa wszystkich trzech poleceń w damage, uwzględnienie etykiety i obrazu oraz przycinanie ikon do powierzchni. W teście wywołującym rzeczywistą funkcję kolorowego renderera dla 50 zasobów i 100 położeń na powierzchni 64×64:

| Kontrola | Przed | Po |
|---|---:|---:|
| Test obszaru odświeżania | błąd | OK |
| Próbne zapisy poza powierzchnią | 6 240 550 | 0 |

W teście zapis piksela jest instrumentowany: liczby dotyczą celowo skrajnych przypadków testowych, nie pomiaru liczby błędów w zwykłej sesji desktopu. To nie jest benchmark szybkości ani test całego systemu. Zbudowano również KERNEL.SYS i DESKTOP.APP w kopii projektu.

## 5. Wygląd i dobór zasobów

Przejrzano `_contact_sheet.png` i konkretne pliki używane przez kod:

- `disks_04_normal.png` — koło zębate; kod przypisuje je My Atari z komentarzem „computer”.
- `tools_01_normal.png` — koperta; kod używa jej jako kosza.
- `apps_00_normal.png` — klatka filmu; kod używa jej jako terminala.

Nazwy grup i komentarze nie odzwierciedlają zawartości PNG. Najpierw stworzyć jawny katalog: obraz → znaczenie → para normal/hover. Wybór ikon powinien pokazywać 25 wzorów, a nie 50 stanów jako niezależne pozycje. Obecny picker pozwala wybrać także wariant hover jako stan normalny; resolver wylicza hover przez `id & ~1`, co wtedy daje tę samą ikonę.

Obecna ścieżka PNG 96×96 → 32×32, wspólna paleta 32 kolorów i maska 1-bit jest rozsądną podstawą dla 800×600×256. Próg alfa 128 usuwa półprzezroczystość; po zmniejszeniu cienkie kontury i cienie mogą być twardsze. Nie ma potrzeby dodawać dekodowania PNG przy każdym rysowaniu. Paleta, maska i para wariantów powinny powstawać podczas budowania. Ocenę finalnych krawędzi należy wykonać na obrazie z emulatora, nie wyłącznie na podglądzie 96×96.

## 6. Picker i ikony plików: dodatkowe błędy

- `icp_click()` zaczyna strefę przycisków przy `sy >= icp_wh−60` (400), ale rysuje je przy `icp_wh−44` (416). Dolna część ostatniego rzędu miniaturek trafia w obsługę przycisków. Kliknięcie miniatury może zapisać poprzedni wybór/zamknąć picker.
- `(sy−24)/40` dla kliknięcia nad siatką może dać zero przez zaokrąglenie dzielenia w C w stronę zera. Najpierw sprawdzać rzeczywisty prostokąt siatki.
- Okno ma stałą wysokość 460, więc nie mieści się w 640×400. Nawet 640×480 pozostawia bardzo mały margines. Potrzebne dopasowanie układu lub strony; 25 wzorów zamiast 50 stanów wyraźnie zmniejszy wymagany rozmiar.
- Wybór nie jest inicjalizowany aktualnym przypisaniem (`icp_sel=-1`). Użytkownik nie widzi, co obecnie obowiązuje.
- Cel pliku zapamiętywany jest jako numer wiersza; po zmianie strony/katalogu/sortowania może oznaczać inny obiekt. Zapamiętać docelową ścieżkę lub klucz podczas otwierania pickera.
- Mapowania zapisane z okna plików nie mają kompletnej ścieżki renderowania: `ic_id_for()` jest obecnie wywoływane tylko dla `APP` w resolverze ikon pulpitu. Samo zapisanie przypisania rozszerzenia/katalogu nie spowoduje zmiany widoku plików.

Te błędy przypisywania i pickera są opisane do wdrożenia; dostarczone dwa patche ich nie naprawiają.

## Kolejność wdrożenia dla DeepSeek

1. Porównać aktualne pliki z `source-manifest.json`. Patche odnoszą się do zapisanej wersji bazowej; nie nadpisywać nowszych zmian bez przeglądu.
2. Zastosować poprawkę renderera, uruchomić `python3 test_renderer.py /ścieżka/do/kopii/ATOS` i przebudować kernel.
3. Sprawdzić kandydat poprawki menu na oddzielnym obrazie dysku: klik poza menu, Escape, wybór każdej pozycji, menu przy krawędzi, nad oknem aplikacji, INFO, wielokrotne otwarcie/zamknięcie i ARRANGE.
4. Wdrożyć stabilne przypisania konkretnych ikon i poprawny, wersjonowany zapis konfiguracji. Dopiero potem poprawiać wygląd pickera.
5. Sprawdzić zmianę My Atari bez wpływu na SYSINFO/pozostałe skróty, wariant hover i trwałość po restarcie. Sprawdzić błąd zapisu, pełną tabelę i błędny plik CFG.
6. Powtórzyć testy dla 800×600×256 oraz trybu mono. Dopiero na poprawnym obrazie mierzyć koszt odświeżania: minimalny uszkodzony prostokąt, a nie pełny ekran przy każdym ruchu.

Do pakietu nie dołączono binariów do automatycznej instalacji. Logi kompilacji zawierają również istniejące ostrzeżenia Makefile/linkera; sukces budowania nie zastępuje testu uruchomienia.
