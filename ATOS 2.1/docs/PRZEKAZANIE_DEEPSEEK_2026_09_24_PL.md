# ATOS 2.0 — punkt przekazania po poprawkach wejścia

24.09.2026. Ten dokument jest aktualniejszy niż wcześniejsze przekazania w docs/ i docs/audit/. Projekt: `/Users/xxx/Documents/Codex/ATOS 2.0`. Oryginału commander-v10 nie modyfikować. Nie ma repozytorium Git; bieżące archiwum i patch służą jako punkt kontrolny.

## Cel i granice

Stabilny natywny system z trzema niezależnymi sesjami tekstowymi, Commanderem i SYSINFO, uruchamiany na własnym BIOS-ie. Nie przepisywać systemu, schedulera ani GUI w jednym etapie. Nowy desktop, port KolibriOS, podmiana rdzenia na MagiC/EmuTOS i MMU pozostają poza bieżącym zakresem. Użytkownik może przekazać dalszą pracę DeepSeekowi ze względu na koszty; pracować małymi, sprawdzalnymi zmianami.

## Stan funkcjonalny

- Niezależne bufory i odtwarzanie ekranów tts1/tts2/tts3; Shift+Alt+F10 przełącza istniejące sesje.
- RUN uruchamia proces foreground przez kolejkę STARTFG; aplikacja nie otrzymuje fałszywego EXIT po kwancie. GUI jest własnością sesji/procesu i jego uprawnionych potomków.
- Usunięto drugi parser surowego IKBD z obsługi aplikacji. Bajty pakietów myszy nie mogą być interpretowane tam jako Esc/F10.
- Commander: podwójna ramka aktywnego panelu; +/− (także numpad) zaznacza/odznacza plik; gwiazdki; niezależne zaznaczenia paneli, maksymalnie 256 plików. F5 kopiuje zaznaczoną grupę po potwierdzeniu; błąd zatrzymuje kopiowanie i zachowuje pozostałe zaznaczenia. Szczegóły: docs/COMMANDER_NORTON_PL.md.
- EN.LNG/PL.LNG i wybór języka w SETUP.APP; preferencja zapisywana. Języki obejmują Commander i SYSINFO.
- Poprawione lustra FAT podczas alokacji BIOS-u. Pozostał wcześniejszy osierocony klaster 437; nie usuwać go bez osobnej analizy.
- Poprawiony dekoder ramek wyjątków 68030 i containment błędów aplikacji. Poprzedni etap potwierdził 11 przypadków EXWAIT oraz odzyskanie pamięci. Nie oznacza to pełnej kwalifikacji SR/PC, supervisor faults, innych CPU lub ochrony pamięci.

## Bieżąca zmiana: utrata wejścia pod obciążeniem

INPUTSTAT ma dodatkowe liczniki:

- RAW: przepełnienie pierścienia surowych bajtów.
- ACIA: zaobserwowane przepełnienie odbiornika ACIA.
- KEY/MOUSE: nieudane przekazanie zdarzenia.
- QUAR: bajty odrzucone w kwarantannie po utracie synchronizacji.
- PARTIAL: wygasłe niekompletne pakiety myszy.

IKBD DROPPED zachowuje poprzednią semantykę sumy błędów. EVENTS PENDING to chwilowa liczba oczekujących zdarzeń, nie licznik strat. Statystyki zerują się po restarcie modułu/systemu.

Reprodukcja przed poprawką: świeży start, TTS2, RUN SYSINFO.APP, poczekać na pełny ekran, F5, Shift+Alt+F10 do tts1, po około 2–3 sekundach wpisać INPUTSTAT. Pierwsze i znikało; shell widział NPUTSTAT. Powtórzone INPUTSTAT pokazywało RAW=0 ACIA=1 KEY=0 MOUSE=0 QUAR=2 (lub 3) PARTIAL=0.

