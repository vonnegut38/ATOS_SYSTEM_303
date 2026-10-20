# Przegląd ostatnich zmian — ATOS 2.1

Stan: 2026-10-06 ~20:30. Zakres: drzewo `ATOS 2.1/` (żywe; ostatnie zmiany źródeł 2026-10-05/06).
Metoda: diff względem snapshota `work/before-boot-speed-20261004-153726/`, przegląd statyczny
najnowszych plików, analiza artefaktów budowy (`readelf/objdump/nm`), uruchomienie testów
hostowych, odczyt obrazu `work/atos-tos1.img`. Bez przebudowy (HANDOVER §4 ostrzega, że
`tools/build_atos2.py` nadpisuje `build/system/`) i bez uruchomień Hatari.

## 0. Werdykt

1. **BLOKER (potwierdzony dowodowo): `GUI_APP_CFLAGS` z `-mno-pcrel` (Makefile:1151) psuje
   wszystkie aplikacje `.APP`.** To wyjaśnia czarną tapetę i martwy F10 — nie jest to wina
   harnessa ani trasowania klawiszy.
2. **Weryfikacja, na której opiera się HANDOVER §1, jest nieodtwarzalna**: skrypt
   `atos2-tools/scenario_tos_module.py` ma zakodowane na sztywno adresy, które dziś wskazują
   w `async_results`, a firmowa bramka PIC `tools/check_gui_apps.py` jest czerwona.
3. **Zapis/odczyt ustawień TOS jest kruchy**: jądro czyta `DESKTOP.CFG` buforem 191 B, gdy
   desktop potrafi zapisać ~200-360 B → opcje `TOSTA/TOSTT` są po cichu ignorowane.
4. **Okno SETTINGS nie urosło** przy dodaniu 2 wierszy (SET_ROWS 6→8) — ostatnie wiersze
   nachodzą na linie statusu i podpowiedzi.
5. Drobniejsze: wyciek stanu `tos_window_enabled` na ścieżkach błędów `kernel_tos_run_program`,
   „zatrute” TOSC po nieudanym pierwszym ładowaniu modułu, martwe komentarze o `ATOS.CFG`,
   sprzeczny komentarz w Makefile o adresie ładowania aplikacji.

Testy hostowe: **37/37 PASS** (`build/tests/*`, artefakty z 2026-10-06 19:00). Regresje są
poza zasięgiem tych testów — dotyczą kodu 68k i modelu ładowania aplikacji.

## 0a. Status wdrożenia (2026-10-06, wieczór)

Plan z audytu został **wdrożony i zweryfikowany** — szczegóły w
[`AUDYT_2026-10-06_WDROZENIE/WDROZENIE.md`](<AUDYT_2026-10-06_WDROZENIE/WDROZENIE.md>)
(kompletny patch: [`wdrozenie.patch`](<AUDYT_2026-10-06_WDROZENIE/wdrozenie.patch>)).
W skrócie, wszystkie punkty z §1-§7 poniżej są zamknięte albo świadomie odłożone:

| obszar | stan |
|---|---|
| `-mpcrel` + `fb_all` na stercie, `GUI_APP_CFLAGS` w `build/.profile`, bramka PIC w `all` i `check` | **zrobione**; `make all` EXIT 0, bramka zielona dla 10 aplikacji, luz branch 12 661 B |
| widoczność awarii (koniec quiet boot, nadzór nad DESKTOP.APP) | **zrobione**; obraz bez `DESKTOP.APP` pokazuje `DID NOT START (code 1)` + `TEXT SHELL ACTIVE` |
| SETTINGS: granice listy, Left/Right, `SAVE FAILED`, układ okna, bufor `DESKTOP.CFG` 512 B, komunikat o restarcie | **zrobione**; `work/uidrive.py` ALL CHECKS PASS, `TOSTA=1/TOSTT=1` w obrazie |
| testy: adresy z `kernel.elf`, świeży licznik przebiegu, asercje, kody wyjścia | **zrobione**; `scenario_tos_module.py` i `scenario_tos_accept.py` ALL PASS |
| WM: wąskie zamykanie okna, zwalnianie `wm_window_req`, zakres uchwytów TOS, damage klienta, Vsync pasmowy, centrowanie, tytuł | **zrobione** |
| TOS: stan po błędach, retry TOSC, `p_env`, blokada reentrancji wejścia, odtworzenie palety | **zrobione** |
| VDI na buforze programu, `source_capacity`, zagnieżdżony Pexec, zakres palety | **świadomie odłożone** (opis w WDROZENIE §7) |

