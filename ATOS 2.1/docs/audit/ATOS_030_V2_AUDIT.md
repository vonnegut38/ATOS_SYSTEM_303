# ATOS 030 2.0 — audyt fazy 0

Data: 24.09.2026. Status: zakończony audyt źródeł, kompilacji i dostępnych testów hostowych; kwalifikacja wykonania na 030/sprzęcie pozostaje otwarta. Bez zmian w oryginalnych źródłach.

## Werdykt

**Istniejący kernel nadaje się warunkowo jako baza rozwoju ATOS 030. Nie jest jeszcze potwierdzonym fundamentem chronionego systemu z PMMU.** Nie ma uzasadnienia dla wymiany całego kernela. Są już procesy natywne, user mode, przełączanie kontekstu, IPC, zasoby per proces, loader oraz sterowniki i narzędzia. Należy naprawić błędy bazowe, odseparować trzy nakładające się modele zadań i usunąć zależności ścieżki natywnej od TOS/desktopu. Menedżer pamięci wirtualnej wymaga nowej, ograniczonej implementacji; eksperyment mapowania ROM nie jest jego gotową podstawą.

Projekt nie wymaga zewnętrznego TOS do natywnego startu według źródeł: własny ROM → BOOT.SYS → KERNEL.SYS. Jednocześnie istnieje alternatywna ścieżka uruchamiania RAM-TOS/EmuTOS. Nie są to obecnie jeden zintegrowany system.

Rekomendacja: przygotować pakiet bazowy B0–B4 z planu prac, a później przekazać dalsze zadania DeepSeek lub innemu wykonawcy. Przekazanie może nastąpić wcześniej, jeżeli nowy wykonawca otrzyma tę samą bazę źródłową i odtwarzalne testy. Dobór modelu nie zastępuje weryfikacji.

## 1. Zakres i baza dowodowa

Źródła: `/Users/xxx/Documents/Deepseek_test/commander-v10`. Korzeń Git: `/Users/xxx/Documents/Deepseek_test` — obejmuje także inne projekty. Nie wykonywać zbiorczych operacji Git na całym korzeniu.

HEAD w chwili pobrania: `2861cec` — „ATOS v10: OUR desktop (bar + clock) runs on EmuTOS under ATOS BIOS (mint+gemlib)”. Baza zawiera również lokalne zmiany w `mint/desktop_mint.c`, `mint/DESKTOP.PRG` oraz nieśledzone ikony i generatory. Manifest obejmuje 475 plików z pominięciem build/, work/, backupów i cache. Testy wykonano na osobnej kopii tej bazy.

Dowody znajdują się w `../evidence/`: stan Git, patch lokalnych zmian, manifest SHA-256, logi kompilacji i testów, adresy symboli, sekcje ELF, sumy wybranych oryginalnych oraz odtworzonych binariów. Metadane obrazów HDD są zapisane, ale ich zawartość i zgodność z bieżącymi źródłami nie zostały potwierdzone. Sam tytuł commita lub screenshot nie stanowi testu wykonania obecnej bazy.

Świeżo odtworzone `bios_mste.bin`, `bios_st.bin`, `KERNEL.SYS` i `BOOT.SYS` mają identyczne SHA-256 jak odpowiadające pliki w oryginalnym build/. To potwierdza odtwarzalność tych czterech binariów, ale nie ich obecność w obrazach HDD. Końcowe porównanie 475 plików źródłowych również nie wykazało zmian względem manifestu.

Nie uruchamiano w tej fazie TOS/EmuTOS, nie wdrażano MMU i nie zapisywano obrazów/nośników. Nie wykonywano startu w Hatari ani na sprzęcie. Zachowane historyczne zrzuty ekranu nie są zaliczane jako nowa weryfikacja. Nie jest to formalny audyt każdej instrukcji ani wszystkich możliwych błędów.

## 2. Wyniki testów