Rozwiązanie ogranicza przerwy w odbiorze przy pracy supervisor z zamaskowanym IRQ6:

1. kernel_ikbd_poll_capture chwilowo podnosi IPL do 7, odkłada status/bajt ACIA do istniejącego pierścienia i przywraca SR. Chroni to odczyt i producenta kolejki przed wyścigiem z IRQ6.
2. Oczekiwanie ACSI na IRQ (odczyt ASM i zapis C) przechwytuje dostępne bajty.
3. Renderer tekstowy wywołuje opcjonalny callback co osiem par znaków (125 razy na pełny ekran), także gdy cache nie wymaga zmiany pikseli.
4. Każda z 32 iteracji głównej pętli konsumentów zdarzeń wykonuje przechwycenie surowych bajtów.

Parser nadal jest jeden: INPUT.SYS. Callbacki nie parsują wejścia ani nie wysyłają zdarzeń. Nie zmieniono globalnej polityki SR aplikacji ani maskowania przerwań supervisora.

ABI: INPUT v1 wydłużono z 20 do 24 bajtów o statistic(index) na offset 20; SHELL sprawdza size. API kernela v3 ma opcjonalny callback ikbd_capture na offset 172, rozmiar 176; ACSI sprawdza size przed użyciem. Nie przesuwać istniejących pól.

Ważne wyniki pośrednie: samo odpytywanie ACSI NIE naprawiło reprodukcji. Dodanie renderera dało jeden sukces, potem ten sam błąd. Dopiero końcowy wariant z odczytem pomiędzy konsumentami przeszedł trzy kolejne próby. Nie przypisywać sukcesu jednej wcześniejszej zmianie; nie wykonano pełnej izolacji wpływu każdego składnika.

## Weryfikacja końcowego wariantu

- python3 tools/build_atos2.py: PASS, 22 testy Python oraz zestawy C, terminal/console/sessions/commander/sysinfo/languages, z odpowiednimi ASan/UBSan. Log: work/input-load-pump-build.log.
- Nowy test tests/test_disk_input_poll.py: rzeczywisty kod kolejki i C oczekiwania dysku z podstawionym MMIO; kolejność bajtów, zawijanie indeksów między seriami i timeout. Nie emuluje maskowania SR ani ASM ACSI.
- Test rasteryzatora: 1000 par, 125 callbacków i zgodność obrazu. Test odtwarzania sesji korzysta z nowego API.
- Hatari 2.6.1, STE/68030, 16 MHz, 4 MiB, mono 640×400, własny ROM. Końcowy KERNEL.SYS: 101556 bajtów.
- Trzy kolejne F5 w SYSINFO → powrót do shella → INPUTSTAT: całe polecenie, wszystkie liczniki strat 0.
- Commander tts1 + SYSINFO tts2: dwa cykle benchmark/przełączenie; zachowane dwa zaznaczenia, panel i ramki. F10 zakończył Commandera pojedynczym EXIT 0. Liczniki strat 0.
- Następnie shell tts1 + SYSINFO tts2 + Commander tts3: pełny cykl, kolejny benchmark, zachowany kursor Commandera. Końcowo wszystkie liczniki strat 0, EVENTS PENDING=0.
- Są to krótkie próby funkcjonalne, nie wielogodzinny stress ani test fizycznego Atari. Nie deklarować pełnej stabilności lub usunięcia każdej możliwej straty wejścia.

## Budowanie i uruchomienie

Przy zamkniętym Hatari:

    cd '/Users/xxx/Documents/Codex/ATOS 2.0'
    python3 tools/build_atos2.py --install

Skrypt buduje i testuje, blokuje HDD i instaluje także kopie aplikacji w SYSTEM/APP. EXWAIT i próbki błędów są już na obrazie, ale standardowy instalator ich nie aktualizuje; po ich zmianie instalować osobno. Nie używać starych celów image-v10*.

