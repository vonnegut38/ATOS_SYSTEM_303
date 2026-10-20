# ATOS 030 — zakres prac i kolejność wdrażania

Status: plan po audycie 24.09.2026. Żaden pakiet B/R/G nie został jeszcze wdrożony. Każdy pakiet powinien stanowić osobną, odwracalną zmianę, zakończoną testami i krótkim wpisem do dziennika.

## Decyzja o zakresie

Wykorzystujemy istniejący kernel jako bazę warunkową. Zachowujemy obecne formaty aplikacji i modułów oraz funkcje, dla których są dowody działania. Najpierw stabilizujemy podstawy i wydzielamy natywną ścieżkę startu. VM, ET4000 8 bpp i desktop są późniejszymi, oddzielnymi etapami.

Poza zakresem najbliższych prac: EmuTOS jako nowa baza, MagiC/KolibriOS, ST Box, emulacja Shiftera, pełna zgodność GEMDOS/AES/VDI, nowy format wykonywalny, akceleracja W32i, duża reorganizacja katalogów, estetyka pulpitu, nowe aplikacje użytkowe.

## Pakiet bazowy — proponowany do wykonania przed przekazaniem

### B0 — jednoznaczna baza i profil uruchomienia

**Cel:** następny wykonawca uruchamia ten sam system, który jest testowany.

- Zachować lokalne zmiany użytkownika; nie robić `git add .` w wieloprojektowym korzeniu.
- Wybrać główną konfigurację emulatora 030 i oddzielną konfigurację regresyjną 68000. Docelowy akcelerator/adapter/RAM pozostają pytaniem do użytkownika.
- Utworzyć powtarzalny sposób przygotowania nowego obrazu z odtworzonym BOOT/KERNEL i modułami; zawsze kopia źródłowego obrazu.
- Zapisać komendy, wersję emulatora, konfigurację, SHA-256 obrazów i wyniki startu.
- Wyraźnie rozróżnić natywny start oraz demo EmuTOS. Nie zmieniać domyślnej ścieżki użytkownika bez opisania skutku.

**Akceptacja:** czysty build; dwa powtarzalne starty natywnej konfiguracji; potwierdzony adres/rozmiar ładowanego kernela; artefakty i logi pozwalające powtórzyć próbę. Jeżeli dostępne Hatari nie emuluje konkretnego adaptera, test karty pozostaje oznaczony jako brak weryfikacji, bez udawania PASS.

**Ryzyko:** średnie. **Wykonawca:** dowolny model z dostępem do narzędzi; przegląd konfiguracji przez osobę znającą docelowy sprzęt.

### B1 — naprawa terminala i kontraktu geometrii

**Pliki:** system/text_terminal.c, include/text_terminal.h tylko gdy konieczne, tests/test_text_terminal.c, ewentualnie testy console/Commander.

**Problem:** F01, odczyt poza cells podczas scroll i sprzeczne stride 80/cols.

**Zakres:** ustalić kontrakt układu komórek zgodny z istniejącymi odbiorcami; poprawić przewijanie/clear/fill/text; dodać regresje dla pełnej i zwężonej geometrii, bez zmiany wyglądu aplikacji. Nie usuwać obsługi zmiennego rozmiaru tylko dla obejścia błędu.

**Akceptacja:** ASan/UBSan bez błędów; prawidłowe scroll/wrap/clear przy 80×25, 80×18 i węższej konsoli; PASS `check-terminal`, `check-commander`, `check-gui-damage` i istniejących testów renderowania. Rozwiązać harness `check-console` w B2, jeśli blokuje porównanie.

**Ryzyko:** niskie–średnie. **Koszt kontekstu:** mały. Dobry pierwszy pakiet również dla DeepSeek.

### B2 — wiarygodny zestaw testów i kontrakty API

**Pliki:** tests/test_cursor_words.py, test_session_overlay.py, test_user_sleep_wakeup.py, test_sysinfo_api.py, test_kernel_console.c, test_video_output.c, test_fat_write.c i dokumentacja kontraktów. Kod produkcyjny tylko wtedy, gdy test ujawni uzasadniony błąd.

