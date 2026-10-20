# ATOS 2.1 — kursor, transport myszy i dwuklik

Audyt 3 października 2026. Aktualne źródła: `/Users/xxx/Documents/Deepseek_test/ATOS 2.1`. Sumy badanych plików znajdują się w source-manifest.json. Nie zmieniano źródeł roboczych ani obrazu dysku. Wykonano analizę kodu i dwa testy diagnostyczne na komputerze gospodarza; nie przeprowadzono pomiaru opóźnienia kursora w Hatari ani testów na Atari.

## Wniosek

Najpierw trzeba naprawić przekazywanie zdarzeń myszy. Pulpit potrafi nie dostać wciśnięcia, więc nie ma czego rozpoznawać jako pierwszy lub drugi klik. Zwiększenie progu dwukliku do około sekundy już jest w kodzie i nie rozwiązuje utraty zdarzeń. Opóźnienie renderowania dodatkowo zwiększa ryzyko zebrania kilku zmian przycisku przed odbiorem.

Niezależnie od tego powiększenie ikon do 48×48 nie zostało konsekwentnie przeniesione do obliczania odświeżania i sprzątania. Pozostałości obrazu nie muszą oznaczać zgubionego kliknięcia: są też osobnym błędem grafiki.

## 1. Potwierdzona utrata kliknięć w kernel_sessions.c

Łańcuch: INPUT → `kernel_raw_mouse_event()` → `ksessions_pump()` → `handle_mouse()` → `ksessions_mouse_event()` → `kernel_user_gui_keyboard_wake()` → KMOU → desktop.

`ksessions_pump()` opróżnia do 64 pakietów i wywołuje `handle_mouse()` przy zmianach przycisków. Jednak `handle_mouse()` nie odkłada tych zmian do kolejki. Nadpisuje pojedyncze `forward_buttons`, sumuje ruch i ustawia pojedynczy `forward_ready`:

```
forward_buttons=buttons|(forward_buttons&0x1cu);
forward_x+=dx; forward_y+=dy; forward_ready=1;
```

Zachowanie pewnych bitów „było wciśnięcie” nie zachowuje sekwencji DOWN/UP. Desktop czyta tylko dolne trzy bity z przesłanego pola przycisków, a start kliknięcia wymaga bitu LEFT_DOWN. Dla szybkiego DOWN/UP końcowy stan nie ma tego bitu. `mouse_press_pending` też jest pojedynczą flagą, nie kolejką wielu kliknięć; obsługa WM dodatkowo wymaga bieżącego wciśnięcia przycisku.

**Test rzeczywistych funkcji `handle_mouse()` i `ksessions_mouse_event()`:**

- Odbiór po każdym zboczu: dostarczone dwa zdarzenia, 13 i 0 — poprawnie.
- Dwa kliknięcia przed odbiorem: wejście 13,0,13,0; wyjście tylko jedno zdarzenie 12. Pulpit widzi stan puszczony zamiast czterech zboczy.

To potwierdzony błąd transportu, niezależny od ustawienia czasu dwukliku. Test jest diagnostyczny i celowo kończy się niepowodzeniem na badanej wersji.

## 2. Kolejne miejsca utraty lub zniekształcenia wejścia

`kernel_user_gui_keyboard_wake()` w kernel_exception.c:

- Przed odbiorem sprawdza pojemność skrzynki `gui_keyboard_pid`, a dopiero później wybiera faktyczny proces docelowy pod kursorem lub przechwytujący kliknięcie. Po zmianie celu nie wykonuje równoważnego sprawdzenia jego pojemności przed dopisaniem. Może blokować odbiór przez skrzynkę innego procesu albo przepełnić skrzynkę właściwego odbiorcy.
- Współrzędne wiadomości pobiera z aktualnej pozycji sesji, nie z zapamiętanej pozycji zdarzenia. Po wprowadzeniu kolejki nie wolno zostawić tego kodu bez zmiany: kliknięcie musi pozostać związane ze swoim x/y, nawet jeśli wskaźnik już odjechał.
- Scalanie KMOU przy jednakowych stanach przycisków jest sensowne dla kolejnych ruchów, ale nie odzyska zboczy wcześniej zgubionych w sesji.

