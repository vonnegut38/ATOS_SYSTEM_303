# Końcowa weryfikacja pakietu

Archiwum sprawdzono ponownym odczytem: sumy ROM-u, HDD i zmienionych modułów zgadzają się z sha256.json. Audyt zamkniętego HDD: 136 plików, identyczne czyste kopie FAT, bez crosslinków; wyłącznie wcześniej znany osierocony klaster 437. Sprawdzono 22 binaria/pakiety względem plików budowania. Zmiany względem poprzedniego checkpointu są ograniczone do oczekiwanych modułów/pakietów; nowe pliki to dwie kopie z wcześniejszego testu Commandera.

Po wykonaniu archiwum ponownie uruchomiono ten sam profil Hatari. Pozostawiono Commander w tts1 i SYSINFO w tts2; Shift+Alt+F10 przełącza pomiędzy nimi. Oba pełne ekrany potwierdzono wizualnie.

Dodatkowa obserwacja do przyszłego testu: przy szybkim uruchomieniu SYSINFO w tts2 i Commandera w tts1, bez czekania na pełny ekran pierwszej aplikacji, Commander przez dwie obserwacje pokazywał PROCESS READY. Po przełączeniu do tts2 i z powrotem pełny ekran był gotowy. Nie rozstrzygnięto, czy był to tylko czas ładowania/planowania, czy odroczona prezentacja. Powtórzyć start aplikacji w krótkich odstępach i mierzyć czas do pierwszej ramki; nie nazywać tej obserwacji potwierdzonym zawieszeniem ani zamkniętą usterką.

Pakiet jest punktem kontrolnym po krótkich testach, nie deklaracją zakończenia całego planu stabilizacji.