- Uzupełnić zastępcze zależności harnessów zgodnie z rzeczywistymi interfejsami.
- Test SysInfo oprzeć na aktualnym ABI 412 B i sprawdzić odrzucenie niepoprawnego rozmiaru/zakresu.
- Wyjaśnić wymaganie wyrównania VRAM: operacje bajtowe i aktualny interfejs nie uzasadniają automatycznie starej asercji. Sprawdzić wszystkich odbiorców.
- Udokumentować semantykę kopiowania istniejącego pliku. Aktualna implementacja dopuszcza overwrite. Jeżeli zachowujemy to zachowanie, testować zawartość, FAT, awarie i rollback przy nadpisaniu. Nie zmieniać semantyki użytkowej bez uzasadnienia.
- Uruchomić także test_video_boot, którego poprzedni test nie dopuścił do wykonania.

**Akceptacja:** pełne `gmake check` oraz dodatkowe wskazane targety przechodzą; brak wyłączania testów i obniżania sanitizers; raport osobno wskazuje poprawione harnessy i błędy produkcyjne. Zielone `check-exception` nie zamyka B3.

**Ryzyko:** średnie. **Koszt kontekstu:** mały przy rozbiciu na harnessy, SysInfo, FAT i video. DeepSeek może wykonać po jednym podzadaniu.

### B3 — poprawna diagnostyka wyjątków 030

**Pliki:** include/exception_format.h, ui/exception_report.c, tests/test_exception.c, boot/exception.S i system/kernel_entry.S tylko w części capture/report; system/kernel_exception.c tylko raport.

- Opisać rzeczywisty format wejściowej ramki i rozróżnienie CPU.
- Poprawić odczyt SR/PC/formatu/offsetu wektora oraz raportowanie numeru wektora.
- Obsłużyć potrzebne ramki 030 A/B bez zgadywania formatu po SR.
- Nie czytać poza krótką ramką; nie kopiować bezwarunkowo 24 B bez wiedzy o długości.
- Naprawić `kernel_fault_sr`, zachować pełne dane potrzebne diagnozie.
- Oddzielić błąd programu od błędu kernela i recovery sond sprzętu.

**Akceptacja:** dostarczony `repro/exception_frame_probe.c` przestaje wykazywać błąd; testy referencyjnych ramek 68000/030 z manuala, poprawny SR/PC dla illegal instruction i bus error w emulatorze. Bez wdrażania VM ani zmiany schedulera.

**Ryzyko:** wysokie. Zalecany przegląd różnic przez doświadczonego wykonawcę przed dalszymi pracami. Nie powierzamy modelowi zadania „napisz obsługę 68030” bez tych ograniczeń.

### B4 — natywna ścieżka startu bez wymagania persony TOS

**Pliki:** kernel_main.c, kernel_entry.S, Makefile, mały interfejs rejestracji persony/konfiguracja startowa; kernel_memory.c wyłącznie jawna polityka rezerwacji.

- Oddzielić instalację trapów zgodności i hooków persony od mechanizmów natywnych.
- Timer natywny ma działać bez `tos_task_tick` i `tos_composite_tick`; zachować opcjonalną ścieżkę legacy.
- Wyłączyć debug autostart poprzez jawny przełącznik, nie nazwę `NOSYS.PRG`.
- Udokumentować i odseparować rezerwację TPA od ogólnej polityki pamięci, bez przypadkowej zmiany obrazu legacy.
- Zapewnić tryb diagnostyczny startujący bez desktopu; nie usuwać shella.

**Akceptacja:** natywny build/start bez wymagania tos_prg/tos_vdi w rdzeniu, jedna natywna aplikacja startuje i kończy się z kodem wyniku; druga próba działa po zakończeniu pierwszej; timer i input działają; wariant legacy pozostaje odtwarzalny. Testy PROC.SYS nie są dowodem natywnego procesu — pokazać konkretny ASYSAPP0.

**Ryzyko:** wysokie. To najbardziej wartościowy punkt przekazania po uporządkowaniu podstaw.

## Rekomendowany punkt przekazania DeepSeek

Po B0–B4 przekazać: commit lub zachowaną kopię plus patch, aktualne obrazy z hashami, komendy uruchomienia, dziennik testów, mapę ABI i listę pozostałych ograniczeń. Nie trzeba wcześniej wdrażać MMU ani desktopu, żeby przekazanie było sensowne.

Oszczędniejszy wariant: przekazać już teraz B1 i część B2, a kosztowniejszą analizę zostawić na przegląd B3/B4. Wybór modelu nie jest oceną jakości kodu: akceptacja wynika z testów i przeglądu.

## Dalsze etapy — nie łączyć z pakietem bazowym

### R1 — allocator i rezerwacje pamięci