`pending_push()` w desktop_wm.c ma limit 16 i po zapełnieniu odrzuca wiadomość bez sygnalizacji. Ta kolejka przyjmuje zdarzenia podczas synchronicznego oczekiwania na odpowiedź GFX. Zgubiony UP może pozostawić `prev_left` lub drag w złym stanie. W tej turze nie zmierzono rzeczywistej częstości jej przepełnienia.

## 3. Sam dwuklik

`DBL_TICKS=12000`, przy nominalnych 12288 jednostkach/s daje około 0,98 s. To już bardzo szerokie okno. Desktop uruchamia ikonę po drugim puszczeniu przycisku, o ile zgadza się `sel_icon`, nie było przeciągnięcia i czas mieści się w limicie.

Czas odczytuje przez `hires()` podczas obsługi wiadomości, zamiast używać czasu rzeczywistego zdarzenia. Przy zaległościach w rysowaniu fizyczny odstęp kliknięć może różnić się od odstępu ich obsługi. Wybór ikony (`sel_icon`) jest też jednocześnie pamięcią pierwszego kliknięcia; lepiej oddzielić te stany.

**Docelowo:** jawny stan kandydata dwukliku: tożsamość obiektu, czas zdarzenia, pozycja, ważność. Anulować go po przeciągnięciu, kliknięciu innego celu, otwarciu menu i uruchomieniu aplikacji. Dopuszczać niewielkie drżenie myszy, ale nie traktować ruchu przeciągania jako kliknięcia. Po przywróceniu niezawodnego wejścia ustawić konfigurowalny próg, np. domyślnie 400–500 ms; nie obniżać go jako pierwszego kroku.

## 4. Dlaczego kursor może reagować wolno

To ocena ścieżki wykonania, nie zmierzona wielkość opóźnienia:

- Stan pozycji jest aktualizowany przez sesję, lecz prezentacja wskaźnika zależy od pracy głównej pętli i kompozytora. Długi batch graficzny opóźnia powrót do obsługi wejścia.
- `kgfx_draw_batch()` zdejmuje i przywraca widoczny kursor przy każdym batchu, także przy rysowaniu odległej lub niewidocznej powierzchni. To dodatkowe odtwarzanie tła i zgłoszenia zmian. Przyczynę należy mierzyć; sam kursor ma tylko do 16×16 pikseli.
- `paint_hover()` czyści tylko dwie komórki, ale potem wywołuje `draw_icons()` dla WSZYSTKICH ikon. `gui_damage()` zwraca jeden prostokąt obejmujący całą listę. W efekcie hover nadal może uruchamiać rysowanie i kompozycję dużej części pulpitu. Obecny komentarz o lokalnym repaint nie opisuje całego kosztu.
- Droga KMOU ma ograniczoną skrzynkę wejściową (6 wiadomości z 8 miejsc skrzynki), a pulpit czeka synchronicznie na usługi. Nie należy wiązać prezentacji aktualnej pozycji kursora z tym, czy aplikacja zdążyła odebrać wszystkie ruchy.

**Zalecana optymalizacja:** osobna aktualna pozycja kursora dla prezentacji i kolejka uporządkowanych zdarzeń interakcji. Ruchy o niezmienionym stanie przycisków można scalać; DOWN/UP zachować z x/y i czasem. Odrysowywać hover tylko dla starej i nowej ikony oraz faktycznie naruszonych sąsiadów. Zdejmować kursor przy rysowaniu do widocznej powierzchni tylko wtedy, gdy zmiana przecina jego obszar; zachować osobną ochronę dla kompozycji i nakładek.

Nie deklarować rozwiązania przez zwiększenie prędkości myszy. `mouse_x+=dx; mouse_y+=dy` daje obecnie ruch 1:1. Jeżeli użytkownik ma na myśli zbyt małą drogę wskaźnika przy płynnym ruchu, potrzebna jest konfigurowalna czułość/akceleracja. Jeżeli wskaźnik rusza z opóźnieniem lub nadrabia po zatrzymaniu ręki, zwiększanie czułości pogorszy sterowanie.