**Znaleziony przy okazji i naprawiony błąd zastany:** po odciszeniu konsoli (koniec quiet
boot) okazało się, że każde czyste `Pterm()` kończyło się `TOS FAULT vec=2` i `TOS RC -2`,
bo diagnostyczny `rd32(frame+2)` bus-errorował na nieużytecznym wskaźniku. Po naprawie
(`tos_ptr_plausible`) akceptacja TOS raportuje **`TOS RC 0`**.

---

## 1. BLOKER: `-mno-pcrel` = absolutne adresowanie w aplikacjach przenośnych

**Fakt ABI (udokumentowany w samym repo):** aplikacje są linkowane na adres 0
(`linker/app-sys.ld:5`), a loader kładzie obraz pod adresem runtime i **nie relokuje go**:

* `system/kernel_module.c:564` — `image=kmem_alloc(file_size,…)`,
* `system/kernel_module.c:575` → `kernel_user_create_loaded(...)`,
* `system/kernel_exception.c:1241` — `child->saved_pc=entry` (adres runtime),
* `system/app_math.c:5-16` — wprost: „applications are linked at address 0 … the loader places
  them at an arbitrary heap address; `-mpcrel` makes the app's own code and data references
  PC-relative … an absolute long jump lands in low RAM and the process dies”.

`-mno-pcrel` zamienia **wszystkie** odwołania do kodu i danych na adresy absolutne (= offsety od 0).

**Dowody z artefaktów (build z 2026-10-06 19:00):**

| plik | `-mno-pcrel` (dziś) | `-mpcrel` (snapshot 10-04) |
|---|---|---|
| `build/system/desktop_wm.o` | 1455 × `R_68K_32`, 0 × `R_68K_PC16` | 1213 × `R_68K_PC16`, 0 × `R_68K_32` |
| `desktop_wm.elf` (zlinkowany) | brak relokacji, `jsr/jmp` absolutne (311), `%pc@` = 0 | `%pc@` = 1214, absolutnych operandów 0 |

Przykład z `build/system/DESKTOP.APP` (offset = VMA): `45 f9 00 00 00 2c 4e 92` =
`lea (0x2c).l,%a2 ; jsr %a2@` — pierwsze wywołanie w `app_main` skacze pod adres 0x2c
zamiast do `gui_call` w obrazie. Skutek: `lookup("SYSTEM.GFX")` zawodzi →
`if(gfx_pid<0||gui_pid<0)return 1;` (`desktop_wm.c:2126`) → aplikacja wychodzi **przed**
`GUI_BEGIN_KEYS` (:2131), `GFX_DESKTOP` (:2132) i `paint()` (:2150). Dlatego: brak tapety
(nikt jej nie maluje) i brak właściciela klawiatury GUI (F10 idzie do sesji shellowej).
Kod obsługi F10 (`desktop_wm.c:2225`, `scan==0x44`) jest nienaruszony.

Dodatkowe potwierdzenia:

* `python3 tools/check_gui_apps.py` → `AssertionError: SYSINFO absolute call: 182: 4ef9 0000 0048 jmp 48 <copytext>`
  — **własna bramka PIC w repo jest czerwona** dla obecnego builda.
* `build/system/app_math.o` jest budowany tymi samymi flagami (`Makefile:1157`), więc plik
  napisany po to, by uniknąć absolutnych skoków libgcc, sam używa adresowania absolutnego.
* Teza z `Makefile:1149-1150` („Apps are loaded at a fixed address, so absolute addressing is
  correct”) jest **sprzeczna** z komentarzem 3 linijki niżej (`Makefile:1152-1154`: „the app
  image is loaded at a runtime address”) oraz z `app_math.c:5`. Komentarz do poprawy.
