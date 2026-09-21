# Mono Commander — 2026-09-16

Natywna aplikacja `COMMAND.APP` dla Atari System, MC68000, mono 640×400.
Start w shellu: `run command.app`.

## Analiza Midnight Commandera

Przejrzane źródła upstream:
- https://github.com/MidnightCommander/mc/blob/master/src/filemanager/panel.c — oddzielny stan paneli, aktywny panel, widoczne wiersze, nazwa i rozmiar pliku.
- https://github.com/MidnightCommander/mc/blob/master/src/filemanager/file.c — rozdzielenie operacji na pliku i katalogu, kontrola istniejącego celu, potwierdzenia i błędy, sprawdzanie pustego katalogu.

MC korzysta z GLib, terminala, POSIX i swojej warstwy VFS. Pełny port tych zależności byłby dużym rozszerzeniem naszego małego systemu. COMMAND to własna implementacja inspirowana jego układem i skrótami, nie port kodu MC. Kod MC jest GPL-3.0-or-later; nie skopiowano go do aplikacji. Font i istniejące funkcje graficzne z EmuTOS pozostają opisane w licenses/emutos-NOTICE i licenses/emutos-GPL-2.0.txt.

## Obsługa

- Tab: aktywny panel; góra/dół: wybór; PgUp/PgDn: następna/poprzednia strona; Home: początek.
- Enter: wejście do katalogu; Backspace lub `..`: katalog nadrzędny.
- F5: kopiowanie wybranego pliku do katalogu drugiego panelu. Y potwierdza; Esc/N anuluje.
- F7: nazwa nowego katalogu 8.3, Enter tworzy, Backspace poprawia, Esc anuluje.
- F8: usunięcie pliku lub pustego katalogu, dopiero po Y.
- F10/Esc: wyjście; F1: krótka podpowiedź.

## Zakres i ograniczenia

Oba panele dotyczą aktualnie zamontowanego woluminu; nie ma przełącznika C:/D: ani kopiowania między woluminami. Nazwy 8.3, ścieżki do 63 znaków i do 8 poziomów. Kopiowanie pojedynczego pliku do 1 MiB; brak kopiowania rekursywnego, nadpisywania, przenoszenia, kosza, zaznaczeń grupowych i obsługi myszy. Pełny katalog nie jest automatycznie powiększany. Aplikacja przejmuje pełny ekran i wejście GUI; integracja jako okno MonoDesk pozostaje osobną pracą. Operacje dyskowe są synchroniczne — w trakcie kopiowania kernel nie przełącza zadań użytkownika.

## Zapis i obsługa błędów

Nowe syscalle: 49 copy (A0 źródło, A1 cel), 50 mkdir (A0 ścieżka), 51 delete (A0 ścieżka). Zero oznacza sukces; błędy FAT_WRITE_* są ujemne. Kernel sprawdza zakresy wskaźników przez istniejącą walidację user_path.

ACSI używa bufora DMA w ST-RAM, WRITE(6), a dla większych adresów prefiksu ICD i WRITE(10). Cache blokowy jest unieważniany przed zapisem. Testowany cel sprzętowy: STE/68000 w Hatari; inne CPU i prawdziwy sprzęt nie są zweryfikowane.

Metadane FAT są zapisywane do obu kopii, z ograniczonym dziennikiem cofania w RAM (128 sektorów, około 66 KiB). Dane kopii trafiają wyłącznie do nowo przydzielonych klastrów; wpis katalogowy publikowany jest na końcu. Brak miejsca i pojedynczy błąd zapisu cofają metadane; nieudane cofanie blokuje dalszy zapis do zamontowanego woluminu. Katalogi niepuste, pliki read-only/system i istniejące cele są odrzucane. Łańcuchy są sprawdzane pod kątem błędnych numerów i cykli. Nie jest to pełny fsck (nie wykrywa wszystkich krzyżowo połączonych plików).

To nie jest trwały dziennik transakcji: utrata zasilania w czasie operacji może zostawić uszkodzenia lub osierocone klastry. Znacznik dirty ustawiany jest przed zmianami; dirty/hard-error wyłącza zapis. Nie należy traktować wersji prototypowej jako magazynu jedynej kopii danych.

## Budowanie i testy

`gmake all check check-fat-write check-mono-font check-wm check-gui-apps`

- Test FAT uruchamia rzeczywisty kernel_fat.c na dysku w pamięci: kopia bajtów, katalogi zagnieżdżone, pliki puste, brak miejsca, cele istniejące, read-only, cykle, obie kopie FAT, BPB 512 i 1024 bajty. Wstrzykuje pojedynczy błąd na każdym zapisie copy/mkdir/delete i sprawdza cofnięcie metadanych.
- Test mono fontu: 32768 kombinacji przesunięcia, maski, kolorów i przezroczystości wobec referencji pikselowej, ASan/UBSan.
- `FATWRITE.APP`: test na emulowanym dysku przez rzeczywiste syscalle i ACSI. Tworzy MCPROBE, kopiuje KERNEL.SYS, porównuje całą zawartość, sprawdza odmowę nadpisania i usunięcia niepustego katalogu, usuwa własne dane i drukuje wynik.
- Walidacja formatu ASYSAPP0, sum kontrolnych, APW2 i wywołań PC-relative w plikach wykonywalnych.

## Wynik testów Hatari i audytu obrazu

Hatari 2.6.1, STE, MC68000 8 MHz, 4 MiB, monitor mono:
- FATWRITE.APP: `FATWRITE: ALL PASS`, kod zakończenia 0; kopia pliku KERNEL.SYS (69 189 bajtów) zgodna bajt po bajcie, katalog i kopia testowa następnie usunięte.
- COMMAND.APP: F7 utworzył TMP, Tab/strzałki/Enter przełączyły panele i katalogi, F5 skopiował DESKTOP.APP (2896 bajtów) do TMP, F8 usunął tylko tę kopię. Anulowanie dialogu pozostawiło dane. F10 wrócił do shella z kodem 0.
- Obraz po wyłączeniu emulatora: 91 plików; kopie FAT zgodne, flagi clean, brak nowych osieroconych klastrów i brak skrzyżowanych łańcuchów. Wszystkie pliki sprzed zmian mają identyczne SHA-256 poza celowo zaktualizowanym KERNEL.SYS.
- W obrazie pozostają puste katalogi MCM i TMP utworzone podczas prób.
- Szybkie serie klawiszy podczas rysowania/odczytu mogą być opóźnione lub pominięte; responsywność przy obciążeniu wymaga dalszego dopracowania. Font mono ma już szybką ścieżkę słowową, a wpisywanie nazwy odświeża sam dialog.