| Sprawdzenie | Wynik | Znaczenie |
|---|---|---|
| Czyste `gmake -j4 all` | PASS | Firmware i natywne obrazy budują się; komunikaty linkera RWX pozostają. Nie obejmuje zewnętrznego toolchainu mint/gemlib. |
| 10 testów C w `gmake check` | PASS | Kontrakty machine/lang/config/exception/IRQ/video/cpu/modules. Nie dowodzi poprawności modeli sprzętu. |
| Python discovery w `gmake check` | 15 testów, 4 błędy | 11 testów OK. Pełne `check` jest czerwone. |
| Dodatkowe targety hostowe | Wynik mieszany | Przeszły m.in. WM, mono font, Commander, cache odczytów, trash, damage, sessions, SysInfo; szczegóły poniżej. |
| Niezależny prefiks ramki 030 | Wykryty błąd | Parser zwraca błędne SR/PC/format. |
| Statyczna kontrola MMU ROM | Wykryte niespójności | Endian CRP, tylko jedna strona ROM, stałe adresy tablic. |

Cztery błędy Python:

- `test_cursor_words`: brak deklaracji/zastępczej implementacji `kvideo_is_vofa` w generowanym harnessie.
- `test_session_overlay`: ten sam problem.
- `test_sysinfo_api`: test oczekuje 400 B, aktualny `system_report_t` ma 412 B i statyczną asercję. To dryf kontraktu testowego; nie należy przywracać starego rozmiaru tylko dla zielonego testu.
- `test_user_sleep_wakeup`: uproszczona struktura testowa nie ma `terminal`; brak `ksessions_current`.

Dodatkowe niepowodzenia:

- `check-terminal`: ASan potwierdza odczyt poza buforem w `term_scroll` (realny błąd implementacji).
- `check-fat-write`: test zabrania nadpisania pliku, podczas gdy `kfat_copy_path` używa `entry_slot(...,2,...)` z komentarzem „create or overwrite”. Potrzebna jawna decyzja kontraktu i testy nadpisywania/rollback; nie klasyfikować automatycznie jako uszkodzenia FAT. Ta sama porażka zatrzymuje końcową część `check-block-cache`, choć oba wcześniejsze testy cache przechodzą.
- `check-console`: brak symboli WM w harnessie linkowania.
- `check-video`: `test_vofa` przechodzi, `test_video_output` oczekuje odrzucenia `vram+1`, a bieżąca funkcja aktywacji przyjmuje je. Rozstrzygnąć wymagania interfejsu zamiast zmieniać asercję w ciemno. Późniejszy `test_video_boot` w tym celu nie został uruchomiony.

## 3. Błędy i ryzyka wymagające pierwszeństwa

### F01 — terminal czyta poza buforem (potwierdzone)

`system/text_terminal.c:41`, `term_scroll`: pętla działa do `i+1 < cols*rows`, ale odczytuje `cells[i+cols]`. Dla 80×25 odczyt przekracza 2000 komórek. Ponadto `term_fill` używa stałego stride 80, a `term_text`/`put` korzystają z `cols`. Zwężenie konsoli wymaga jednego kontraktu stride. Kryterium naprawy: test 80×25 i mniejszych geometrii, przewijanie i czyszczenie, brak błędów ASan/UBSan, brak zmiany sprawnego rysowania Commandera.

### F02 — dekoder wyjątków nie odpowiada 68030 (potwierdzone)