## 5. Nowy błąd grafiki: 48×48 vs stare granice 32×32

`system/icons_color.h` deklaruje `ICON_SIZE=48`; renderer używa wysokości `ICON_SIZE+16`, czyli 64. `include/gui_damage.h` nadal dla obu rodzajów ikony zakłada minimalny obraz 32 i wysokość 48.

Test pojedynczego `GUI_ICON_RGB` przy (100,100), pustej etykiecie i rozmiarze polecenia 48×64 zwraca granice **(92,100)–(132,148)**. Potrzeba co najmniej prawej krawędzi 148 i dolnej 164. Batch ignoruje w/h ikony przy obliczaniu tych granic. Skutkiem może być brak odświeżenia prawej/dolnej części.

W samym `kgfx_icon_color()` minimalna szerokość nadal wynosi 40 od x−8. Dla obrazu 48 wymaga minimum **56**; przy pustych lub krótkich podpisach renderer sam przycina obraz. Sprzątanie starych pozycji w `paint_icons()` nadal czyści 112×48, podczas gdy aktualne komórki są czyszczone jako 120×64. To pozostawia dolny pas po przesuwaniu kolorowych ikon.

**Naprawa:** jedna wspólna definicja wymiarów kolorowej ikony, osobna od wymiarów mono; ten sam kontrakt w rendererze, damage, czyszczeniu starych pozycji, pickerze i hit-testach. Nie zmieniać samego h w komendzie — obecny damage go nie używa. Testować puste, krótkie i długie podpisy, prawa/dolna krawędź ekranu oraz przesunięcie ikony. Dołączony test celowo nadal wykazuje błąd.

## Kolejność pracy dla DeepSeek

1. Wprowadzić kolejkę zdarzeń wejściowych sesji z DOWN/UP, absolutnym x/y i czasem. Oddzielić ją od aktualnego stanu kursora. Nie wystarczy zwiększyć limitu czasu lub odtworzyć jednego syntetycznego kliknięcia z bitu „było naciśnięcie” — może być ich kilka.
2. Odbiorcę i pojemność jego skrzynki ustalać przed zdjęciem zdarzenia z kolejki. Przy braku miejsca zachować zdarzenie. Zaplanować jawną obsługę przeciążenia; nie odrzucać cicho UP. Zachować przechwycenie odbiorcy przez cały klik.
3. Przenieść x/y i timestamp przez protokół KMOU. Pole value[3] obecnie zawiera licznik zgubionych zdarzeń: nie nadpisywać go bez zmiany kontraktu i wszystkich odbiorców. Desktop, okna aplikacji i WM muszą korzystać ze zgodnej kolejności zdarzeń, nie konkurować o ten sam pojedynczy odczyt.
4. Zabezpieczyć kolejkę pending pulpitu i oddzielić stan dwukliku od zaznaczenia.
5. Naprawić kontrakt ikon 48×48, następnie ograniczyć hover do zmienionych obszarów i zmierzyć prezentację kursora.

## Warunki odbioru

- Każde DOWN ma dostarczone UP we właściwej kolejności, także gdy oba nastąpiły podczas jednego rysowania.
- Dwa szybkie kliknięcia uruchamiają dokładnie raz; pojedynczy nie uruchamia; przeciąganie nie uruchamia.
- Sprawdzić 100, 250, 450 i 900 ms oraz kolejkę zajętą repaintami, klik na innej ikonie, odjazd kursora, zmianę okna, menu i długie przytrzymanie.
- Sprawdzić pełną skrzynkę docelowego procesu innego niż gui_keyboard_pid; żadnego nadpisania i wiszącego przycisku.
- Rejestrować czas wejścia, dostarczenia KMOU i prezentacji kursora, przepełnienia kolejek oraz powierzchnię odświeżania. Podać medianę i p95 opóźnienia w kolorze i mono.
- Testy w pakiecie to reprodukcje aktualnych błędów. Nie są poprawką ani dowodem działania całego systemu po zmianach. Nie dołączono ryzykownego częściowego patcha transportu, ponieważ wymaga skoordynowanej zmiany producenta, routingu, WM i odbiorców.