* Obraz `work/atos-tos1.img` zawiera dokładnie bieżący build (`SYSTEM/APP/DESKTOP.APP` =
  30 214 B, zgodny z `build/system/DESKTOP.APP`), więc objaw dotyczy tego, co się testuje.

**Rekomendacja (kolejność):**
1. Szybkie rozstrzygnięcie bez przebudowy: odczytać w Hatari `kdbg_uf_count/vector/pc/image`
   (adresy z `nm build/system/kernel.elf`) — przy `-mno-pcrel` oczekiwane
   `kdbg_uf_count>0`, wektor ∈ {2,3,4,8} i `pc` w niskim RAM.
2. Wrócić do `-mpcrel` (`Makefile:1151`) i **zbić obraz aplikacji poniżej ±32 KB**.
   Dziś sam `desktop_wm.o` ma 35 228 B (text 27 764 + rodata 2 232 + data 82 + bss 5 150).
   Największe bufory `.bss` (`fb_all` 1536 B, `launch` 656, `ic_key` 512, `ma_info` 444,
   `pending` 320) przenieść do alokacji runtime przez `gui_call(16,…)`, tak jak już zrobiono
   z `list` (`desktop_wm.c:2139`) — to ~3,5 KB zapasu.
3. Docelowo (jeśli 32 KB ma przestać być ograniczeniem): realna relokacja — `--emit-relocs`
   albo własna sekcja relokacji w `.APP` i pętla `base+addend` w `kernel_spawn_user_async`
   obok `verify_app_image` (`kernel_module.c:528`); albo stały adres ładowania równy adresowi
   linkowania. „0 relokacji w ELF” **nie jest** dowodem bezpieczeństwa — przy adresowaniu
   absolutnym adresy są domykane na etapie linkowania i nie zostawiają wpisów.

### 1a. Weryfikacja kandydata poprawki (audyt z 2026-10-06, Hatari + build kontrolny)

Audyt użytkownika (`outputs/Audyt-ATOS-2026-10-06-start/`, drzewo Codex z 2026-10-04) potwierdził
przyczynę uruchomieniowo (`kdbg_uf_pc=0x2c`, `kdbg_uf_vector=11` Line-F, `end_image=0x01096440`,
status -267) i dostarczył patch: powrót `-mpcrel` + `fb_all[64]` na stertę przez `gui_call(16,…)`.
Patch zweryfikowałem niezależnie w kopii źródeł (`/tmp/atos-fixcheck`, drzewo główne nietknięte):

* patch stosuje się czysto (`patch -p1 --dry-run` → exit 0);
* `desktop_wm.o`: **1264 × `R_68K_PC16`, 0 × `R_68K_32`** (pełne PIC), `app_math.o`: 2 × PC16, 0 × 32;
  3 × `R_68K_32` w `gui_app_runtime.o` to pola nagłówka (`__app_file_end`, `__app_memory_end`,
  `.text.startup`) — offsety, nie adresy;
* zapas zasięgu w zlinkowanym `desktop_wm.elf` (28 844 B): największe odwołanie danych przez `%pc`
  = 2024 B (luz **30 743 B**), najdłuższy branch 16-bitowy `bsrw app_main` = 20 092 B
  (luz **12 675 B**) → sufit ±32 KB nie wróci przy najbliższej zmianie kodu;
* `make TOS=1 -j4 all` w kopii: **EXIT=0**, zero komunikatów `relocation truncated`;
* `fb_all` bez zerowania po `kmem_alloc` jest bezpieczne: wszystkie odczyty są ograniczone przez
  `fb_count`/`fb_total` (`desktop_wm.c:678, 1050, 1360, 1720`), więc brak inicjalizacji `.bss`
  nie zmienia zachowania (jawne czyszczenie to tylko hardening).

**Pułapka potwierdzona eksperymentalnie:** `PROFILE_STAMP := build/.profile` (`Makefile:17`)
zawiera wyłącznie bazowe `CFLAGS` (bez `GUI_APP_CFLAGS`), więc zmiana `-mno-pcrel` → `-mpcrel`
**nie unieważnia obiektów**. Po nałożeniu patcha i zwykłym `make all` odbudował się tylko
`DESKTOP.APP` (bo skasowałem jego obiekty), a `SYSINFO.APP` i reszta zostały z adresowaniem
absolutnym — firmowa bramka nadal czerwona:

```
$ python3 tools/check_gui_apps.py
AssertionError: SYSINFO absolute call:  182: 4ef9 0000 0048  jmp 48 <copytext>
```

Po `rm -f build/.profile && make TOS=1 -j4 all` (≈15 s CPU) bramka jest **zielona dla wszystkich
10 aplikacji GUI**. Wniosek: wymuszona przebudowa to element naprawy, nie ostrożność.

---

## 2. Ustawienia TOS: zapis 384 B, odczyt 191 B → opcje po cichu ignorowane

* Zapis: `desktop_wm.c:2050` — `char buf[384]`, nagłówek + po jednej linii na ikonę
  (`:2068-2076`), maksymalnie 11 ikon × do 17 B (label 12 znaków, `desktop_wm.c:216`).
* Odczyt: `kernel_main.c:176` — `static char buf[192]`, `:182` czyta z pojemnością
  `sizeof(buf)-1` = 191.
* `kfat_load_system_file` **odrzuca** plik większy od pojemności
  (`kernel_fat.c:142`: `capacity<file_size → FAT_FILE_CORRUPT`), a `tos_cfg_read` wtedy
  po prostu wraca (`:182`) — `TOSTA/TOSTT` wracają do 0 i **nic nie sygnalizuje błędu**.
* Próg jest realny: nagłówek 30 B (`RES`, `THEME`, `TOSTA`, `TOSTT`), a przy 11 ikonach
  same wpisy to 36 B (builtiny) + do 136 B (8 skrótów po 12 znaków) ≈ 202 B.
  W obecnym obrazie plik ma 30 B (wgrany instalatorem, nie przez SAVE SETTINGS), dlatego
  problem jeszcze nie wystąpił.
* `if(size>=sizeof(buf))size=sizeof(buf)-1u;` (`kernel_main.c:183`) jest **martwym** kodem —
  po sprawdzeniu pojemności `size` nigdy nie przekroczy 191.

Fix: bufor ≥ 512 B (albo odczyt przez `read_path_at`, albo osobny plik tylko dla TOSTA/TOSTT)
+ diagnostyka, gdy plik jest nieczytelny.

---

## 3. SETTINGS: okno za małe o dwa wiersze

* `SET_ROWS` 6 → 8 (`desktop_wm.c:707`), `SET_TOP=44`, `SET_ROW_H=24`, `st_wh=256` (:713, bez zmian).
* Wiersz 6 (`PREFER TT-RAM`): y = 188..208; wiersz 7 (`SAVE SETTINGS`): y = 212..232.
* Tymczasem linia statusu jest rysowana na `st_wh-56` = **y=200** (`:802`), a podpowiedź na
  `st_wh-40` = **y=216** (`:803-804`) → oba nowe wiersze nachodzą na tekst stopki.
* `st_wh` nie jest przeliczane przy otwarciu okna (`:849-852` używa stałej), a `gfx(29,…)`
  ustawia wysokość klienta na `st_wh-16` (`:806`).

Fix: zwiększyć `st_wh` (np. do 256+2*24=304) albo liczyć je z `setup_rows()`, i przesunąć
stopkę pod listę wierszy.

---

## 4. `kernel_tos_run_program`: stan nie sprzątany na ścieżkach błędów

* `tos_prg.c:2099` — `tos_window_enabled=1u;` ustawiane **przed** walidacją pliku.
* Wczesne wyjścia: `:2102` (-2, brak pliku), `:2105` (-3), `:2108` (-4), `:2111` (-5),
  `:2115` (-6), `:2122` (-6), `:2126` (-7), `:2217-2220` (-0x40).
* Zerowanie jest tylko na ścieżce sukcesu: `:2305` (plus ręczne `case 8u` w T2, `:1284`).
* Skutek: po nieudanym uruchomieniu (np. dwuklik na brakującym `.PRG` w pulpicie) flaga
  „tryb okna” zostaje włączona do następnego przebiegu; `tos_composite_*` i `tos_open_window`
  patrzą właśnie na nią. Dziś kolejne uruchomienie nadpisuje flagę, więc objaw jest łagodny,
  ale to wyciek stanu i pułapka przy dalszej rozbudowie.

