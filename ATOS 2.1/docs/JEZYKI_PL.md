# ATOS 2.0 — Commander, SYSINFO i SETUP: EN / PL

Stan: 24 września 2026. Zmiany są w osobnym katalogu ATOS 2.0 i jego testowym obrazie HDD. Oryginalny commander-v10 nie był modyfikowany.

## Użytkowanie

W shellu wpisz `run setup.app`. Strzałkami wybierz **English** albo **Polski**, a następnie naciśnij **Enter**, aby zapisać. **Esc** zamyka SETUP. Wybór jest wspólny dla aplikacji i zachowuje się po restarcie systemu. Aktualnie zapisano EN.

Zamknij i uruchom ponownie Commander (`run command.app`) oraz SYSINFO (`run sysinfo.app`), aby wczytały wybrany język. Działająca aplikacja zachowuje własny słownik; zmiana ustawienia w innej sesji nie modyfikuje jej tekstów ani stanu.

Stan pozostawionego Hatari: tts1 — SYSINFO EN; tts2 — SETUP EN; tts3 — Commander EN (widoczny). Przełączanie: Shift+Alt+F10. Własny BIOS `build/bios_mste.bin`, profil `atos2.cfg`, STE/68030, 16 MHz, 4 MB, Shifter mono 640×400.

## Pliki na dysku Atari

- `\SYSTEM\LANG\EN.LNG` — angielski pakiet.
- `\SYSTEM\LANG\PL.LNG` — polski pakiet.
- `\SYSTEM\LANGUAGE.CFG` — dokładnie trzy bajty `EN` + LF albo `PL` + LF.
- `\SYSTEM\APP\SETUP.APP` — wybór języka aplikacji. To osobny program, niezależny od sprzętowego BIOS SETUP i jego pola language.

Format pakietu:

```text
ATOS-LNG-1
# Komentarz od początku linii
CMD_COPY=F5 Copy
CMD_CANCEL=Esc: Cancel
SYS_MACHINE=Machine
SET_LANGUAGE=Application language:
```

Klucze są stałe i rozróżniają wielkość liter. Wartości po `=` zachowują spacje. Obsługiwane są LF i CRLF; sekwencja `\n` oznacza nową linię, a `\\` pojedynczy ukośnik odwrotny. Maksymalny rozmiar pliku: 12000 bajtów; wartość: 78 znaków po rozwinięciu escape. Poszczególne pola ekranu mogą być krótsze — zachowuj krótkie etykiety i skróty F1–F10. Tekst nie jest interpretowany jako format printf ani kod.

Kodowanie to ASCII. Polska wersja zachowuje dotychczasowe teksty bez ą/ę/ł itd.; obsługa tych znaków wymaga osobnego, uzgodnionego mapowania czcionki terminala. Plików nie zapisuj jako UTF-8 z BOM.

Brakujący wpis korzysta z angielskiego tekstu wbudowanego w aplikację. Nieznane klucze są dozwolone. Powtórzony znany klucz, zły nagłówek, niedozwolony znak, błędny escape lub przekroczenie limitu odrzuca pakiet i przywraca cały angielski słownik. Nieprawidłowa konfiguracja języka oznacza EN. SETUP nie zapisuje wyboru pakietu, którego nie może poprawnie wczytać; zgłasza też błąd zapisu konfiguracji.

Tłumaczenia można zmienić, podmieniając sam plik LNG na dysku, bez przebudowy aplikacji. Każdy proces odczytuje pakiet tylko przy uruchomieniu. SETUP wczytuje go również po zmianie zaznaczenia, aby pokazać podgląd.

## Kod i utrzymanie dla DeepSeek

- `languages/catalog.json` — źródło dostarczanych tłumaczeń i stabilnych kluczy.
- `tools/make_languages.py` — generuje `languages/EN.LNG`, `PL.LNG` i angielski słownik awaryjny `include/lng_catalog.h`. Uruchamiać po zmianach katalogu źródłowego; nadpisuje wygenerowane pliki.
- `include/app_lng.h` — wspólny ograniczony parser i odczyt przez istniejące stat/open/read/close. Osobny bufor i indeksy w każdym procesie. Brak globalnych wskaźników do relokowanych napisów.
- `system/commander.c`, `system/sysinfo.c` — wszystkie polskie teksty interfejsu, statusów, dialogów i komunikatów błędów zastąpione kluczami. Nazwy urządzeń, jednostki, nazwy plików i techniczne znaczniki pozostają niezmienne.
- `system/setup_app.c` — natywna aplikacja wyboru języka, klawiatura i mysz. Pola przycisków mają stałe obszary trafienia; etykiety SYSINFO są rysowane osobno, żeby długość tłumaczenia nie przesuwała funkcji myszy.
- `include/kernel_language.h`, syscall **59** — argument0: 0=EN, 1=PL; wynik 0 lub ujemny błąd. Zapisuje wyłącznie istniejący trzybajtowy LANGUAGE.CFG, kontroluje rozmiar/atrybuty i pełny zapis. Nie dodaje ogólnego zapisu dowolnego pliku; używa transakcyjnego `kfat_write_path_at`.
- `tools/build_atos2.py --install` — kompilacja, testy i instalacja przy zamkniętym Hatari. Instaluje Commander i SYSINFO zarówno w SYSTEM, jak i SYSTEM/APP (shell znajdował wcześniej starą kopię SYSINFO w SYSTEM). SETUP trafia do SYSTEM/APP, pakiety do SYSTEM/LANG. LANGUAGE.CFG powstaje tylko, jeśli go brakuje; kolejne instalacje zachowują wybór użytkownika (`--if-missing`). Aktualizacja pakietów nadpisuje dostarczane EN.LNG/PL.LNG na obrazie — zachowaj własne edycje przed aktualizacją.

