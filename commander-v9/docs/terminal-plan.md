# Plan terminala znakowego — stan v7

1. Wspólny model terminala 80×25: znak + atrybut, niezależny kursor, przewijanie, operacje tekstowe i własna semigrafika. Stan należy do instancji, bez wspólnych globalnych zmiennych kursora.
2. Renderer mono 640×400: porównanie komórek z ostatnim widokiem; zapis wyłącznie zmienionych par znaków bez kopiowania całej bitmapy. Ramki są znakami fontu.
3. Bezpieczne przekazanie 2000 komórek przez SYSTEM.GFX, z kontrolą właściciela pierwszego planu i pełnego zakresu pamięci procesu. Zachowanie istniejących klientów graficznych.
4. Commander używa komórek terminala, zachowuje wejście klawiaturą/myszą i funkcje FAT/kosza. Wskaźnik myszy pozostaje nakładką systemową.
5. Testy modelu, renderera, izolacji dwóch instancji, zgodności obrazu i obsługi Commandera. Natywne testy terminala i kosza oraz kontrola w Hatari na osobnym obrazie.

Zakres etapu: terminal i pierwszy klient. Istniejący BIOS/boot-console oraz shell pozostają zgodne. Uruchamianie wielu shelli, skróty tts1/tts2, przekierowanie stdout i priorytety grup procesów to następny etap — same osobne bufory terminala jeszcze tego nie implementują.

## Stan wdrożenia

Model, renderer, kontrola pamięci w brokerze i migracja Commandera są wykonane. Testy hosta przechodzą; w Hatari STE/68000 8 MHz, 4 MB, mono test TERMTST potwierdził 1000/0/1 zmienionych par znaków, a TRASHTST przenoszenie/przywracanie/trwałe usuwanie poddrzewa. Szczegóły końcowej weryfikacji są w README-terminal-v5.md przy obrazie.

## Dalsza kolejność

### Uzgodniona obsługa konsol

- Alt+Shift+F10 przełącza aktywną konsolę kolejno tts1 → tts2 → … → tts1. Zwykłe F10 nie jest skrótem przełączania konsol.
- Każdy prompt zaczyna się identyfikatorem konsoli, np. `tts1 C:\>` lub `tts2 C:\SYSTEM>`; numer pochodzi z tożsamości terminala, niezależnie od aktualnego katalogu.
- Przełączenie zachowuje uruchomione aplikacje, zawartość terminala i rozpoczętą linię polecenia. Nie wolno implementować tego jako zamknięcia Commandera. EXIT dodatkowego shella pozostaje osobną operacją.
- Skrót należy przechwytywać systemowo przed przekazaniem klawiszy aplikacji, rozpoznawać oba Shifty i nie powtarzać przełączenia na autorepeat. Zdarzenie F10 należące do skrótu nie może trafić do Commandera jako polecenie zakończenia.
- Powyższe stanowi wymaganie następnego etapu. Obraz v6 nadal ma jeden shell i dotychczasowe F10 kończące Commandera; przełączanie tts1/tts2 nie jest jeszcze wdrożone.

### Wykonane w v6

Zrealizowano podłączenie zwykłego wyjścia shella, modułów i aplikacji do terminala mono. Kernel przejmuje callbacki po wczesnej diagnostyce, grupuje krótkie zapisy i odświeża ekran w pętli obsługi. Długie polecenia wymuszają odświeżenie po co najmniej 320 znakach między odświeżeniami. Tekst podczas pracy desktopu pozostaje w komórkach i jest odtwarzany przy powrocie. Konsola ROM pozostaje awaryjna oraz jako fallback poza mono. Stan wejścia shella i jego bieżący katalog nadal należą do jednej instancji; następne są punkty 2–4 poniżej.

1. Podłączyć zwykłe wyjście shella i aplikacji do instancji terminala: tekst, kursor, przewijanie, obsługa Backspace/Tab. Zachować małą konsolę awaryjną dostępną przed inicjalizacją usług.
2. Dodać tablicę terminali i przypisanie procesów do terminala. Wyjście procesów w tle zmienia ich bufor, bez rysowania na ekranie aktywnej konsoli. Przełączenie terminala odrysowuje różnice lub pełny widok po zmianie trybu.
3. Dopiero potem osobne shelle tts1/tts2: niezależne wejście, bieżący katalog i stan edycji, skróty przełączania, dziedziczenie terminala przez dzieci oraz porządkowanie po EXIT.
4. Dodać ważenie czasu CPU grup terminali z gwarancją postępu w tle. Sprawdzić obciążenie CPU, oczekiwanie na I/O i wiele procesów w jednej konsoli; 5–10% nie może być wyłącznie stałym opóźnieniem shella.
5. Zmierzyć czas reakcji całych operacji na 8 MHz. Obecne liczniki zapisów pikseli nie mierzą opóźnień FAT, IPC, schedulera ani emulowanego dysku.


## Wykonane w v7 — konsole tts1/tts2/tts3

Wdrożono trzy bufory terminala oraz trzy konteksty modułu SHELL.SYS: katalog, edytowana linia, historia i stan poleceń. Są to konteksty wspólnego modułu shella, nie trzy osobne procesy użytkownika. Procesy aplikacji mają identyfikator terminala; dzieci i zadania kolejkowane go dziedziczą. Wyjście i powiadomienia o zakończeniu należą do odpowiedniej konsoli, a nieaktywna konsola nie odbiera klawiatury.

Alt+Shift+F10 cyklicznie wybiera aktywne konsole i nie przekazuje F10 aplikacji. Polecenia tts1, tts2, tts3 wybierają lub tworzą sesję. EXIT zamyka dodatkową sesję bez aktywnych procesów i zadań oczekujących; tts1 pozostaje otwarta. Numer jest w prompcie i systemowym znaczniku w prawym górnym rogu. Menu pokazuje inne aktywne konsole oraz „[+] Nowy”, gdy jest wolny slot. Esc lub kliknięcie poza menu je zamyka.

Nakładka zachowuje zasłonięte piksele; rysowanie aplikacji usuwa ją przed zmianą tła, a pętla systemowa odtwarza ją po aktualizacji. Niezmieniona nakładka i nieruchomy kursor nie są ponownie rysowane. Przełączenie zachowuje jedyny desktop graficzny, więc można zostawić Commandera w jednej sesji i pracować tekstowo w pozostałych.

Pozostają: wiele jednoczesnych desktopów, ważenie CPU konsol 5–10% w tle oraz montowania niezależne od sesji. Zmiana wolumenu przy wielu aktywnych konsolach jest zablokowana, bo montowanie FAT nadal jest globalne. Scheduler nadal używa round robin. Tryby kolorowe nie były celem tego etapu.


## Etap v8 — VOFA/ET4000

Wydzielono wybór bufora i prezentację do kernel_video.c / video_bus.S.
VOFA ma sterownik mono 640×480 na podstawie EmuTOS oraz cache zmian
640×400 w ST-RAM. Konfiguracja SYSTEM/VIDEO.CFG jest jawnie opt-in.
Wersja startowa: 68000, BIOS w mono, ST-RAM poniżej 12 MB.
Testy hosta i fallback w Hatari wykonane; obraz VGA na realnej karcie
wymaga pierwszej próby. Kolejne kroki: niezależny start przy kolorowym
wyjściu Shiftera, pomiar transferu VRAM, dopiero potem okna i kolory.