Fix: jedno wspólne zakończenie (`goto`) z zerowaniem `tos_window_enabled`, `tos_tpa_next`
i ewentualnym zwolnieniem buforów dla wszystkich ścieżek.

---

## 5. Wiarygodność weryfikacji (to kosztuje najwięcej czasu w kolejnych sesjach)

1. **Scenariusz TOS.SYS czyta złe adresy.** `atos2-tools/scenario_tos_module.py:28-31`
   ma zakodowane `loads=0x087cb8`, `module_screen=0x087cb0`, `fallbacks=0x087cb4`,
   `adopted=0x087cac`. W bieżącym `build/system/kernel.elf`:
   `kdbg_tos_adopted_module=0x87c6a`, `kdbg_tos_module_screen=0x87c6e`,
   `kdbg_tos_screen_fallbacks=0x87c72`, `kdbg_tos_compat_loads=0x87c76`.
   Wszystkie cztery zakodowane adresy leżą **wewnątrz `async_results` (0x87c98+)** — skrypt
   czyta pola tabeli zadań asynchronicznych, nie diagnostykę TOS. Zapisane wyniki
   (`work/tosmodule.out` 17:43, `work/tosmodule_missing.out` 17:49) są starsze niż bieżący
   kernel (19:00) i obraz (19:52), więc liczby z HANDOVER §1 („loads=1, adopted=1,2,
   fallbacks=0”) **nie pochodzą z obecnych artefaktów**. To wprost łamie regułę z HANDOVER §3
   („NEVER hardcode `kdbg_*` addresses”). Infrastruktura do rozwiązywania symboli istnieje:
   `atos2-tools/atos2_auto.py:219-236`.
2. **Bramka PIC jest czerwona** (`tools/check_gui_apps.py`, szczegóły w §1) — a nie ma jej
   w `Makefile`, więc build TOS=1 jej nie uruchamia.
3. **Audyt ABI nie obejmuje modułu**: `tools/build_atos2.py:25-33` nie zawiera
   `system/tos_module.S`, więc niezmiennik „D3 zachowane” (naprawiany ręcznie) nie jest
   niczym pilnowany.
4. **ATOS 2.1 nie jest w gitcie.** `git status` w katalogu nadrzędnym pokazuje
   `?? ATOS 2.1/`; jednocześnie jedyne zmiany śledzone (`commander-v10`, `files-...`) mają
   daty 2026-09-15…09-30, czyli nie są „ostatnimi zmianami”. Dla drzewa ATOS 2.1 istnieją
   tylko dwa ręczne snapshoty z 10-04, obejmujące 3 pliki źródłowe. Wniosek: brak możliwości
   zrobienia prawdziwego diffu i realne ryzyko utraty pracy — warto dodać `ATOS 2.1/` do repo
   (poza `work/`, `build/`, `*.img`).

---

## 6. Co się zmieniło w ostatniej sesji (2026-10-05/06) i jak to oceniam

| obszar | pliki (mtime) | ocena |
|---|---|---|
| Aplikacja TOS w oknie pulpitu (WM + kompozytor) | `kernel_wm.c` (15:33), `wm_core.c` (15:19), `kernel_graphics.c` (07:46), `kernel_main.c` (18:46) | kierunek dobry; klip/band i przeniesienie konwersji z Timer C do pętli głównej sprawdzone, brak błędów w mapowaniu argumentów `kgfx_blit_planar` |
| Moduł TOS.SYS (working set, TOSTA/TOSTT) | `tos_module.S` (17:36), `kernel_module.c` (17:37), `kernel_module.h` (18:35), `scenario_tos_module.py` (17:37) | D3/kody błędów/zwolnienie bufora OK; zastrzeżenia: TOSC „zatruwa” retry (§7), scenariusz nieaktualny (§5.1) |
| Start programu TOS | `tos_prg.c` (18:35), `tos_entry.S`/`kernel_entry.S` (03:21) | wyciek stanu na błędach (§4) |
| Pulpit: SETTINGS + ikony + routing PRG | `desktop_wm.c` (18:59), `tos_ext.h` | dobre poprawki (patrz §8); SETTINGS za małe okno (§3), `ATOS.CFG` w komentarzach |
| Build | `Makefile` (18:59), `tools/build_atos2.py` (16:11), `linker/tos-module-sys.ld` (16:11) | **regresja: `-mno-pcrel`** (§1) |
| Boot | `kernel_main.c` (usunięty drugi splash), `ui/post.c` (15:34) | OK: `boot_hw` ustawiane w `kernel_startup_services` (`kernel_main.c:260`), pierwszy `boot_splash` wołany tam (:261) — usunięcie wcześniejszego wywołania nie gubi logo; pasek postępu dopisuje tylko nowe komórki (porównanie etykiet po wskaźniku jest bezpieczne — wołane wyłącznie z literałami: `loader/bootstage.c:80,230`) |