Dodanie kolejnego języka wymaga rozszerzenia wyboru w SETUP, identyfikatora w konfiguracji, mapowania ścieżki i walidacji syscallu 59. Zmiana istniejących tekstów EN/PL nie wymaga takich zmian. Nie zmieniaj kluczy przy zwykłej korekcie tłumaczenia.

## Weryfikacja

Końcowe `tools/build_atos2.py --install`: PASS. `gmake check`: 20 testów Python i zestawy C. Dodatkowo check-terminal, check-console, check-sessions, check-commander, check-sysinfo, check-languages: PASS, testy C z AddressSanitizer i UndefinedBehaviorSanitizer.

Nowe testy sprawdzają kompletność obu pakietów dla trzech aplikacji, fallback, zachowanie spacji, CRLF, escape, duplikaty, kontrolne/niedozwolone znaki, przepełnienia, krótkie odczyty, zamykanie uchwytów, ograniczony zapis ustawienia, błędy zapisu, podgląd i anulowanie SETUP. Test BIOS-u sprawdza oba egzemplarze FAT oraz błędy I/O.

Hatari: potwierdzono zapis PL i EN w SETUP, odczyt wyboru po ponownym uruchomieniu programu i zachowanie EN po restarcie emulatora. Commander wyświetlił oba języki, w tym angielski dialog nowego katalogu. Zaktualizowane SYSINFO wyświetliło PL i EN; w EN sprawdzono wszystkie trzy strony i zakończenie benchmarków CPU, ST-RAM i odczytu dysku; brak TT-RAM został poprawnie opisany. Przełączanie między działającymi aplikacjami odtwarzało ich teksty. Nie jest to długotrwały test stabilności całego systemu.

## Dodatkowa poprawka BIOS/FAT

Podczas kontroli obrazu znaleziono istniejący błąd `hw/config.c::cfg_fat_put`: alokacja BIOS/BIOS.CFG zmieniała tylko pierwszy FAT. Poprawka odczytuje liczbę kopii i ich rozmiar z BPB oraz aktualizuje obie kopie. Nowy BIOS został zbudowany i uruchomiony w Hatari. Nie dodano pełnego dziennika transakcji do starego zapisu BIOS-u; przerwanie zasilania/błąd drugiego zapisu nadal wymaga kontroli dysku.

W testowym obrazie zidentyfikowano klastry 353 i 354 jako katalog /BIOS i plik /BIOS/BIOS.CFG. Po zamknięciu Hatari skorygowano wyłącznie odpowiadające im cztery bajty w FAT2, według zweryfikowanego FAT1. Kopia sprzed naprawy: work/fat2-before-bios-mirror-repair.bin. Kontrola po naprawie: zgodne kopie FAT i czyste flagi, bez krzyżujących się łańcuchów. Obraz ma również jeden osierocony klaster 437, zaobserwowany przed tą naprawą; nie był automatycznie zwalniany.

## Otwarty problem do następnego etapu

> **Aktualizacja 2026-09-26:** opisany niżej `PROGRAM EXIT: 0`/prompt podczas
> działania aplikacji **nie występuje** w obecnym kodzie —
> `shell_foreground_attached` wstrzymuje prompt po starcie foregroundu, a string
> `PROGRAM EXIT` był martwy (usunięty). Weryfikacja runtime: `run SYSINFO.APP`
> → `PROCESS READY, PID: 4`, po 2/4/6 s brak promptu. Pozostaje **artefakt
> wejścia** (gubione/powielane znaki, np. `rrun` zamiast `run`). Do
> rozróżnienia: zdarzenia emulatora, autorepeat i błędy wejścia OS.

Przy uruchamianiu aplikacji shell potrafi wypisać `PROGRAM EXIT: 0` i swój prompt na jej pierwszym ekranie, mimo że aplikacja nadal działa. *(historyczne — patrz aktualizacja wyżej)* Zaobserwowano to także dla SETUP i SYSINFO. Pojedynczy Down w Commanderze lub F1 w SYSINFO spowodował odrysowanie działającej aplikacji.

Następny audyt powinien objąć ścieżkę RUN w `system/shell_module.S` (wywołanie wejścia aplikacji, obsługa wyniku, zwalnianie obrazu/stosu i wypisywanie PROGRAM EXIT) w relacji do asynchronicznego planisty oraz właściciela ekranu. Nie maskowano tego problemu okresowym odświeżaniem aplikacji. Automatyczne wpisywanie przez CUA również sporadycznie gubiło/powielało znaki; rozróżnić zdarzenia emulatora, powtarzanie klawiszy i błędy wejścia OS.

Wcześniejsze otwarte sprawy stabilności, ramek wyjątków 030, FAT/VOFA i TOS pozostają opisane w poprzednich raportach. Ta zmiana dostarcza obsługę języków, nie deklaruje pełnej stabilności ATOS.