Hatari: File → Load Config → atos2.cfg → Reset. Pasek ma pokazywać 16MHz / 030 / STE / MONO. Domyślny profil programu może być błędny. ROM: build/bios_mste.bin; HDD: work/atos-v2.img. Po przeniesieniu archiwum poprawić bezwzględne ścieżki w atos2.cfg. Bezpośredni launcher run-atos2.sh nie ma potwierdzonego udanego startu w środowisku Codexa; używano UI aplikacji macOS.

## Najbliższy zakres dla DeepSeeka

1. Bez zmian architektury wykonać dłuższy stress na końcowym obrazie: dziesiątki cykli F5/przełączenie/wpisywanie, dwie instancje Commandera i SYSINFO, ruch myszy, +/− i Tab, zamykanie/odtwarzanie sesji. Zapisywać INPUTSTAT i bilans pamięci przed/po. Sprawdzić fizyczną klawiaturę/mysz, a nie tylko automat UI.
2. Jeśli ACIA lub RAW wzrośnie: wskazać konkretny długi odcinek supervisor. Sprawdzić koszt overlay/cursor, kopiowania komórek i blitter wait. Timer-C powraca do supervisora przez kernel_user_pause_recover bez przywrócenia wcześniejszego SR; główna pętla może działać przy IPL6. Nie obniżać globalnie IPL bez analizy wyścigów INPUT fallback, raw ring i odziedziczonych hooków TOS.
3. Po przejściu stress: dokończyć B3, porównać rzeczywiste SR/PC błędów 68030 z debuggerem, osobno supervisor fault i inne CPU. Następnie mały etap B4: oddzielenie nieaktywnych zależności TOS od natywnego startu/timera.
4. Zostawić na osobne zadania MMU, FAT overwrite/rollback, VOFA, nowe profile sprzętowe i desktop. Nie łączyć z bieżącym wejściem.

Archiwum zawiera aktualny kod, testy, build/ROM, dokumentację, logi i zamknięty HDD. W pakiecie zewnętrznym są sumy SHA-256, audyt dysku i input-load.patch względem kopii work/input-load-before. Starszy punkt ATOS_2_0_STABILNOSC nie obejmował nowych ramek/zaznaczeń Commandera ani tej diagnostyki.

## Aktualizacja 24.09.2026 — poprawka utraty wejścia (SR/IPL6)

Po dłuższym stressie opisana poprawka wejścia okazała się niewystarczająca. Diagnoza wykazała, że po
wywłaszczeniu przez Timer-C ścieżka kernel_user_pause_recover wracała do supervisora przez RTS bez RTE,
zostawiając SR=0x2600 (IPL6) i maskując IRQ6 w pętli głównej; ok. 24% próbek pętli pracowało przy IPL6,
a bajty IKBD ginęły jako ACIA overrun / QUAR / PARTIAL. To nie była jedna długa funkcja — overruny
rozłożyły się na wszystkie sekcje pętli.

Poprawka punktowa: w system/kernel_entry.S, w kernel_user_pause_recover, przed RTS dodano przywrócenie
bazowego SR nadzorcy: move.w #0x2500,%sr (S=1, IPL5). Nie zmieniono globalnej polityki IPL w innych
miejscach ani parsera INPUT.SYS.

Walidacja: 20 cykli tego samego stressu na obrazie z poprawką — wszystkie liczniki INPUTSTAT = 0
(wcześniej po 16 cyklach ACIA=1, QUAR=9, PARTIAL=1). Build i testy: PASS.

Stan: poprawka jest zainstalowana w work/atos-v2.img (sha256 403b7a1e...) oraz w build/system/KERNEL.SYS
(sha256 15c986f8...). Szczegóły: docs/RAPORT-POPRAWKA-WEJSCIA-SR_2026-09-24_PL.md; patch:
work/fix-input-sr.patch; sumy: PRZEKAZANIE_DEEPSEEK/sha256-fix-input-sr.json. Uwaga: to nadal nie jest
pełna kwalifikacja (emulator, brak fizycznej myszy, 20 cykli) — przed wydaniem zrobić dłuższy stress
i test fizyczny. Audyt dysku kontrola_dysku.json dotyczy obrazu sprzed tej poprawki.