---

## 7. Inne znalezione defekty (mniejsza waga)

* **TOSC publikowane przed ładowaniem i nigdy nie wycofywane** (`kernel_module.c:757-772`):
  po nieudanej pierwszej próbie wpis `KERNEL_SERVICE_TOS_CFG` zostaje w tabeli, więc kolejne
  wywołanie `kernel_tos_compat_ensure` przewraca się na kontroli duplikatu
  (`kernel_publish_service`, `:462`) → `return -110` **przed** ponownym ładowaniem.
  Skutek: brak jakiegokolwiek retry do końca sesji (także dla przyczyn przejściowych) i mylący
  kod błędu (-110 „interface error” zamiast -100 „module not found”). Fix: zapamiętać wynik
  publikacji w `static uint8_t` i przy kolejnych próbach tylko aktualizować `prefer_tt`.
* **Wyciek przy nieudanej walidacji po udanym ładowaniu** (`kernel_module.c:773-777`):
  moduł ma już zaalokowane 64 KB i zarejestrowaną usługę, a przy -120..-123 nikt tego nie
  zwalnia; `tos_compat_service` zostaje ustawione na wadliwy wskaźnik (dziś bez wywołań —
  mina). W połączeniu z powyższym nie da się tego powtórzyć ani posprzątać.
* **Okno TOS centrowane na sztywno dla 800x600.** `tos_prg.c:1883-1884` i `:1984-1985` liczą
  `(800-tos_win_w)/2` i `(600-tos_win_h-TOS_TITLE)/2`, choć `kvideo_mode_width()` jest już
  zadeklarowane (`:53`) i używane (`:2003`). Na pulpicie 640x400 (Shifter) okno 640 px ląduje
  na x=80 (ucinane z prawej), a na 1024x768 nie jest wyśrodkowane. Fix: użyć realnego rozmiaru
  ekranu (i tak samo przy zmianie trybu).
* **Zamknięcie okna TOS zamyka WSZYSTKIE okna procesu wywołującego.** `tos_open_window`
  rejestruje okno pod `wpid=kernel_user_current_pid()` (`tos_prg.c:1994-1998`), a program TOS
  uruchamiany jest synchronicznie w kontekście wołającego (`gui_call(65)`), więc `wpid` to pid
  pulpitu (albo shella). `tos_close_window` (`tos_prg.c:2035`) woła
  `kernel_wm_release_surface(pid,handle)`, a ta — `kernel_wm_close_pid(pid)`
  (`kernel_wm.c:1967-1977`, `:111-119`) — zamyka **każde** okno tego pid (poza tłem, `flags&4`).
  Tymczasem okna dialogowe pulpitu (menedżer plików, SETTINGS, kosz, wybór ikony) też są oknami
  WM tego samego pid: każdy `GFX_BATCH` z uchwytem okna zakłada surface przez
  `kernel_wm_app_surface(argument0,handle)` (`kernel_exception.c:2101`). Skutek: po zakończeniu
  programu TOS uruchomionego np. z okna menedżera plików znikają również pozostałe okna pulpitu
  (menedżer plików, SETTINGS, kosz). Desktop jest o tym powiadamiany
  (`window_close` → `kernel_user_notify_window_close`, `kernel_wm.c:679-680`) i czyści stan, więc
  to nie „rozjazd stanu”, a utrata okien, których użytkownik nie zamykał.
  Fix: zamykać tylko okno `(pid,handle)` (wąska funkcja zamiast `kernel_wm_close_pid`).