Zachować publiczne API kmem. Dodać walidację zakresów i przepełnień, jasne zachowanie dla uszkodzonych nagłówków oraz wydzielić rezerwacje sprzętowe. Testować alokacje/granice/fragmentację/free błędnych wskaźników na kontrolowanej puli. Nie wprowadzać VM w tym samym zadaniu. Warunek: znana mapa RAM i brak nakładania się rezerwacji/DMA/VRAM.

### R2 — minimalne PMMU 030

Osobna specyfikacja mapowań: kernel/supervisor, user, stosy, strony read-only, urządzenia cache-inhibited, tablice i synchronizacja ATC. Na początek stałe mapowania i kontrolowane błędy dostępu. Bez swap/demand paging/fork/COW/ST Box. Kryterium: aplikacja nie zapisuje kernela; kontrolowany błąd nie niszczy systemu; dwie przestrzenie nie widzą swoich prywatnych danych. Warunek wejścia: B3, B4, R1 i profil rzeczywistej PMMU.

### R3 — kwalifikacja kontekstu i zasobów

Zachować istniejący natywny scheduler, sprawdzić współpracę z R2: dwa procesy, sleep/wake, IPC, exit/wait, kill i utratę zasobów. Zdefiniować politykę FPU — do czasu pełnej obsługi nie deklarować bezpiecznego współdzielenia FPU. Oddzielić procesy od GUI/session przez małe interfejsy. Testy mierzą postęp i cleanup, a nie tylko brak crasha.

### S1 — wspólny transport storage

Wydzielić bezpośrednie ACSI z kernel block do sterownika, zachowując cache i FAT. Osobno rozstrzygnąć flush i serializację dostępu. Testować awarie zapisu/rollback wyłącznie na kopiach obrazów. Nie wymieniać systemu plików.

### G1 — kontrakt powierzchni i prezentacji

Zdefiniować stride, pojemność, format, clipping, nakładający się blit, właściciela bufora, zatwierdzanie zmiany trybu i przejęcie awaryjne. Usunąć pozorne sukcesy set_mode bez poprawnego bufora. Podłączyć istniejący Shifter/VOFA mono jako dowód całej ścieżki. Nie dodawać jeszcze widgetów.

### G2 — ET4000 8 bpp dla konkretnego adaptera

Dopiero po wyborze sprzętu: rejestry/tryb/paleta/VRAM/bankowanie lub liniowy dostęp, poprawne mapowanie i test ekranowy. Najpierw programowe wypełnienia/kopiowanie/kursor; W32i acceleration oddzielnie. Kryterium: wzorce testowe bez przekroczeń VRAM, poprawny pitch i paleta, brak zapisów w trybie idle, kontrolowana obsługa braku karty. Test fake-I/O nie zamyka zadania sprzętowego.

### G3 — małe natywne GUI i desktop

Zachować sprawne WM_core i blit. Jedno okno z input/odrysowaniem/close, następnie dwa procesy z oknami. Dopiero potem desktop/launcher. Aplikacja awaryjna ma dać się zamknąć bez utraty shella i drugiego procesu. Bez zgodności AES/VDI na tym etapie.

## Minimalny graf zależności

B0 → B1/B2 → B3 → B4 → R1 → R2 → R3.

G1 można prowadzić po bazie niezależnie od pełnego VM; G2 wymaga G1 i profilu sprzętu; G3 wymaga stabilnych procesów i grafiki. S1 prowadzić jako oddzielny pakiet. Nie utrzymywać jednocześnie kilku konfliktujących zmian w tych samych plikach.

## Zasady kontroli kosztu

- Jeden prompt = jeden zamknięty pakiet, zwykle 2–5 plików; większy pakiet dzielić.
- Czytać najpierw handoff i kartę zadania, potem tylko wskazane pliki; pełny audyt wyłącznie w razie nowej sprzeczności.
- Nie zlecać „dokończ cały system” ani automatycznej poprawy wszystkich testów.
- Zapisywać wynik: zmiany, komendy i wyniki, pozostałe ograniczenia, następny krok.
- Uruchamiać testy adekwatne do zmiany; pełny zestaw na granicy pakietu, nie po każdej kosmetycznej edycji.
- Przeglądać zmiany krytyczne: assembler, IRQ, pamięć, MMU, FAT-write. Wynik modelu i zielona kompilacja nie są samodzielną akceptacją.
- Nie podawać wyceny całego systemu przed B0–B4. Koszt zależy głównie od kwalifikacji sprzętu i liczby regresji, nie liczby katalogów.