## Aktualizacja 24.09.2026 (2) — VOFA/ET4000 w module VOFA.SYS

Zgodnie z analiza rozmiaru wydzielono obsluge VOFA/ET4000 z jadra do opcjonalnego modulu VOFA.SYS.
Kernel ładuje go tylko gdy SYSTEM/VIDEO.CFG zawiera "VOFA" i wola jego init; bez modulu/karty zostaje
Shifter. Shadow 32 KB jest teraz alokowany leniwie z kmem tylko przy aktywacji karty.

Efekt: KERNEL.SYS 101 560 -> 98 826 B, BSS 261 618 -> 229 622 B (-32 KB), ST-RAM FREE po starcie
3083 -> 3114 KB (+31 KB), VOFA.SYS 2 996 B (opcjonalny). Build i testy PASS; boot OK; stress 15 cykli
(3,9 min) bez strat. Patch: work/vofa-module.patch; analiza: docs/ANALIZA-ROZMIARU-KERNEL-SHELL_2026-09-24_PL.md;
sumy: PRZEKAZANIE_DEEPSEEK/sha256-vofa-module.json. Kolejny krok z planu: TOS.SYS (persona TOS) i dalsze
dynamiczne bufory FAT/WM.

## Aktualizacja 24.09.2026 (3) — TOS=0 i dynamiczne bufory

Dodano profil build-time TOS=0 (bez persony TOS) oraz usunieto trzy stale bufory z BSS: FAT undo 66 KB
(alokowany leniwie przy pierwszym zapisie) i WM console_surface + app_surface 96 KB (alokowane przy
pierwszym uzyciu GUI). Efekt: KERNEL.SYS 98 826 -> 98 962 B (drobny wzrost kodu alokacji), BSS
229 622 -> 67 590 B (-162 KB), ST-RAM FREE po starcie 3114 -> 3272 KB (+158 KB); razem z VOFA BSS
261 618 -> 67 590 i wolny ST-RAM 3083 -> 3272 KB. TOS=0 osobno daje ~13 KB (gc-sections i tak usuwa
wiekszosc persony) — pelny modul TOS.SYS ma zly stosunek zysku do pracy z powodu 27 symboli jadra
i zakresu +/-32 KB. Build i testy PASS; boot OK; SETUP zapisal jezyk (sciezka FAT write z dynamicznym
undo) bez bledu; stress 15 cykli (3,9 min) bez strat. Patch: work/tos-buffers.patch; sumy:
PRZEKAZANIE_DEEPSEEK/sha256-tos0-buffers.json.

## Aktualizacja 24.09.2026 (4) — audyt RAM i nowy uklad pamieci

Audyt kmem wykazal, ze ~824 KB zuzycia to glownie rezerwacja pod baze kernela 0x80000 (512 KB) plus
obraz ~167 KB, a nie bufory. Przeniesiono KERNEL.SYS na 0x00020000 i BOOT.SYS na 0x00008000
(spójnie: boot_protocol.h, kernel-sys.ld, boot-sys.ld, kernel_entry.S, boot_entry.S, bootstage.c,
test_kernel_load_addr.py). Efekt: ST-RAM FREE po starcie 3272 -> 3656 KB (+384 KB), used ~440 KB;
clean rebuild + testy PASS, boot OK, stress 15 cykli bez strat. Patch: work/layout-ram.patch; audyt:
docs/AUDYT-RAM-KMEM_2026-09-24_PL.md; sumy: PRZEKAZANIE_DEEPSEEK/sha256-layout-ram.json. Kolejne
latwe zyski: block cache 128->32 sektory (~48 KB) i desktop_framebuffer leniwie (~32 KB).