* **VDI nie trafia w bufor modułu** (rozjazd „working set”): `tos_vdi_set_surface` jest wołane
  tylko raz, z żywym framebufferem jądra (`kernel_main.c:566` → `kernel_graphics.c:505-518`);
  drugi caller, `kernel_tos_use_surface` (`tos_prg.c:167-174`), nie ma żadnego wywołania.
  Adoptowany ekran modułu obsługuje tylko Physbase/Line-A/`st_raster` (`tos_prg.c:1206`,
  `:424-432`, `:498`), a wywołania VDI programu idą przez `tos_vdi_call` (`tos_prg.c:1691`)
  do `vdi_target` = współdzielony ekran (`tos_vdi.c:14,49,71`) — czyli w to, przed czym
  ostrzega komentarz w `tos_prg.c:2196-2198`. Program widzi dwa różne ekrany zależnie od API;
  fix: `tos_vdi_set_surface(tos_screen, tos_src_stride())` przy adopcji i przywrócenie po
  zakończeniu. (Skutku wizualnego nie potwierdzono uruchomieniowo.)
* **Martwy warunek `magic != KERNEL_SERVICE_TOS`** (`kernel_module.c:775`) — `find_service`
  dopasowuje po `id`, które jest tym samym magic, więc warunek nigdy nie zajdzie.
* **HANDOVER §1 myli rozmiar**: „`system/tos_module.S` (264 B)” — źródło ma ~4,3 KB, 264 B to
  zbudowany `build/system/TOS.SYS`.
* **Komentarze kłamią o pliku konfiguracji**: `kernel_main.c:155` i `desktop_wm.c:718` mówią
  `\SYSTEM\ATOS.CFG`, kod czyta i pisze `DESKTOP.CFG` (`kernel_main.c:178`,
  `desktop_wm.c:2070`). Istniejący `work/ATOS.CFG` (`TOS_TT_RAM=1`, inna składnia) nie jest
  przez nic czytany.
* **Sprzeczny komentarz w `Makefile`** (1149-1150 vs 1152-1154) — patrz §1.
* **`in_tt` w komunikacie preloadu** (`kernel_main.c:584`): `(uintptr_t)sc >= st_ram_bytes`
  to heurystyka; przy `st_ram_bytes==0` lub ≥16 MB zawsze pokaże „TT RAM”.
* **Martwy `add(GUI_BG,8,20,31*8,16,…)`** w `paint()` (`desktop_wm.c:330`) — pozostałość po
  usuniętym echem statusu; niepotrzebny element listy przy każdym odświeżeniu.
* **Zduplikowana deklaracja zapowiadająca** `static int tos_program_path(const char*);`
  (`desktop_wm.c:473` i `:1027`) — kosmetyka.

---

## 8. Zmiany dobre / potwierdzone

* **Selektor ikon**: stare `for(j=0;j<51u;…)` z `id=j*2` sięgało id 100, a `icon_color_table`
  ma dokładnie 96 pozycji (`icons_color.h:16272`, tabela `:16174`) — id ≥ 96 wpadało w zakres
  ikon dynamicznych (`kernel_graphics.c:1119-1121`). Nowe `total=ICON_COLOR_COUNT/2` = 48
  i `ICP_ROWS=7` (49 komórek) jest poprawne; dodatkowo rysowanie partiami usuwa gubienie
  wiersza SAVE/CANCEL przy przepełnieniu listy rysowania, a dwuklik = zapis.
* **Ikony per rozszerzenie i per aplikacja** (`desktop_wm.c:1518-1550`) — wcześniej wszystko
  dostawało `ICON_DOKUMENT`/`ICON_SYSTEM`.
* **Wspólny, bezrozróżniający wielkości liter matcher** `.PRG/.TOS/.TTP/.GTP`
  (`include/tos_ext.h`) używany w pulpicie, menedżerze plików i shellu — jedna decyzja routingu
  zamiast trzech kopii; na to jest test hostowy `test_tos_ext` (PASS).
