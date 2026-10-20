# Naprawa animacji PLASMA

3 października 2026. Przygotowano poprawkę `system/plasma.c`, patch wraz z zależnością Makefile i zbudowaną aplikację PLASMA.APP. Test na oddzielnym obrazie ATOS w Hatari, 800×600×256, potwierdził zmianę obrazu bez ruchu myszy lub naciskania klawiszy. Zrzuty klatka-1.png i klatka-2.png pokazują różne fazy tej samej uruchomionej aplikacji.

## Przyczyna zatrzymania

Po pierwszej klatce pętla wywoływała `receive(&m,1)`, czyli blokujący odbiór wiadomości. Bez zdarzeń program nie przechodził do kolejnej klatki. Animacja była zależna od wejścia. Po nieudanym odbiorze kod mógł też analizować poprzednią zawartość wiadomości. To nie był przede wszystkim problem szybkości obliczeń efektu.

Oryginalny kod celowo wykonywał 40 000 iteracji pustej pętli, aby obciążać CPU. Dodatkowo używał 128 indeksów koloru, choć bieżąca wygenerowana paleta ma 102 kolory, w pozycjach 20–121.

## Zmiany

- Nieblokujący odbiór zdarzeń; animację uruchamia zegar, nie wiadomość myszy.
- Najwyżej 16 wiadomości na iterację, żeby ciągły ruch nie zagłodził animacji.
- Oczekiwanie przez `pause_tick()` zamiast pustej pętli zużywającej procesor.
- Docelowy okres około 100 ms. Gdy renderowanie się spóźnia, program przeskakuje do bieżącej fazy zamiast nadrabiać każdą zaległą klatkę.
- Część przestrzenna wzoru obliczana raz. W klatce aktualizowana jest faza i kolor.
- Polecenia wypełniania nie są zerowane bajt po bajcie przy każdym kafelku. Pola nieużywane pozostają wyzerowane w BSS.
- Sąsiednie komórki o jednakowym kolorze łączone w poziome paski. Dla testowanej klatki kolorowej nadal potrzeba 9 batchy; nie twierdzimy, że ten punkt drastycznie zmniejszył IPC.
- Ustawienie wysokości zawartości tylko podczas inicjalizacji, zamiast po każdej klatce.
- Obliczanie kolorów według faktycznej liczby wpisów palety. Makefile śledzi teraz zależność od icons_color.h. Mono ma jawne odwzorowanie na dwa kolory.
- Wyczyszczenie marginesów okna przy starcie.

Wzór nadal używa kafelków i wspólnej palety ikon. Korekta zawijania do prawidłowego zakresu palety zmienia sekwencję kolorów względem dawnego, częściowo niepoprawnego zakresu 128. Nie nadpisuje globalnej palety pulpitu.

## Testy

`test_plasma.py` kompiluje rzeczywisty poprawiony kod aplikacji z kontrolowanym zegarem i atrapą IPC. Sprawdza każdą fazę, każdy piksel wzoru i wariant mono/kolor wobec niezależnego wzoru referencyjnego. Sprawdza też pracę bez wejścia oraz zamknięcie przez WCLS. Wszystkie kontrole przeszły. Test nie udaje pomiaru Atari.

Kompilacja PLASMA.APP na m68k przeszła. Następnie na prywatnej kopii obrazu systemu uruchomiono aplikację w osobnym procesie Hatari. W kopii desktopu dodano wyłącznie testowy automatyczny start PLASMA, aby nie wymagać ręcznego klikania. Ta modyfikacja desktopu NIE znajduje się w patchu ani w dostarczonym PLASMA.APP. Trzy zrzuty co około dwie sekundy pokazały zmianę obrazu bez wejścia; dwa dołączono do raportu. Emulator testowy zatrzymano po próbie.

Nie zmierzono rzeczywistego FPS ani procentowego spadku użycia CPU. 10 FPS jest celem harmonogramu, nie gwarantowaną osiągniętą prędkością. Test mono wykonano na poziomie funkcji, nie w emulatorze. Interaktywne resize i przeciąganie nie były celem tej poprawki: płótno efektu pozostaje stałe 320×220 wewnątrz okna 340×260.

## Przekazanie DeepSeek

1. Porównać wersję bazową z source-manifest.json; zastosować plasma.patch na aktualnych zgodnych źródłach. Nie nadpisywać późniejszych zmian w ciemno.
2. Uruchomić `python3 test_plasma.py /ścieżka/do/projektu` i zbudować `make build/system/PLASMA.APP`.
3. Po zatrzymaniu emulatora zainstalować aplikację na kopii obrazu, w SYSTEM/APP. Gotowy PLASMA.APP jest zbudowany dla palety analizowanej wersji. Po zmianie icons_color.h przebudować aplikację.
4. Sprawdzić animację bez wejścia, zamknięcie przez X/Escape, responsywność pozostałych aplikacji oraz mono. Wersja okienkowa respektuje routing klawiatury systemu; ten test nie naprawia globalnego transportu myszy.

Dalsza większa optymalizacja wymaga wydajnej prezentacji małego bufora 8bpp jako jednej operacji, zamiast dziewięciu batchy prostokątów. Warto ją mierzyć osobno; nie jest potrzebna do usunięcia zatrzymania. Płynniejszą kolorystykę można uzyskać przez uzgodnioną paletę efektu, ale nie przez samowolną zmianę wspólnej palety pulpitu.

Robocze źródła DeepSeek i jego obraz systemu nie zostały zmienione. Pakiet jest gotowy do przeglądu i wdrożenia.
