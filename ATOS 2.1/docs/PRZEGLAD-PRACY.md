# ATOS 2.0 — rzetelny przegląd pracy (analiza read-only)

Data przeglądu: 2026-09-26. Zakres: kopia robocza `ATOS 2.0` w
`/Users/xxx/Documents/Deepseek_test`. Oryginał `Codex/ATOS 2.0` i
`commander-v10` nie były dotykane. Nie budowano, nie uruchamiano Hatari;
weryfikacja wyłącznie przez czytanie źródeł, `grep`, porównania binarne i dwa
testy hostowe uruchomione w tempdir (bez zapisu artefaktów w projekcie).

---

## 1. Streszczenie stanu (punkty 1-8)

| # | Punkt roadmapy | Stan | Główne dowody (plik:linia) | Uwaga |
|---|---|---|---|---|
| 1 | Model drive/PATH — AUTOEXEC bez hardkodu C: | **ZROBIONE (model); AUTOEXEC martwy** | `system/kernel_fat.c:174` i `:363` (obcinanie `X:`), `system/shell_path.c:11-15`, `system/kernel_exception.c:1656` (`segment[1]==':'\`), `system/AUTOEXEC.BAT` | Model `X:` działa i jest device-independent. Ale `AUTOEXEC.BAT` nie jest nigdzie instalowany ani czytany (grep: tylko docs/JSON) i wskazuje `\SYSTEM\COMMAND\PATH.APP`, gdy installer pisze do `\SYSTEM\APP` (`tools/build_atos2.py:39-40`). |
| 2 | Test „hostile PATH” (błąd, nigdy panic) | **CZĘŚCIOWO** | Poprawka: `system/shell_module.S:2046-2047` (tylko `-1` idzie dalej po PATH); komunikaty `SHELL.SYS:2481-2490`; scenariusze `atos2-tools/scenario_faultsep.py`, `scenario_abifix*.py`, `scenario_pathfix.py` | Brak zautomatyzowanego testu z asercjami — to skrypty robiące wyłącznie zrzuty ekranu (`window=True`, patchowany `atari-030-port/hatari-tf536`). „Nigdy panic” potwierdzone tylko ręcznie. |
| 3 | Konkretny panic: wektor+PC+SR+instrukcja | **ZROBIONE** | `shell_do_path_display` `shell_module.S:907-934`, `shell_parse_u32` `:2242-2269`, `shell_do_wait` `:1524-1594`, `shell_do_watchdog` `:1317-1332` — wszystkie zapisują/odtwarzają `%d2`; bramka `tools/asm_abi_audit.py:10,53-61`; `tools/build_atos2.py:25-31` | Przyczyna i 4 miejsca naprawy zgodne z `V10-STATUS.md:723-748`. Uruchomiona bramka: **ABI CHECK PASS** dla 17 plików. |
| 4 | Shell/FS: DIR/CD/PWD/TYPE/COPY/MOVE/DEL/MKDIR, PATH, apek, argumenty, fg/bg, I/O | **CZĘŚCIOWO** | Są: `dir/ls/cd/type/cat/pwd/clear/cls/run/start/startfg/path` (`shell_module.S:2429-2448`, dispatch `:748-862`). Brak builtinów **copy/move/del/mkdir** — nie ma ich w tabeli ciągów (grep 0). API jądra i Commander je mają: `kernel_fat.c:431,551,566`, syscalle 49/50/51 `kernel_exception.c:1505-1507`, `system/commander.c:349,533,535` | Apek/argumenty/fg/bg/IO-błędy działają; zarządzanie plikami tylko z Commandera, nie ze shella. |
| 5 | Stabilność procesów (fault, /0, bus/addr, zły pointer, guard) | **ZROBIONE (rdzeń) + otwarty wątek `-2`** | `system/kernel_entry.S:134-234` (wektory 2/3/4/5/6/7/8/10/11 + `kernel_try_user_fault`), `kernel_exception.c:1241-1243` (`kernel_user_fault`), guardy stosu/heapu `:385-402,430-459`, testy-appki (`divfault_app.S`, `busfault_app.S`, `stackfault_app.S`, `heapguard_app.S`, …) | Teardown poprawny: `kernel_user_end` `kernel_exception.c:866-875` używa `kmem_release` (zdejmuje `KMEM_PINNED`, `kernel_memory.c:94-98`). Wątek `-2` (brak pamięci przy kolejnym spawn) jest w docs sprzecznie opisany — patrz §2. |
| 6 | BIOS/storage: HDD/IDE/ACSI/FDD, brak urządzenia, timeout, zły FAT, brak partycji, read error | **CZĘŚCIOWO** | Boot: `loader/bootstage.c:188-239` (RETRY, REMEMBERED…, BOOT DISK READ ERROR, BOOT PARTITION NOT FOUND); FDD `hw/fdc.c`, FAT12 `hw/fat12.c:67+`; IDE `hw/ide.c`; ACSI `hw/acsi.c`; szerokość FAT `kernel_fat.c:305` | **ACSI write = no-op zwracające 0** (`hw/acsi.c:120-125`), więc cfg/FAT/failover nie zapiszą się na maszynie z ACSI. „IDE-only” w Hatari nierozstrzygający (`V10-STATUS.md:72-78`). Brak obsługi gęstości/godziny FDC 0xffff860e dla MegaSTE/TT (`FLOPPY-BOOT-PLAN.md:39` vs `hw/fdc.c`). |
| 7 | Zamrożenie ABI 1.0 dla natywnych aplikacji | **NIE ROZPOCZĘTE / brak dowodów** | Istnieje tylko `MODULE_ABI_VERSION 3u` (`include/boot_protocol.h:12`), `BOOT_HANDOFF_VERSION 17u` (`:10`), pole `abi_version` (`include/kernel_module.h:24`) | Brak jakiegokolwiek „ABI 1.0”, brak deklaracji zamrożenia, brak dokumentu. grep „ABI 1.0/freeze” w docs/include = 0. |
| 8 | Grafika/desktop + osobna persona TOS | **CZĘŚCIOWO** | Backendy: `system/video_driver.c:120-125`, `video/shifter.c`, `video/et4000.c`; kompozytor `system/kernel_wm.c:74-95`; framework desktopu `system/desktop_module.c` + `tests/test_desktop_module.c` (`Makefile:908-911`); persona: `system/tos_entry.S` (trapy+fault stuby `:206-235`), `system/tos_prg.c` (GEMDOS, VDI, częściowy AES `:1396-1600`), syscall 65 `kernel_exception.c:1525-1529` | EmuTOS RAM-TOS pod ATOS BIOS działa (`V10-STATUS.md:304-314`). Ale `docs/PLAN-GEM-KOMPATYBILNOSC.md` twierdzi, że AES nie istnieje i nie ma ochrony faultów — nieaktualne (patrz §2). |

---

## 2. Sprzeczności i nieaktualne wnioski

### 2.1 Wewnątrz `docs/V10-STATUS.md`

1. **Sekcja „Do zrobienia” (linie 707-721) jest w całości nieaktualna.** Wymienia
   jako niezrobione: Phase 3 trwała konfiguracja, Phase 4 `block_device_t`,
   Phase 5 wyjątki, Phase 6 IRQ, Phase 7-13 `video_driver_t` — a te same
   punkty są oznaczone „(zrobione)” wyżej (`:80-95`, `:60-70`, `:97-121`,
   `:110-121`, `:123-156`, `:189-206`).
2. **„Zapis z jądra na IDE: BRAK ŚCIEŻKI” (`:710-714`)** sprzeczne z
   „**Zapis z jądra na IDE — NAPRAWIONE** (`:478-484`)” (handoff v9 niesie
   `acsi_write_sector`; potwierdzone w `include/boot_protocol.h:68`).
3. **„`path`: wadliwy builtin usunięty — teraz uruchamia `PATH.APP`” (`:630-640`)**
   jest nieprawdziwe. Builtin nadal istnieje i jest osiągalny:
   `shell_module.S:763-767` (`shell_same` → `shell_do_path_display`) oraz
   implementacja `:907-934`. Ta sama sekcja `:723-748` opisuje naprawę tego
   właśnie builtinu (zapis `%d2`), więc wewnętrznie też jest sprzeczna.
4. **„Znane problemy (wcześniejsze): `test_sysinfo_api.py` i
   `test_user_sleep_wakeup.py` padają” (`:56-57`)** — nieaktualne. Uruchomione
   w tym przeglądzie: oba **OK** (patrz §6). Jednocześnie `tools/build_atos2.py:32`
   woła `gmake check` z `check=True`, więc gdyby faktycznie padały, cały
   `--install` by się przerywał.
5. **`AUTOEXEC.BAT` jako żywy mechanizm (`:638`, `:671-674`)** — plik
   `system/AUTOEXEC.BAT` nie jest instalowany (brak w `build_atos2.py`,
   `Makefile`, kodzie) i wskazuje `\SYSTEM\COMMAND\PATH.APP`, podczas gdy
   installer umieszcza `PATH.APP` w `\SYSTEM\APP` (`build_atos2.py:39-40`);
   `\SYSTEM\COMMAND\PATH.APP` istnieje na obecnym obrazie, ale nie powstaje
   z tego builda.

### 2.2 `docs/SHELL-REFACTOR-PLAN.md` — wewnętrznie sprzeczny

- `:104-123`: `-2` = brak pamięci obrazu/stosu poprzedniego procesu, „do
  zrobienia: zwalniać zanim shell wystartuje następny”.
- `:146-183`: teardown istnieje i jest poprawny; `-2` to „najpewniej
  kolejność” — potem `:170-175` koryguje: `kernel_user_end` jest synchroniczne,
  więc hipoteza „odroczony reap” **obalona**.
- `:185-206`: „wycieku NIE ma; psuje się **DYSPOZYTOR**”.
- `:208-221`: „to **NIE dyspozytor, to HARNESS** (wpisane `emory` zamiast
  `memory`)”.
- Dokument jest też **uszkodzony redakcyjnie**: zdanie z `:223` („Do zrobienia
  w harnessie… po wpisaniu”) kończy się dopiero w `:240-243`, a w środku
  wstawiono nagłówek „### Harness — stan próby naprawy” (`:224`).
- **Wniosek:** ostateczna teza „dispatcher jest sprawny” wyjaśnia tylko
  obserwację `emory`; **nie wyjaśnia zaobserwowanego `-2`** przy `hw` po
  `path \NOPE` (`:116-123`). Wątek `-2`/kolejności zwalniania nie został
  domknięty ani pokryty testem regresji. To nadal realne ryzyko punktu 5.

### 2.3 `docs/PLAN-GEM-KOMPATYBILNOSC.md` vs kod — w większości nieaktualny

- `:76-77`: „trap #2 VDI działa. **Trap #2 AES nie istnieje**” — a kod ma
  `tos_aes_dispatch` z `appl_init` (10), `wind_create` (100), `objc_draw` (42),
  `form_*` `system/tos_prg.c:1303-1600`, wołane z `asf_tos_aesvdi` `:1620-1647`.
- `:88-90`: „AES (0xC8+)” — kod rozpoznaje AES po niskich opkodach i rozdziela
  VDI/AES inaczej (`:1637-1643`).
- `:92-95`: „Ochrona przed faultem programu: **brak**… double bus error
  zatrzymuje CPU” — a są stuby `asf_tos_fault2/3/4/5/6/7/8/11`
  (`system/tos_entry.S:206-235`, `system/tos_prg.c:167-184`).
- `:189-207` (diagnoza SysInfo: `TOS_HEAP_SIZE 65536`, `Mfree` nieobsłużone,
  `Mshrink→0` jako błąd) — nieaktualne: `TOS_HEAP_SIZE 4194304`
  (`tos_prg.c:591`), `tos_mfree` z koalescencją (`:981-1000`), `Mfree` w
  dispatchu (`:1110`). `Mshrink` nadal `return 0` (`:1111`), ale `0` w GEMDOS
  to sukces, nie błąd — teza w dokumencie jest błędna metodycznie.

### 2.4 `docs/BIOS-BRINGUP-CHECKLIST.md`

- `:60-65` i `:70-73`: „brak `diag_ram_pattern`”, „Dodać `diag_ram_pattern`”
  — funkcja **istnieje** od `diag/tests.c:67` i jest zadeklarowana w
  `include/diagnostics.h:16`. (Rzeczywisty problem jest inny: **nikt jej nie
  woła** — patrz §3.)
- `:30-34` „Do dodania: POST na RS-232” vs `:45-47` „RS-232 odpada” —
  sprzeczność w jednym pliku (kolejność priorytetów zmieniona, stara lista
  została).

### 2.5 `docs/DESKTOP-ARCHITECTURE.md` vs kod

- `:3-5`, `:65-68`: „Nie implementujemy go teraz… Żadnego kodu pulpitu” —
  a w projekcie jest `system/desktop_module.c`, `system/desktop_app.S`,
  `tests/test_desktop_module.c` i działający cel `Makefile:908-911`
  (`DESKTOP_MODULE: ALL PASS`). Dokument jest kontraktem, ale zdanie „żadnego
  kodu” jest nieprawdziwe.

### 2.6 `docs/FLOPPY-BOOT-PLAN.md` vs `docs/V10-STATUS.md`

- `FLOPPY-BOOT-PLAN.md:74-76`: „Etap B … Shell: do zrobienia (API modułu)”,
  a `:78-79` mówi „Etap API ZROBIONE”. Kolejne akapity tego samego pliku
  opisują to samo dwa razy (historia poprawek bez kasowania starych zdań).

---

## 3. Realne otwarte błędy (szczególnie niewidoczne w emulatorze)

1. **ACSI jest read-only — zapisy z jądra i konfiguracji milcząco przepadają.**
   `hw/acsi.c:120-125`: `acsi_write_sector` dla ACSI robi `return 0` bez
   zapisu (jedynie IDE `id & 0x80` idzie do `ide_write_sectors`). Skutek na
   fizycznym ST/STE z dyskiem ACSI: `cfg_save()`, zapadka failoveru wideo,
   `kfat_save_path` i tworzenie katalogów z Commandera nie utrwalą się.
   Emulator z IDE tego nie pokaże. Dowód po stronie ROM: ten sam plik `:117-119`.
2. **FDD: brak konfiguracji gęstości/zegara 0xffff860e dla MegaSTE/TT.**
   `docs/FLOPPY-BOOT-PLAN.md:39` wymienia ten rejestr (bit0 freq, bit1 gęstość),
   ale `hw/fdc.c` nigdzie go nie dotyka (grep 0). Na ST/STE obraz testowy
   przejdzie, na MegaSTE/TT z innym zegarem FDC boot z dyskietki może nie
   wystartować. Do tego `fdc_wait` to sztywne 2 000 000 iteracji (`fdc.c:44-49`),
   a `fdc_delay` to 40 `nop` (`:25`) — czasy dobrane w Hatari.
3. **IDE/CF: detekcja i timeouty dobrane pod emulator.**
   `hw/ide.c:36` (`ide_delay` = 8 odczytów ALTSTAT), `ide_identify` startujące
   zaraz po resecie (`:56-69`) i pętle 1 000 000 iteracji w `:66,132,167,177`.
   Fizyczny HDD/CF może nie zdążyć ze spin-upem po zimnym starcie, a wtedy BIOS
   nie znajdzie dysku (i przejdzie do skanu FDD/promptu). Emulator startuje
   natychmiast.
4. **`diag_ram_pattern` jest martwym kodem i testy nie są spięte z POST.**
   Funkcja `diag/tests.c:67-81` (bezpieczna dla 68000), ale grep pokazuje
   **zero wywołań**; `ui/diagnostics.c:19-26` uruchamia tylko 8 starszych
   testów, a `ui/post.c:42 post_run()` nie woła żadnego `diag_*`. Czyli POST
   nadal nie wykryje uszkodzonej komórki RAM przed zapisem EPROM-u.
5. **Brak sumy kontrolnej ROM-u i walidacji wersji przy starcie.**
   `_rom_header` ma datę/wersję (`boot/reset.S:9-28`), ale nie ma sumy;
   checklist `docs/BIOS-BRINGUP-CHECKLIST.md:66` sam to potwierdza. Uszkodzony
   wsad EPROM-u nie zostanie wykryty.
6. **`bios_reset()` nie ustawia znacznika ciepłego startu 0x0420.**
   `boot/reset.S:41-46` czyta i zeruje magic `0x752019f3`, ale jedynym
   pisarzem jest `boot/ramjump.S:12` (skok do obrazu RAM). Ctrl+Alt+Del
   (`main.c:12-17`) robi `reset` bez zapisu magic, więc POST zawsze zaraportuje
   `C` (cold). Diagnostyka na kości będzie myląca (funkcjonalnie RAM jest
   zachowany).
7. **`fdc_current_track` jest jednym globalem dla dwóch stacji.**
   `hw/fdc.c:50-63`: jeśli `track == fdc_current_track`, SEEK jest pomijany,
   a zmienna nie jest per-stacja. Boot używa tylko A:, więc dziś to latentne,
   ale ścieżka systemowa może czytać A i B w jednej sesji (katalog F5/FDD).
   Po zimnym starcie `.data` jest odtwarzane z ROM (`reset.S:84-95`), więc
   wartość startowa 0xFF wraca — to akurat złagodzone.
8. **`cpu_probe_030_cacr` chwilowo włącza bit 8 CACR (data cache) na 68030.**
   `boot/exception.S:173-183` zapisuje `d0` z ustawionym bitem 8 i przywraca
   `d1`; bez `cpusha`/invalidacji. W praktyce przy detekcji CPU na gołym
   sprzęcie to krótkie okno; warto potwierdzić na 68030, że nie zostawia
   nieświeżych odczytów. (Nie potwierdzone jako błąd — do weryfikacji na kości.)

---

## 4. Ryzyka przed wypaleniem EPROM-u

| # | Ryzyko | Dlaczego groźne | Dowód |
|---|---|---|---|
| R1 | ACSI read-only | Użytkownik z ACSI nie zapisze konfiguracji ani nie użyje failoveru; system wygląda na działający i traci dane po resecie | `hw/acsi.c:120-125` |
| R2 | FDC 0xffff860e / gęstość | Na MegaSTE/TT boot z FDD może nie zadziałać, choć na ST/STE w Hatari działa | `hw/fdc.c` (brak rejestru), `FLOPPY-BOOT-PLAN.md:39` |
| R3 | Timing IDE/CF i spin-up | BIOS skanuje dysk wcześnie; CF/HDD może nie odpowiedzieć → brak bootu lub „NO BOOT DEVICE” | `hw/ide.c:36,56-69,66` |
| R4 | Brak `diag_ram_pattern` w POST i braku sumy ROM | Wada RAM/wsadu ujawni się dopiero po wypaleniu i włożeniu kości | `diag/tests.c:67`, `ui/post.c:42`, checklist `:66` |
| R5 | Wybór banku EPROM (0xFC0000 ST vs 0xE00000 MSTE) | Zły mapping/dekodowanie = brak bootu mimo poprawnego obrazu; nie da się tego w pełni sprawdzić bez sprzętu | `V10-STATUS.md:373-378`, `build/atos-bios-tf536-st-dual.bin` (512 KB) |
| R6 | Failover wideo zależy od `cfg_save()` | Na ACSI zapadka `video_pending` się nie zapisze → automatyczny powrót na Shifter nie zadziała | `main.c:21-24`, `V10-STATUS.md:435-446`, `hw/acsi.c:120-125` |
| R7 | 68000 vs 68030 (mmu030 w obrazie ST) | `boot/mmu030.S` (`-m68030`) jest linkowany do ROM-u ST; jeśli detekcja CPU zawiedzie i `ui/tos.c:27` wejdzie w MMU na 68000, MOVEC = illegal | `Makefile:20,55-57`, `ui/tos.c:27`, checklist `:22-23` |
| R8 | Obraz dwubankowy vs pojedyncze | `atos-bios-st-68000.img` = bank1 dualnego (zweryfikowane binarnie), ale to nie dowód poprawnej pracy na realnym TF536 | patrz §6/pomiary |

---

## 5. Priorytetowy plan następnych kroków (max 8)

1. **Dodać prawdziwy zapis ACSI albo twardo oznaczyć ACSI jako read-only w UI.**
   Uzasadnienie: to najbardziej „cichy” błąd na fizycznym ST/STE (R1). Pliki:
   `hw/acsi.c:120-125`, `main.c` (cfg_save), `system/kernel_block.c:137`,
   `docs/V10-STATUS.md` (usunąć sprzeczny akapit `:710-714`).
2. **Dodać testy z asercjami dla „hostile PATH” i `-2`/reuse pamięci procesu.**
   Uzasadnienie: dziś punkt 2 i wątek `-2` opierają się na zrzutach, nie na
   bramce. Pliki: nowe `atos2-tools/scenario_*_assert.py` (na bazie
   `scenario_faultsep.py`), ewentualnie host-test wywołujący
   `kernel_user_end`→`kernel_spawn_user_async`; `tools/build_atos2.py`.
3. **Spiąć `diag_ram_pattern` (i pozostałe `diag_*`) z POST-em + dodać sumę
   kontrolną ROM-u.** Uzasadnienie: R4 — to jedyny sposób, by przed EPROM-em
   wykryć wadliwy RAM/wsad. Pliki: `ui/post.c:42`, `ui/diagnostics.c:19-26`,
   `diag/tests.c:67`, `include/diagnostics.h:16`, ew. `diag/tests.c` nowa
   `diag_rom_checksum`.
4. **Dodać obsługę 0xffff860e (gęstość/zegar FDC) dla MegaSTE/TT i przetestować
   na profilu MegaSTE.** Uzasadnienie: R2. Pliki: `hw/fdc.c:44-73`,
   `docs/FLOPPY-BOOT-PLAN.md:39`, `atos2-tools/scenario_fdboot.py`.
5. **Utwardzić detekcję/spin-up IDE: retry IDENTIFY z odczekaniem i dłuższym
   timeoutem przy pierwszym starcie.** Uzasadnienie: R3. Pliki: `hw/ide.c:56-69`,
   `hw/block_device.c`, `loader/bootstage.c:207-225`.
6. **Uporządkować sprzeczne dokumenty (jedno źródło prawdy).** Uzasadnienie:
   `V10-STATUS.md` „Do zrobienia” i `SHELL-REFACTOR-PLAN.md` wprowadzają w błąd
   przy każdej kolejnej sesji. Pliki: `docs/V10-STATUS.md:630-640,707-721`,
   `docs/SHELL-REFACTOR-PLAN.md:104-243`, `docs/PLAN-GEM-KOMPATYBILNOSC.md:74-207`,
   `docs/BIOS-BRINGUP-CHECKLIST.md:60-73`.
7. **Dodać builtiny shella COPY/MOVE/DEL/MKDIR (lub świadomie skreślić je
   z roadmapy) oraz uzupełnić `help`.** Uzasadnienie: punkt 4 „DIR/CD/PWD/TYPE/
   COPY/MOVE/DEL/MKDIR”. Pliki: `system/shell_module.S:2429-2449`
   (tabela + dispatch), API `system/kernel_fat.c:431,551,566`,
   syscalle `system/kernel_exception.c:1505-1507`.
8. **Poprawić `console_lines()` w harnessie wg rzeczywistej semantyki
   `term_scroll`.** Uzasadnienie: obecna implementacja czyta ostatni wiersz,
   nie widoczny ekran, i zawsze zawodzi `type_checked()`; docstring opisuje
   nieistniejący ring buffer. Pliki: `atos2-tools/atos2_auto.py:198-256`,
   `system/text_terminal.c:41-47`, `include/text_terminal.h:21-22`.

---

## 6. Podejrzane „zrobione” (nie da się potwierdzić / sprzeczne z kodem)

1. **„`path`: builtin usunięty”** (`V10-STATUS.md:630-640`) — **nieprawda**:
   builtin żyje (`shell_module.S:763-767,907-934`).
2. **„`test_sysinfo_api.py` / `test_user_sleep_wakeup.py` padają”**
   (`V10-STATUS.md:56-57`) — **nieprawda w obecnym drzewie**; oba przechodzą
   (uruchomione read-only, `OK`). Gdyby padały, `build_atos2.py:32` (`check=True`)
   przerywałby `--install`.
3. **„`AUTOEXEC.BAT` nie wymusza już `C:`”** (`V10-STATUS.md:671-674`) —
   plik nie jest instalowany ani wykonywany, więc opis dotyczy martwego pliku;
   dodatkowo wskazuje `\SYSTEM\COMMAND\PATH.APP`, którego obecny installer
   nie tworzy (`build_atos2.py:39-40`).
4. **`console_lines()` „brakuje tylko offsecie komórek”** (`SHELL-REFACTOR-PLAN.md:229-238`)
   — **błędna diagnoza**: `term_scroll` to memmove (`text_terminal.c:41-47`),
   nie ring; komórki są na offsecie 0 (`text_terminal.h:21`, potwierdzone
   `m68k-elf-nm` = `consoles` @ `0x46d58`). Problem to złe `(full_rows-rows)*cols*2`
   i nieaktualny docstring, nie „układ struktury”.
5. **„`diag_ram_pattern` dodane / POST z testami”** — funkcja istnieje, ale
   **nie jest wywoływana** (grep 0 wywołań) i nie ma jej w `ui/diagnostics.c`
   ani `ui/post.c`; checklist wciąż mówi „brak”. Zrobione tylko połowicznie.
6. **„Zapis z jądra na IDE — NAPRAWIONE”** (`V10-STATUS.md:478-484`) — kod
   wsparcia jest (`hw/acsi.c:122`, `include/boot_protocol.h:68`,
   `system/kernel_block.c:137`), ale **ten sam dokument 30 linii dalej** pisze
   „BRAK ŚCIEŻKI” (`:710-714`) i „IDE-only nierozstrzygający” (`:72-78`).
   Nie potwierdzone jednoznacznie.
7. **„Hostile PATH — ani razu panic”** (`V10-STATUS.md:699-702`) — brak
   zautomatyzowanego dowodu; istnieją tylko scenariusze zrzutów
   (`scenario_faultsep.py`, `scenario_pathfix.py`, `scenario_abifix*.py`).
8. **„`-2` wynika z pamięci / potem: pamięć nie wycieka”** — kod teardownu jest
   poprawny (`kernel_exception.c:866-875`, `kernel_memory.c:94-98`), ale
   sprzeczne wnioski w `SHELL-REFACTOR-PLAN.md` nie domykają obserwacji `-2`.
   Nie potwierdzone jednoznacznie.
9. **`DESKTOP-ARCHITECTURE.md`: „żadnego kodu pulpitu”** — kod pulpitu istnieje
   (`system/desktop_module.c`, `system/desktop_app.S`,
   `Makefile:908-911`). Dokument nieaktualny.
10. **`PLAN-GEM-KOMPATYBILNOSC.md`: „trap #2 AES nie istnieje”, „brak ochrony
    faultów”, `TOS_HEAP_SIZE 65536`, `Mfree` nieobsłużone** — wszystko
    nieaktualne wobec `system/tos_prg.c:591,981-1000,1110,1396-1600` i
    `system/tos_entry.S:206-235`.
11. **`PLAN-GEM`: „Mshrink → return 0 = sygnał błędu”** — błędna interpretacja:
    w GEMDOS 0 = sukces; kod `tos_prg.c:1111` zwraca 0 (czyli „sukces”).

---

## 7. Weryfikacje wykonane bez uruchamiania emulatora (dla porządku)

- **Rozmiary/relacje ROM** (dowód binarny): `build/atos-bios-tf536-st-dual.img`
  = 524 288 B; `build/atos-bios-st-68000.img` = `build/atos-bios-mste.img` =
  262 144 B. Pierwsze 256 KB dualnego jest **bit-identyczne** z
  `atos-bios-st-68000.img`, drugie 256 KB z `atos-bios-mste.img`;
  `.bin == .img`; pierwsze 192 KB banku ST = `bios_st.bin`, resztę do 256 KB
  wypełnia `0xFF`; `mste.img` = `bios_mste.bin`. **Zgodne z dokumentacją.**
- **Bramka ABI**: `python3 tools/asm_abi_audit.py --check <17 plików>` →
  `ABI CHECK PASS`, 0 naruszeń. Uwaga: heurystyka regex (`asm_abi_audit.py:12-61`)
  ma ślepe plamy — sprawdza tylko wypisane pliki, granicą funkcji jest poprzednie
  `rts`, a dla instrukcji nie-`move` traktuje ostatni operand jako zapis (może
  dawać fałszywe trafienia dla `cmpi/tst`); nie obejmuje C ani plików BIOs-u
  spoza listy.
- **Symbole harnessu**: `m68k-elf-nm build/system/kernel.elf` → `consoles` @
  `0x46d58`, zgodnie z `atos2-tools/atos2_auto.py:181-196`.
- **Dwa testy hostowe** uruchomione w tymczasowym środowisku (bez zapisu do
  projektu): `tests.test_sysinfo_api` → OK, `tests.test_user_sleep_wakeup` → OK.

> Zastrzeżenie: wszystkie wnioski runtime (Hatari) pochodzą z dokumentacji;
> w tym przeglądzie nie uruchamiano emulatora na polecenie zleceniodawcy.
> Fragmenty, których nie dało się potwierdzić w kodzie, oznaczono jawnie
> „nie potwierdzone”.