* **Rysowanie tylko uszkodzonych fragmentów** (`tos_composite_work`, `paint_icons`,
  `post_draw_progress`) — poprawne i celowe.
* **Moduł TOS.SYS**: 264 B, D3 zachowane na wszystkich ścieżkach (`tos_module.S:43,113-114`),
  kody błędów nigdy 0 (`kernel_module.c:772`), zwolnienie bufora przy nieudanym
  `register_service` (`tos_module.S:93-109`), brak przepełnienia bufora 64 KB
  (`kernel_graphics.c:578-579` waliduje `stride*height`).
* **Testy hostowe 37/37 PASS.**

---

## 9. Kolejność działań (proponowana)

1. **Krok 1 (bloker)** — nałożyć patch z audytu (§1a), **wymusić przebudowę**
   (`rm -f build/.profile && gmake TOS=1 -j4 all`), zainstalować spójny zestaw APP i uruchomić
   pulpit + pozostałe aplikacje. Równolegle: wpiąć `tools/check_gui_apps.py` w build TOS=1 i dodać
   `GUI_APP_CFLAGS` do `build/.profile`, żeby ta regresja nie mogła wrócić niezauważona.
2. **Krok 2 (widoczność)** — zakończyć quiet boot (`kconsole_quiet(0)`), sprawdzać wynik
   `kernel_spawn_user_async` przy starcie pulpitu (`kernel_main.c:563`) i raportować późniejszą
   śmierć procesu; bez tego każda awaria wygląda jak zwis.
3. **Krok 3 (SETTINGS + konfiguracja)** — błędy klawiatury (granica listy zamyka okno, Left/Right
   nieosiągalne), `SAVE FAILED` nadpisywane przez `CONFIG SAVED` (`desktop_wm.c:814`), wysokość
   okna z `SET_ROWS`, bufor `DESKTOP.CFG` ≥ 512 B + diagnostyka odczytu, komunikat o potrzebie
   restartu po zapisie.
4. **Krok 4 (testy)** — `uidrive.py` (wysyła Down zamiast Up, 5 identycznych zrzutów),
   `scenario_tos_module.py` (adresy z `kernel.elf`, asercje, timeout, zerowanie znaczników,
   kod wyjścia ≠ 0 przy porażce).
5. **Krok 4.5 (WM, zanim ruszy zgodność TOS)** — `kernel_wm_release_surface` nie może wołać
   `kernel_wm_close_pid` (`kernel_wm.c:1972`), `wm_window_req[].set` musi być zwalniane
   (`kernel_wm.c:1832`), uchwyty okna TOS nie mogą kolidować z 2/3/5/6/7, a blit klienta musi
   zgłaszać `kernel_wm_app_dirty`. Dwa pierwsze dają objaw „pulpit znowu czarny” i będą mylić
   diagnostykę.
6. **Krok 5** — dopiero teraz wracać do zgodności GEMDOS/BIOS/XBIOS i niezależnych `.PRG`;
   przy okazji: stan `tos_window_enabled` na ścieżkach błędów, retry TOSC, wyciek `p_env`,
   zakres palety 236..251 vs rampa motywu.
7. `git add "ATOS 2.1"` (bez `work/`, `build/`, `*.img`) — żeby następny przegląd dał się
   zrobić z diffu, a nie z mtime.

## 10. Czego nie sprawdzono

* Zachowania uruchomieniowego w Hatari (brak uruchomień w tej sesji): dokładny wektor błędu
  aplikacji `-mno-pcrel`, wygląd „czarnej tapety”, widoczność fallbacku TOS.SYS.
* Czy zarchiwizowany build `-mpcrel` z 10-04 (`work/before-boot-speed-20261004-153726/
  build/system/DESKTOP.APP`) współpracuje z obecnym jądrem/WM z 10-06.
* Czy harness klawiatury ma niezależny problem (z kodu nie da się tego stwierdzić; objaw
  tłumaczy brak własności klawiatury przez pulpit).
* Adresy z HANDOVER §1 (`0x108EE10`, `0x1FD520`) — brak artefaktu w repo, a `0x1FD520`
  pochodzi z przebiegu starszego niż bieżący kernel i nie rozróżnia wierszy tabeli.