`include/exception_format.h:23` odczytuje słowo formatu z początku ramki. Prefiks MC68030 zawiera SR, PC, następnie format/vector offset. `tests/test_exception.c` potwierdza własny błędny układ i opisuje format 7 jako bus error 030. Ramki bus error 030 wymagają obsługi A/B. Własny probe z rzeczywistym prefiksem daje format=2, SR=00e0, PC=2000a008 zamiast A,2700,00e02000. Źródło: [MC68030 User’s Manual, §8.4](https://www.nxp.com/docs/en/reference-manual/MC68030UM-P2.pdf).

Powiązane: `system/kernel_entry.S:109` zapisuje SR przez `move.l (%sp),kernel_fault_sr`, a raport maskuje dolne 16 bitów, czyli część PC, nie zapisane SR. Naprawa diagnostyki musi obejmować firmware i kernel, z osobnymi przypadkami 68000/030. Nie przenosić tego parsera do obsługi błędów PMMU bez korekty.

### F03 — MMU ROM nie jest menedżerem VM (potwierdzone statycznie)

`boot/mmu_boot.c` przeznaczono do mapowania ROM EmuTOS:

- adres CRP 0x003f0000 zapisywany bajtami little-endian daje na 68k 0x00003f00;
- zmieniany jest jeden wpis strony 32 KiB, przy dopuszczaniu obrazu do 512 KiB;
- tablice są pod 0x003f0000/0x003f1000 w ST-RAM, mimo komentarza o ALT-RAM;
- brak rezerwacji tych zakresów przez przyszły allocator VM, przestrzeni per proces i kompletnej ścieżki page fault.

Zachować jako eksperyment legacy. Nie naprawiać go po drodze jako sposobu na wdrożenie natywnego MMU. Nowy VM powinien mieć własny, mały test mapowań i pełną mapę zasobów.

### F04 — natywny timer zależy od TOS (potwierdzone)

`system/kernel_entry.S:233`, `kernel_timer_c` wywołuje `tos_composite_tick` i `tos_task_tick` przed natywnym przełączaniem użytkownika. W projekcie są osobno: dwa zadania modułu PROC.SYS, natywna tabela sześciu procesów i scheduler persony TOS. To trzy różne mechanizmy, których nie wolno traktować jako jednego gotowego schedulera 030.

### F05 — allocator: flagi nie zapewniają ochrony (potwierdzone); odporność wymaga napraw/kwalifikacji

`kernel_memory.c` to allocator blokowy ST/TT, nie allocator stron. READ/WRITE/EXEC/SUPERVISOR są metadanymi, nie mapowaniami PMMU. W `pool_alloc` zaokrąglenie i dodawanie rozmiarów nie mają pełnej kontroli przepełnienia; funkcje free/pin/owner czytają nagłówek przed pełnym potwierdzeniem przynależności wskaźnika do puli. `pool_heal` i odtwarzanie guard maskują niektóre uszkodzenia metadanych. To ryzyka ustalone z kodu, nie reprodukcja exploitów.

`kmem_init` rezerwuje zakres przez limit TT 0x04000000 specjalnie dla TOS. Polityka persony nie powinna być ukryta w natywnym allocatorze.

### F06 — nowy backend grafiki nie zastąpił starej ścieżki (potwierdzone)

`kernel_main` wymusza `kgfx_init(2u,...)`; `kernel_graphics.c` korzysta ze SCREEN_BASE i przypadków VOFA. `wm_get_fb` publikuje adres starego `kgfx_framebuffer` w nowej strukturze, po czym czyta go z powrotem. Nowe `video/shifter.c` i `video/et4000.c` są podłączane w testach, nie w sprawdzonej ścieżce produkcyjnej. `set_mode` może zmienić wymiary bez poprawienia pitch/rozmiaru bufora, a paleta/kursor/vsync są puste. Enumeracja PLANAR_2/4 nie oznacza pełnej obsługi tych formatów przez nowe blity.

### F07 — centralne IRQ wymaga kontraktu atomowości (ryzyko statyczne)

`system/irq.c:10` publikuje `fn` przed `context`, bez sekcji krytycznej. Rejestracja/usuwanie podczas aktywnego dispatch może udostępnić niespójny slot. Można zacząć od ograniczenia rejestracji do fazy inicjalizacji albo od krótkiej, udokumentowanej sekcji krytycznej. Test sekwencyjny nie sprawdza tej sytuacji. Podłączone są Timer C i ACIA; brak dowodu kompletnego podłączenia VBL/HBL i pozostałych źródeł.

### F08 — storage nadal przecieka ACSI do kernela (potwierdzone)

Warstwa `hw/block_device.c` rozróżnia ACSI/IDE, ale `kernel_block.c` zawiera bezpośrednie operacje na rejestrach DMA ACSI dla zapisu/identyfikacji. Pełny system nie ma jeszcze jednolitego transportu. `block_flush` raportuje sukces na podstawie obecności urządzenia, nie wykonuje rzeczywistego flush. Nie obiecywać trwałości danych na podstawie tego API.

## 4. Start i mapa pamięci

Start natywny według źródeł:

1. `boot/reset.S:_reset`: SR=2700, tymczasowy stos 0x8000, wczesna detekcja RAM, sondy, stos RAM-top−0x1000, zerowanie BSS.
2. `main.c:firmware_main`: CPU/FPU, ST/ALT-RAM, maszyna i rozszerzenia, konfiguracja; opcjonalny `auto_tos` jest odrębnym odgałęzieniem.
3. POST/setup/boot manager → `loader/bootstage.c`: wyszukanie FAT/BOOT.SYS, walidacja nagłówka i checksum.
4. BOOT.SYS uruchamia callback `start_kernel`; KERNEL.SYS jest sprawdzany i ładowany pod 0x80000.
5. `kernel_entry` zeruje kernel BSS i przechodzi do `kernel_main`.
6. Kernel ustawia wektory/VBR=0, allocator, grafikę mono, konsolę, ACSI/cache/FAT/partycje oraz moduły CORE,VFS,PROC,TIMER,EVENT,INPUT,SHELL. Uruchomienie normalnej pętli jest silnie związane z gotowością shella i grafiki.

Handoff ABI v3: 52 B, dane maszyny/RAM/boot oraz wskaźniki usług firmware. Stan cache/MMU, własność przerwań i zakresy pamięci nie mają kompletnego jawnego kontraktu. ROM ma historycznie zgodny nagłówek startowy; to nie oznacza wymagania oryginalnego TOS.

| Zakres/punkt | Rola i ograniczenie |
|---|---|
| 0x00000000 | Wektory, VBR ustawiany na 0; są też zapisy struktur kompatybilności w low memory. |
| 0x00000400 | Początek RAM BSS firmware; w bieżącym ELF BSS ma 6478 B. Sprawdzić czas życia względem zmiennych persony TOS. |
| 0x00008000 | Tymczasowy stos resetu. |
| 0x00010000–0x00017cff | SCREEN_BASE, 32000 B obrazu mono. |
| 0x00020000 | BOOT.SYS (790 B); wcześniejsza detekcja ALT-RAM używa tego adresu jako scratch. |
| 0x00080000 | Początek KERNEL.SYS, entry 0x80020. |
| 0x0009e528–0x000e03e9 | BSS kernela w świeżym buildzie; koniec obrazu w pamięci 0xe03ea. |
| od kernel_end do ST-top−0x4000 | Pula alokatora ST. |
| 0x003f0000 i 0x003f1000 | Stałe tablice wyłącznie eksperymentu MMU ROM; wymagają dostępnej pamięci i rezerwacji. |
| 0x00c00000 / 0x00d00000 | VRAM / I/O adaptera VOFA, nie uniwersalna mapa każdej ET4000. |
| 0x00e00000 | Wariant ROM Mega STE 256 KiB. |
| 0x00fc0000 | Wariant ROM ST 192 KiB. |
| 0x01000000 | Założony początek ciągłej pamięci ALT/TT. |
| 0x04000000–0x04ffffff | Stała TPA persony TOS; nie wolno zakładać obecności na każdej konfiguracji. |

Bieżący build KERNEL.SYS: 124200 B pliku; rozmiar pamięci 394218 B. Konkretne wartości dotyczą tylko manifestu tego audytu. Mapa jest odtworzona statycznie, nie zmierzona w działającym 030.

## 5. Procesy, wywołania systemowe i pliki wykonywalne

Natywne aplikacje mają format ASYSAPP0 z nagłówkiem 32 B, checksum, entry, rozmiarem pamięci/stosu oraz końcówką APW2 opisującą obszar wykonywalny/zapisywalny. `verify_app_image` ogranicza obraz do 256 KiB, stos 1–16 KiB. Loader `kernel_spawn_user_async` alokuje obraz i stos, zeruje rozszerzenie pamięci i przekazuje zasoby do `kernel_user_create_loaded`. Format i użyte konwencje relokacji/PC-relative trzeba zachować do czasu udokumentowania ABI; sam kod loadera nie robi standardowych relokacji TOS dla aplikacji natywnych.

`system/kernel_exception.c` skupia procesy, syscall dispatcher, pliki, GUI, sesje i IPC. Tabela ma 6 slotów, zachowuje D0–D7/A0–A6, PC/SR/USP, PID/parent PID, stan, kwant, watchdog, mailbox, stos/obraz i właściciela pamięci. Są wait/sleep/yield/spawn/exit i cleanup zasobów. `kernel_entry.S` przechodzi przez RTE do user mode. Nie ma dowodu izolacji PMMU, kompletnego FPU context ani kwalifikacji wszystkich ramek wyjątków na 030. SR użytkownika ustawiany jest na 0x0500; to ogranicza dostępne przerwania i wymaga przeglądu przed poleganiem na VBL.

Natywny trap #0 ma kontrolę zakresów argumentów, lecz bez MMU program wykonujący bezpośrednie zapisy nie jest w ten sposób izolowany. Nie nazywać kontroli syscalli ochroną całej pamięci procesu.

Moduły ASYSMOD0/ABI3 ładowane są osobno i rejestrują usługi przez tablicę callbacków. Są komponentami uprzywilejowanymi, nie izolowanymi aplikacjami. Zachować obecny model jako bazę, najpierw opisać różnicę „moduł/usługa/proces”.

## 6. Sterowniki, storage i konsola

Istnieją: firmware ACSI/IDE, wspólny identyfikator transportu, kernelowy cache 64 KiB z buforem DMA w ST-RAM, partycje AHDI, FAT i operacje kopiowania/katalogów/kosza. FAT nie ma wykazanej zależności od GEMDOS: korzysta z kernel block. Warto zachować ten dorobek.

Do poprawy: transport zapisu/identyfikacji, kontrakt flush, globalne bufory i transakcje przy przyszłej współbieżności. Firmware ACSI write zwraca porażkę, podczas gdy kernel ma własną ścieżkę zapisu; tych możliwości nie wolno łączyć w jednym stwierdzeniu „ACSI read-only”. Trwałość BIOS.CFG musi być oceniona dla konkretnego transportu, nie wyłącznie przez test CRC.

`text_terminal.c` daje bufor komórek; `kernel_console.c` utrzymuje sesje i korzysta z WM/grafiki, natomiast firmware ma wcześniejszy renderer i callbacki. Zachować wszystkie trzy role: early console, panic diagnostics i shell. Izolację zacząć od interfejsu wyjścia, nie od przeniesienia wszystkich funkcji do procesu.

## 7. Grafika i ET4000/W32i

Najbardziej konkretna implementacja sprzętowa to `system/vofa_et4000.c:vofa_et4000_init` oraz `system/video_bus.S`. Ustawia VGA/ET4000 przez sprawdzane callbacki, bada zegar i VRAM, przygotowuje 640×480 mono, a kernel prezentuje 640×400. `kernel_video.c` ma statyczny shadow 32000 B i kopiuje zmienione słowa bajtowo do karty. To wartościowy backend odniesienia, ale nie gotowe 1024×768×8.

`hw/nova.c` i detekcja rozszerzeń nie zastępują produkcyjnego sterownika wszystkich adapterów. Nie znaleziono w przejrzanej głównej ścieżce gotowej akceleracji W32i, sprzętowego kursora ani kompletnego sterownika chunky 8 bpp. `video/et4000.c` jest szkieletem powierzchni z callbackiem, nie dowodem konfiguracji sprzętu.

Możliwe testy niezależne: `vofa_et4000_init` przez fake I/O (już istnieje), damage/shadow-copy, clipping/blit, generowanie palety, walidacja trybu i pitch. Dopiero osobny test emulatora z właściwym adapterem/sprzętu potwierdza zachowanie karty.

Własność ekranu jest rozproszona pomiędzy kgfx, WM, sesje, kconsole, TOS i kvideo. Nowy backend powinien mieć jawny kontrakt bufora i prezentacji; nie ma potrzeby od razu przepisywać sprawnych algorytmów WM.

## 8. TOS/EmuTOS i mapa zależności

Faktyczne niepożądane zależności:

```text
kernel_main → kgfx / kconsole / sessions / WM / shell / personę TOS
kernel_timer_c → tos_composite_tick → grafika
kernel_timer_c → tos_task_tick → drugi model procesów
kernel_exception → procesy + syscalle + sesje + GUI + pliki + IPC
kernel_memory → stałą rezerwację dla TOS TPA
kernel_graphics → Shifter/VOFA + pomocnicze renderery EmuTOS
kernel_block → bezpośrednie rejestry ACSI
```

Właściwe zależności już obecne: FAT → kernel block; terminal tekstowy ma własny bufor komórek; część WM jest testowalna na hoście; moduły mają rejestr usług; VOFA ma wstrzykiwane operacje I/O.

`system/tos_prg.c`, `tos_vdi.c`, trapy 1/2/13/14, Line-A, cookie/low memory i TPA to persona kompatybilności. `loader/tos.c`, `boot/ramjump.S`, `boot/mmu_boot.c` to eksperymenty alternatywnego bootowania. `mint/desktop_mint.c` i `system/desktop_gem.c` są aplikacjami/prototypami GEM, nie fundamentem natywnego desktopu 030.

W źródłach już są adaptacje EmuTOS (`vofa_et4000.c`, renderery/zasoby), z notami pochodzenia. Samodzielność od ROM EmuTOS nie wymaga usuwania sprawnych algorytmów tylko ze względu na pochodzenie. Faza 0 nie dokłada obcego kodu; zachować istniejące noty i osobno zinwentaryzować pochodzenie.

Docelowo natywne aplikacje/desktop korzystają z systemowego API oraz GUI; adaptery TOS wywołują te same mechanizmy. Kernel udostępnia ogólne trapy/wyjątki, bez wymagania aktywnej persony. Nie wymuszać procesu użytkownika dla każdego sterownika w pierwszej migracji.

## 9. Tabela migracji

Poziomy dowodu: S=analiza statyczna, B=kompilacja, H=test hostowy. Brak nowych dowodów runtime 030 w tym audycie. Lokalizacje docelowe są propozycją, nie poleceniem przenoszenia.

| Komponent | Teraz / cel | Zależności | Status | Zalecenie / dowód | Ryzyko | Docelowo |
|---|---|---|---|---|---|---|
| Reset/ROM | boot/reset.S, start | RAM/probes | KEEP | zachować sekwencję; S/B | wysokie przy zmianie | boot/ |
| Boot/handoff | loader/bootstage.c | FAT, firmware callbacks | MODIFY | jawny kontrakt i mapy; S/B | wysokie | boot/ |
| CPU/RAM/maszyna | hw/machine.c, memory.c, boot/memdetect.S | magistrala | KEEP | dodatkowa kwalifikacja 030; B/H częściowo | wysokie | drivers/platform/ |
| Konfiguracja | hw/config.c | FAT/block | MODIFY | walidacja jest; test trwałości per transport | średnie | boot/config/ |
| Diagnostyka wyjątków | exception_format.h, report, entry | CPU frames/console | MODIFY | F02, niezależny test nie przechodzi | krytyczne | kernel/arch/m68k/ |
| MMU ROM | boot/mmu* | TOS, stałe adresy | LEGACY | F03; nie używać jako VM | krytyczne | legacy/rom-mmu/ |
| Menedżer VM | brak | PMMU, allocator, faults | MISSING | osobny etap po bazie | krytyczne | kernel/mm/ |
| Allocator | kernel_memory.c | ST/TT, TPA | REFACTOR | zachować API, poprawić zakresy/kontrakt | wysokie | kernel/mm/ |
| Procesy natywne | kernel_exception.c | pamięć, timer, GUI/session | REFACTOR | zachować semantykę; S/B | wysokie | kernel/process/ |
| Kontekst 68k | kernel_entry.S | ramki, USP, IRQ | MODIFY | kwalifikacja 030, bez przepisywania | krytyczne | kernel/arch/m68k/ |
| PROC.SYS | proc_module.S | module API | KEEP | opisać dwa zadania modułu vs procesy | średnie | kernel/services/ |
| TOS scheduler | tos_prg.c | kernel IRQ | MOVE | opcjonalna persona, F04 | wysokie | compat/tos/ |
| IPC | kernel_exception.c, event_module.S | procesy, mailbox | REFACTOR | rozdzielić dwa API i cleanup | średnie | kernel/ipc/ |
| Syscall | kernel_exception.c | memory/fs/gui | REFACTOR | wydzielać po testach ABI | wysokie | kernel/syscall/ |
| Moduły/loader | kernel_module.c | kmem/FAT/callbacks | KEEP | udokumentować ASYSMOD0/ASYSAPP0 | średnie | kernel/loader/ |
| IRQ | irq.c, kernel_entry.S | MFP/ACIA | MODIFY | atomowość i aktywne źródła | wysokie | kernel/irq/ |
| Block cache | kernel_block.c | DMA + ACSI | REFACTOR | zachować cache, odseparować transport | wysokie | fs/block/, drivers/storage/ |
| ACSI/IDE | hw/acsi.c, ide.c | magistrala | MODIFY | zachować, spójne read/write/flush | wysokie | drivers/storage/ |
| FAT/partycje | kernel_fat.c/partition.c | block | KEEP | kontrakt overwrite, serializacja transakcji | wysokie | fs/ |
| IKBD | hw/ikbd.c, kernel_module.c | IRQ/ring | REFACTOR | oddzielić wejście od sesji/loadera | średnie | drivers/input/ |
| Terminal | text_terminal.c | bufor komórek | MODIFY | F01 przed migracją | wysokie | gui/terminal/ |
| Konsola | kernel_console.c, ui/console.c | WM, sessions, hardware | REFACTOR | early/panic oddzielnie od shella | średnie | kernel/diag/, apps/shell/ |
| WM/blit | wm_core.c/wm_blit.c | callbacki/powierzchnie | KEEP | host tests PASS | średnie | gui/wm/ |
| KGFX | kernel_graphics.c | Shifter/VOFA/TOS-style render | REFACTOR | zachować sprawne algorytmy, wydzielić backend | wysokie | gui/render/ |
| video_driver | include/video_driver.h, video/* | attach/shadow | MODIFY | kontrakt i produkcyjne podłączenie, F06 | wysokie | drivers/video/ |
| VOFA | vofa_et4000.c, video_bus.S | adapter 0xc/0xd | KEEP | mono referencja; host fake-I/O PASS | wysokie | drivers/video/vofa/ |
| ET4000 8bpp/W32i | szkielety/wykrywanie | adapter, VRAM | MISSING | brak kompletnej wykazanej ścieżki | wysokie | drivers/video/ |
| TOS API | tos_prg.c/tos_vdi.c | trap/global state | MOVE | izolować; teraz nie rozwijać | wysokie | compat/tos/ |
| GEM desktop próby | mint/, desktop_gem.c | EmuTOS/gemlib | LEGACY | zachować jako demonstracje | niskie dla celu 030 | legacy/gem-desktop/ |
| Commander/SysInfo | system/commander.c/sysinfo.c | natywne API | KEEP | wartościowe testy/aplikacje | średnie | apps/ |
| ST Box | brak | VM, emulator devices | MISSING | poza planem bazowym | bardzo wysokie | przyszła decyzja |

Nie ma obecnie komponentu wskazanego do natychmiastowego usunięcia. Po pełnej migracji można rozważyć wyłączenie starych glue/debug harness z domyślnego buildu. Nie kasować ich w fazie 0.

## 10. Co zachować i czego brakuje

Zachować: własny firmware i loader, detekcję, handoff z wersją, moduły, natywne procesy/IPC, FAT/cache/kosz, Commander/SysInfo, terminal po naprawie, testowalne WM i VOFA mono. Nie zmieniać teraz formatu aplikacji, nie przenosić całego drzewa i nie usuwać źródeł o pochodzeniu EmuTOS.

Brakuje: zakwalifikowanego profilu sprzętu 030; kontrolowanego handoff stanu CPU; poprawnej diagnostyki ramek; allocator/VM z polityką ochrony; dowodu izolacji dwóch procesów; niezależnej od TOS ścieżki IRQ/startu; produkcyjnego backendu 8 bpp; jednoznacznej własności ekranu; świeżych testów runtime i wiarygodnego powiązania źródła–binarium–obraz.

Największe ryzyka: ramki/wyjątki/PMMU, współwłasność kontekstów procesów, współbieżność globalnych buforów i zasobów, mapowanie adaptera ET4000 oraz mylenie historycznych screenshotów z aktualnym testem.

## 11. Decyzja i przekazanie

Nie zalecam wymiany kernela. Zalecam wykorzystanie go jako bazy z wąskim pakietem stabilizacyjnym, a następnie osobnym etapem VM i grafiki. Przydatność na docelowym sprzęcie jest warunkowa do chwili testów runtime. Zakres, kolejność, akceptacja i zasady przekazania znajdują się w `ZAKRES_PRAC.md` i `DEEPSEEK_HANDOFF.md`.

Oryginalny projekt pozostaje bez zmian. Na tym kończy się faza 0; pakiety implementacyjne nie są oznaczone jako wykonane.
