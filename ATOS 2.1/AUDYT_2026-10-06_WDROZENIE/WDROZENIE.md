# ATOS 2.1 — wdrożenie naprawy (2026-10-06)

Ten katalog jest wynikiem pracy nad planem z audytu `Audyt-ATOS-2026-10-06-start`
(UWAGI.txt + propozycja-naprawy.patch) oraz przeglądu `../PRZEGLAD-ZMIAN-2026-10-06.md`.
Zawiera:

* `wdrozenie.patch` — komplet zmian w źródłach ATOS 2.1 (9 plików). Nakłada się na
  stan **przed** naprawą (`patch -p1`), zawiera więc również przywrócenie `-mpcrel`
  z `propozycja-naprawy.patch` i jest jej nadzbiorem.
* `WDROZENIE.md` — ten opis: co zmieniono, dlaczego i czym to zweryfikowano.

Oryginalne obrazy (`work/atos-tos1.img`, `work/atos-tos-mod.img`, `work/atos-v2.img`)
**nie zostały zmienione** — testy szły na kopiach `work/fix-*.img`. Backup całych
źródeł przed wdrożeniem: `../../backup-atos21-20261006-204700/`.

## 1. Bloker: aplikacje `.APP` znów są PIC (krok 1)

* `Makefile:1148` — `GUI_APP_CFLAGS` wraca z `-mno-pcrel` na `-mpcrel` (loader
  przydziela obraz dynamicznie i nie relokuje; komentarz opisuje to poprawnie).
* `system/desktop_wm.c` — `fb_all[FB_TOTAL]` (1536 B) przeniesione z `.bss` na stertę
  jądra przez `gui_call(16,…)`, z kontrolą wyniku. Wszystkie odczyty `fb_all` są
  ograniczone przez `fb_count`/`fb_total` (linie 678/1050/1360/1720), więc brak
  zerowania po alokacji nie zmienia zachowania.
* `Makefile` — `build/.profile` zawiera teraz **także** `GUI_APP_CFLAGS` i
  `SHELL_CFLAGS`, nie tylko `CFLAGS`. Bez tego zmiana flagi nie unieważniała
  obiektów i powstawał „mixed build”: `DESKTOP.APP` nowy, `SYSINFO.APP` dalej
  z adresowaniem absolutnym (bramka PIC to wykrywała, ale nikt jej nie uruchamiał).
* `Makefile` — `check-gui-apps` ma receptę `python3 tools/check_gui_apps.py`,
  jest w `make check` **i** jest zależnością `all`, więc każdy build profilu TOS=1
  pilnuje niezmiennika PIC.

**Weryfikacja**

| co | wynik |
|---|---|
| `make TOS=1 -j4 all` (po `rm build/.profile`) | EXIT 0, zero `relocation truncated` |
| `desktop_wm.o` | 1264 × `R_68K_PC16`, **0 × `R_68K_32`** |
| `app_math.o` | 2 × PC16, 0 × 32 |
| zapas zasięgu w `DESKTOP.APP` (28 952 B) | dane `%pc`: 2057 B (luz **30 710**), najdłuższy branch `.w`: 20 106 B (luz **12 661**) |
| `python3 tools/check_gui_apps.py` | zielona dla wszystkich 10 aplikacji GUI |
| `make TOS=1 check` | EXIT 0 |
| Hatari: pulpit wstaje, `kdbg_desk_rejok > 0`, SETTINGS działa | patrz §3 |

## 2. Widoczność awarii (krok 2) — i błąd, który dzięki temu wyszedł na jaw

* `system/kernel_main.c` — `kconsole_quiet(0)` na końcu bring-upu (wcześniej quiet
  zostawało włączone na zawsze i **wszystkie** późniejsze komunikaty jądra, w tym
  zakończenie procesu, były tłumione).
* `system/kernel_main.c` — wynik `kernel_spawn_user_async` dla `DESKTOP.APP` nie jest
  już odrzucany; dodany nadzór nad procesem pulpitu (`kernel_user_take_completion`)
  i czytelny komunikat na konsoli tekstowej: `DESKTOP.APP DID NOT START (code n)`
  albo `DESKTOP.APP FAILED [PID n] FAULT k` + `TEXT SHELL ACTIVE…`, z zamazaniem
  planszy startowej (`kgfx_fill_rect` na pełny ekran + `kvideo_flush`; `kgfx_clear`
  czyści bufor aplikacji, nie ramkę na ekranie).

**Weryfikacja:** obraz `work/fix-nodesktop.img` (DESKTOP.APP → DESKTOP.APX), Hatari:
czysty ekran + `DESKTOP.APP DID NOT START (code 1)` / `TEXT SHELL ACTIVE…`,
zrzut `nodesktop.png`.

**Skutek uboczny (naprawiony):** po odciszeniu konsoli okazało się, że **każde**
czyste `Pterm()` raportowało `TOS FAULT vec=2` i `TOS RC -2`, bo diagnostyczny odczyt
`rd32(frame+2)` w obsłudze Pterm0/Pterm bus-errorował na nieużytecznym wskaźniku.
`system/tos_prg.c` ma teraz `tos_ptr_plausible()` i czyta `frame`/`usp` tylko gdy
wskaźnik jest sensowny → `TOS RC 0`. To był błąd **wcześniejszy**, wcześniej
niewidoczny.

## 3. SETTINGS i konfiguracja (krok 3)

* `system/desktop_wm.c` — Up/Down na granicy listy **nie zamyka już okna**
  (wcześniej wpadły w `else` jak Escape); Left/Right są dopuszczone w warunku, więc
  przełączanie zakładek strzałkami działa (wcześniej gałąź była nieosiągalna).
* `system/desktop_wm.c` — `desktop_save()` zwraca wynik; komunikat `SAVE FAILED`
  nie jest już nadpisywany „CONFIG SAVED”; przy zapisie na karcie RESOLUTION pojawia
  się `SAVED - TOS OPTIONS NEED RESTART` (jądro czyta TOSTA/TOSTT raz, przy STARCIE).
* `system/desktop_wm.c` — wysokość okna SETTINGS liczona z `SET_ROWS`
  (`setup_window_height()` = 44 + 8·24 + 60 = 296) i podnoszona przy otwarciu;
  wcześniej stałe 256 px powodowało, że „SAVE SETTINGS” i nowe wiersze TOS nachodziły
  na linię statusu i podpowiedź klawiatury.
* `system/kernel_main.c` — bufor `DESKTOP.CFG` 192 B → 512 B (desktop potrafi zapisać
  ~365 B; `kfat_load_system_file` odrzuca cały plik większy od pojemności) + komunikat
  `DESKTOP.CFG: UNREADABLE - TOS OPTIONS DEFAULT`, gdy plik nie da się wczytać.

**Weryfikacja (Hatari, `work/uidrive.py` na `work/fix-desktop.img`):** `ALL CHECKS PASS`
— licznik `kdbg_desk_rejok`, F10 → menu → SETTINGS, samokorygująca nawigacja po
wierszach (test wykrywa podświetlony wiersz z obrazu, bo harness gubi klawisze),
przełączenie obu opcji, zapis i **odczyt z obrazu**: `DESKTOP.CFG` zawiera
`TOSTA=1`, `TOSTT=1`; sześć zrzutów ma różne SHA-1.

## 4. Testy (krok 4)

* `atos2-tools/scenario_tos_module.py` — przepisany: adresy `kdbg_tos_*` **z
  `build/system/kernel.elf`** (`m68k-elf-nm`, koniec ze stałymi, które wskazywały
  w `async_results`), gotowość z `kdbg_loop_calls` (baner powłoki jest zasłonięty
  przez planszę startową), protokół świeżego uruchomienia (licznik przebiegów
  w `0x81C`), asercje (`loads==1`, `adopted==runs`, `fallbacks==0`, brak faultów),
  tryb `--missing`, kody wyjścia 0/1/2.
* `system/tosprobe.c` — program czyści `DONE`, inkrementuje licznik przebiegu i dopiero
  wtedy pisze `START`; stary wynik nie może być wzięty za nowy.
* `work/uidrive.py` — zamiast „UI driven” i pięciu identycznych zrzutów: gotowość
  z jądra, nazwane klawisze (`keydown`/`keyup`), weryfikacja każdego naciśnięcia,
  asercje na ekranie i na obrazie, kod wyjścia ≠ 0 przy porażce.

**Weryfikacja:** `scenario_tos_module.py --missing`/normalny: `ALL CHECKS PASS`
(`loads=1`, `adopted=2`, `fallbacks=0`, `kdbg_uf_count=0`, `module_screen=0x1FDB20`);
`scenario_tos_accept.py`: `TOS LAUNCH ACCEPTANCE: PASS` z `TOS RC 0`;
`uidrive.py`: `ALL CHECKS PASS`.

## 5. Okno TOS w WM (krok 4.5) i stan persony (krok 5)

* `system/kernel_wm.c` + `include/kernel_wm.h` — nowe `kernel_wm_close_window(pid,handle)`:
  `kernel_wm_release_surface()` nie woła już `kernel_wm_close_pid()`, które przy
  każdym zakończeniu programu TOS zamykało **wszystkie** okna procesu-launchera
  (okno plików, SETTINGS, kosz…), bo okno TOS rejestruje się pod pidem launchera.
* `system/kernel_wm.c` — `wm_window_req[].set` jest zwalniane przy zamykaniu okna
  i przy zwalnianiu powierzchni. Wcześniej slot (z 6) wyciekał przy każdym przebiegu
  TOS; po wyczerpaniu `wm_window_rect()` zwracało 0 i okno rejestrowało się jako
  pełnoekranowe tło (`flags=7`, `wm_background_canvas=1`) → trwale czarny pulpit.
* `system/tos_prg.c` — uchwyty okien TOS w osobnym zakresie `0x100+`
  (`TOS_WM_HANDLE_BASE`), zamiast kolidować z uchwytami pulpitu 2/3/5/6/7.
* `system/tos_prg.c` — `tos_composite_present()`: po konwersji pasma zgłasza
  `kernel_wm_app_dirty()`, komponuje **i prezentuje** (`kvideo_flush()`). Pętla główna
  jądra jest zablokowana przez cały czas trwania programu TOS, więc sama kompozycja
  zostawiała gotową ramkę w buforze WM i nic jej nie pokazywało — okno bywało
  widoczne dopiero po Vsync. XBIOS 37 nie robi już pełnej konwersji i pełnego
  recompose na każdą ramkę (korzysta ze ścieżki pasmowej).
* `system/tos_prg.c` — okno centrowane na **realnym** ekranie (`kvideo_mode_*`),
  tytuł okna z nazwy programu (`kernel_wm_set_title`).
* `system/tos_prg.c` — `tos_window_enabled` ustawiane **po** walidacjach (koniec
  wycieku „tryb okna” po nieudanym przebiegu) i zerowane na ścieżce `-0x40`;
  `p_env` jest zwalniane (wcześniej wyciek 4 B + nagłówek na każdy przebieg);
  paleta motywu jest odtwarzana przy zamknięciu okna (zakres klienta 236..251
  pokrywa się z rampą motywu — komentarz „236..251 is free” był błędny);
  `tos_poll_input()` ma blokadę reentrancji (program na IPL 5 bywa wyprzedzany przez
  Timer C — komentarz twierdził, że nie).
* `system/kernel_module.c` — TOSC publikowane **raz** (`static tos_cfg_published`);
  wcześniej po nieudanej pierwszej próbie każde kolejne `kernel_tos_compat_ensure`
  kończyło się na `-110` (duplikat usługi) i retry był niemożliwy.

## 6. Jak to powtórzyć

```bash
cd "ATOS 2.1"
rm -f build/.profile && make TOS=1 -j4 all      # wymuszona przebudowa (patrz .profile)
make TOS=1 check
python3 tools/check_gui_apps.py

# obrazy testowe (kopie! oryginałów nie ruszamy)
cp work/atos-tos1.img work/fix-desktop.img
python3 tools/install_boot_sys.py work/fix-desktop.img build/system/KERNEL.SYS
python3 tools/install_boot_sys.py work/fix-desktop.img build/system/DESKTOP.APP --directory SYSTEM/APP
ATOS_UI_IMG="$PWD/work/fix-desktop.img" python3 work/uidrive.py

ATOS_MOD_IMG="$PWD/work/fix-module.img" python3 atos2-tools/scenario_tos_module.py
ATOS_MOD_IMG="$PWD/work/fix-module.img" python3 atos2-tools/scenario_tos_module.py --missing
ATOS_IMG="$PWD/work/fix-module.img" python3 atos2-tools/scenario_tos_accept.py
```

Uwaga: obrazy testowe (`work/fix-desktop.img`, `work/fix-nodesktop.img`,
`work/fix-module.img`) to kopie po ~768 MB — można je usunąć.

## 7. Czego świadomie nie zmieniono

* **VDI nie jest przekierowane na bufor programu TOS** (`tos_vdi_set_surface` celuje
  w żywy framebuffer): programy rysujące przez VDI nadal omijają okno. Zmiana
  dotyka kontraktu VDI/Line-A i wymaga własnego testu — do zrobienia w kroku 5.
* **`source_capacity=65536`** przy blitach TOS jest stałą, więc kontrola pojemności
  nie chroni bufora podanego przez program przez XBIOS Setscreen (dla buforów
  własnych jest poprawna).
* **Zagnieżdżony Pexec** nadal nie zapisuje/odtwarza stanu okna (osobny temat).
* **Zakres palety 236..251** nadal dzielony z rampą motywu — odtwarzamy motyw po
  zamknięciu okna, ale w trakcie działania programu pasek pulpitu ma jego kolory.
* **Hatari**: harness gubi/duplikuje klawisze (to była przyczyna flaky testu SETTINGS);
  test jest teraz samokorygujący, ale przy bardzo dużych stratach klawiszy nadal
  zakończy się FAIL-em — świadomie, zamiast udawać sukces.


## 8. Demo — zrzuty w `zrzuty/`

| plik | co pokazuje |
|---|---|
| `settings-flow.gif` | pulpit → F10 (menu OPTIONS) → SETTINGS → `LOAD TOS AT START` → `PREFER TT-RAM` → zapis (klatki 01–06) |
| `01-pulpit.png` | pulpit ATOS po naprawie (ikony, pasek, tapeta) |
| `03-settings.png` | okno SETTINGS z poprawionym układem (nic nie zasłania `SAVE SETTINGS`) i dwoma opcjami TOS |
| `06-zapisane.png` | po zapisie: `SAVED - TOS OPTIONS NEED RESTART`, oba pola zaznaczone |
| `07-brak-desktop-app.png` | obraz bez `DESKTOP.APP`: `DESKTOP.APP DID NOT START (code 1)` + `TEXT SHELL ACTIVE…`, plansza startowa zmazana |
| `08-tos-rc0.png` | `run TOSPROBE.PRG` w profilu tekstowym: `TOSPROBE: ALL PASS` i **`TOS RC 0`** (przed naprawą `TOS FAULT vec=2` / `TOS RC -2`) |
| `tos-window.gif`, `09-okno-tos-na-pulpicie.png` | **program TOS w oknie na pulpicie ATOS**: tytuł `TOSCOL.PRG`, okno wyśrodkowane, treść (16 słupków + ramka) skomponowana na żywo, pulpit pod spodem nienaruszony |

Jak powstały: `01`–`06` z `work/uidrive.py` (finalny build), `07` z obrazu
`work/fix-nodesktop.img`, `08` z `work/fix-module.img`, `09`/`tos-window.gif` z obrazu
`work/fix-demo.img`.

**Zastrzeżenie do `09`/`tos-window.gif`:** wejście `FILE → RUN…` pulpitu jest
obsługiwane **tylko myszą**, a ten harness nie potrafi ustawić pozycji kursora
(`hatari-event mousemove` jest ignorowane — sprawdzone), więc okno wywołano
jednorazowym autostartem `TOSCOL.PRG` z pętli głównej w **kopii roboczej** źródeł
(`/tmp/atos-demo-build`, hook oznaczony `DEMO ONLY`), zbudowanej z tych samych źródeł
co produkt. W samym produkcie okno otwiera się normalnie po dwukliku ikony lub przez
`FILE → RUN…` — tej ścieżki nie dało się tu wyklikać.

## 9. Poprawki z sesji „uruchom TOSCOL / TOSMONO zabija kursor”

Zgłoszenie użytkownika: *„t oscol nie mogę uruchomić, a tosmono wyłącza nasz kursor
myszy całkiem”*. Reprodukcja w Hatari (wstrzyknięty dwuklik + pomiar `kdbg_loop_calls`,
`kgfx_cursor_visible()`) dała trzy odrębne fakty:

1. **Kursor nie jest wyłączany** — powiększony kadr 40×40 px pokazuje strzałkę
   narysowaną (XOR) na ikonie; w pełnym zrzucie ginęła wizualnie na tle ikony.
2. **Program TOS blokuje pętlę główną jądra na czas swojego działania** (zmierzone:
   `kdbg_loop_calls` 588 → 588 przez ~15 s dla `TOSMONO.PRG`). Przez ten czas pulpit
   i kursor stoją — stąd wrażenie „kursor wyłączony”. To architektura osobowości
   (program TOS działa synchronicznie), ale da się ją złagodzić.
3. **Uruchamianie z ikon zależało wyłącznie od dwukliku i trafienia w kafel**, a
   nieudany start nie mówił nic. Log wskazań kursora z sesji użytkownika pokazał
   kliknięcia na `y=161–166`, gdy kafel ikony TOSCOL zaczyna się na `y=164`.

Wprowadzone zmiany (wszystkie w produkcie, nie w kopii demo):

* `system/desktop_wm.c`
  * **Uruchamianie ikon z klawiatury**: strzałki (Left/Up, Right/Down) chodzą po
    liście ikon, **Enter** uruchamia zaznaczoną przez ten sam `icon_activate()`,
    którego używa dwuklik. Podczas ruchu pokazywana jest nazwa ikony.
  * Zaznaczona ikona jest teraz **podświetlana także w trybie kolorowym**
    (`draw_icon()` patrzył tylko na `hover_icon`, więc klawiaturowe zaznaczenie było
    niewidoczne) i **odświeżane są oba kafle** (stary i nowy — bez tego zostawał
    „duch” poprzedniego podświetlenia).
  * **Komunikat o wyniku startu** (`show_notice()`): `NAZWA TOS RC n`, `STARTED`,
    a przy nieudanym starcie natywnym nazwa pliku zamiast anonimowego `START FAILED`.
    Komunikat jest przemijający (~2,4 s), więc nie wraca stary „status”.
  * **Fallback**: jeśli start natywny zawiedzie, ścieżka jest ponawiana w osobowości
    TOS (loader sam sprawdza nagłówek `0x601A`). To czyni klasę błędów „.PRG, który
    nie został rozpoznany po rozszerzeniu” niemożliwą do trafienia przez użytkownika.
* `system/toscolour.c`, `system/tosmono.c`, `system/tosmed.c`, `system/tosdemo.c`
  * **Hold przerywany klawiszem**: demonstracyjna pętla sprawdza `Cconis` (GEMDOS
    0x0B) i przy klawiszu konsumuje go (`Crawcin`, 0x07) i kończy program, więc
    użytkownik nie jest zablokowany na cały czas trwania okna.

Weryfikacja (`/tmp/atos-kbd-test.py`, obraz `work/fix-kbd.img`, bez żadnych hooków):
strzałki przechodzą kolejno `My Atari → TRASH → TERMINAL → COMMANDER → TOSMONO →
TOSCOL` (potwierdzone odczytem komunikatów z kadrów), Enter uruchamia program TOS
(`kdbg_tos_adopted_module` rośnie), a dowolny klawisz natychmiast oddaje pulpit.

### 9a. Przyczyna źródłowa „TOSCOL nie mogę uruchomić" — ucięty `LAUNCH.CFG`

`launch_load()` czytał plik konfiguracyjny **jednym** `read()` do bufora 384 B:

```c
char buf[384];
r=gui_call(3,(uint32_t)fd,(uint32_t)(sizeof(buf)-1u),(void*)buf,0);
```

Tymczasem `\SYSTEM\LAUNCH.CFG` w obrazie ma **510 B**. Odczyt kończył się w połowie
ostatniej linii, więc do listy trafił wpis o ścieżce `\SYSTEM` (zamiast
`\SYSTEM\TEST\TOSCOL.PRG`). Skutki dokładnie takie, jak zgłosił użytkownik:

* ikona `TOSCOL` **jest** rysowana (etykieta i ścieżka są osobnymi polami),
* ale ścieżka nie kończy się na `.PRG`, więc start szedł **ścieżką natywną**,
* spawn `.APP`-a z pliku `.PRG` kończył się porażką, a komunikat mówił tylko
  `START FAILED` — bez nazwy pliku i bez kodu.

Po drodze ginęły też dalsze wpisy (`TOSDEMO`, `SYSINFO`, `TASKMGR`…), więc na pulpicie
były tylko trzy skróty zamiast pełnej listy — co widać na zrzutach „przed”.

Naprawa:

* `read_all()` w `system/desktop_wm.c` — czyta **w pętli do EOF**, zawsze kończy NUL-em;
  bufor 1024 B jest **jeden i na stercie** (`gui_call(16,…)`), bo powiększenie BSS
  o 3 × 1 KB przesunęło symbole poza zasięg `R_68K_PC16` i linker odmówił budowy
  (ten sam limit, który był przyczyną crashu pulpitu opisanego w §1).
* Ten sam helper obsługuje `ICONS.CFG` i `DESKTOP.CFG` (wcześniej identyczny,
  jednorazowy odczyt).
* `tos_rc_notice()` — komunikat zawiera **nazwę pliku**: `TOSCOL.PRG TOS RC -2`
  zamiast `START FAILED`, więc każda przyszła awaria startu jest czytelna.
* Fallback: nieudany start natywny jest ponawiany w osobowości TOS.

Weryfikacja końcowa (`/tmp/atos-kbd-test.py`, `work/fix-kbd.img`, obraz produktowy):

```
PASS  keyboard selection moved (screens changed)
PASS  Enter launched TOSCOL (module adopted)  adopted=1
PASS  TOSCOL window is on screen
PASS  desktop loop is alive again after a key  loop 733 -> 882
PASS  Enter launched TOSMONO (module adopted)  adopted=2
PASS  TOSMONO hold is interruptible (loop resumes < 4 s)
ALL CHECKS PASS
```

Dowody: `zrzuty/10-toscol-z-klawiatury.png` (okno `TOSCOL.PRG` otwarte Enterem),
`zrzuty/11-zaznaczona-ikona-toscol.png`, animacja `zrzuty/keyboard-launch.gif`.


## 10. Kursor zawsze widoczny + wskaźnik żyje w trakcie programu TOS

Domknięcie obu zgłoszeń u źródła („nasz kursor myszy wyłączony”, „pulpit zamarza”).

### 10a. Kursor rysowany obwódką, nie XOR-em

`kgfx_cursor_show()` w trybie 8bpp **xorował bit 0** koloru pod kursorem. Na
jednolitym tle to działa, ale nad 48×48 kolorowymi ikonami dawało
pomarańczowo-turkusową plamę — zobacz `zrzuty/13-kursor-stary-xor.png` — a przy
odpowiednim doborze barw potrafiło zniknąć zupełnie.

Teraz kursor pisze **realne indeksy palety**: wnętrze strzałki kolorem `paper`
(indeks 1), a jedynka piksela wokół kształtu kolorem `ink` (indeks 0) — patrz
`zrzuty/12-kursor-nowy-na-ikonie.png`. Ten sam kształt, brak XOR-a, czytelny na
jasnym, ciemnym i ditherowanym tle; para 0/1 to konwencja motywu („0 ink, 1
paper”), a osobowość TOS nadaje indeksowi 0 to samo znaczenie, więc kursor
przetrwa także paletę programu TOS.

### 10b. Wskaźnik i obraz żyją, gdy działa program TOS

Pomiar przed naprawą: podczas przebiegu programu TOS `kdbg_loop_calls` stał
(np. 588 → 588 przez ~15 s), bo `kernel_tos_run_program()` blokuje pętlę główną
jądra — a razem z nią `ksessions_pump()`, czyli jedyne miejsce, które podawało
delty IKBD do `mouse_x/mouse_y`, oraz `tos_composite_work()`. Skutek: kursor stał
w miejscu przez cały przebieg programu.

Trzy zmiany:

1. `system/kernel_sessions.c` — `ksessions_pointer_track(dx,dy)` (bezpieczne z
   przerwania, **tylko pozycja**, żadne zdarzenie nie jest kolejkowane) oraz
   `ksessions_pointer_take_dirty()`.
2. `system/tos_prg.c` — ramkowanie TOS **przechwytuje wreszcie delty myszy**
   (wcześniej dwa bajty po nagłówku `0xF8` były po prostu odrzucane) i podaje je
   do `ksessions_pointer_track()`; a `tos_composite_work()` przy ruchu wskaźnika
   wymusza przejście przez dzielnik `/5` i — gdy program nie zmienił ani jednego
   pasma — wykonuje **prezent tylko dla kursora** (`kernel_wm_compose_now()` +
   `kvideo_flush()`). Bez tego skan pasm zwracał `y0<0`, nie było prezentu i
   kursor zostawał tam, gdzie ostatni raz go narysowano.
3. `system/toscolour.c`, `tosmono.c`, `tosmed.c`, `tosdemo.c` — hold pompuje
   `Vsync` (XBIOS 37), więc trap-stub prezentuje klatkę także wtedy, gdy program
   nic już nie rysuje.

Pomiar po naprawie (obraz `work/fix-cursor.img`, kopia robocza z hookami
`DEMO ONLY`, które symulują ruch myszy — harness nie umie ruszyć kursora hosta):

```
loop=400 (pętla główna NADAL zablokowana)   adopted=1   tiki≈150/s
px: 700 -> 0        (wskaźnik jądra śledzi ruch w trakcie programu)
wmdump: 799/0       (kompozytor widzi świeżą pozycję)
kadry: kursor sunie 90 -> 62 -> 32 -> 2 px na y=188
```

Dowody: `zrzuty/kursor-podczas-tos.gif` (kursor jedzie po pulpicie, gdy program
TOS jest na ekranie), `12-kursor-nowy-na-ikonie.png`, `13-kursor-stary-xor.png`.

### 10c. Co nadal blokuje pulpit

Pętla główna jądra jest zablokowana przez cały przebieg programu TOS (to
architektura osobowości: program działa synchronicznie). Dzięki 10b **kursor i
obraz** podążają za myszą, ale ikony, zegar i inne aplikacje stoją do końca
programu. Pełne rozwiązanie wymagałoby oddawania sterowania (kontynuacja
`kernel_tos_run_program`) — świadomie poza zakresem tej iteracji.


## 11. Rozdzielenie palety: program TOS nie przemalowuje już pulpitu

Zgłoszenie: *„TOSMONO uruchomiłem i kursora brak. Zmienia kolor też górny pasek po
odpaleniu, a nie powinien się zmieniać. TOSCOLOR też zmienia kolor paska i blokuje
kursor.”* — wszystkie trzy objawy miały jedną przyczynę: **wspólną pulę palety**.

Mapa palety była taka:

```
0..19      kolory systemowe/NC
20..235    paleta ikon (ICON_PAL_BASE 20, ICON_PAL_COLORS 216)
236..251   rampa motywu (THEME_RAMP_BASE = 20+216)   <-- KOLIZJA
236..251   paleta klienta TOS (TOS_CLIENT_PALETTE_BASE)
```

czyli **dokładnie ten sam zakres**: pasek pulpitu rysował się z `ramp[0..3]`
(indeksy 236..239), a program TOS wpisywał w 236..251 swoje 16 kolorów. Pomiar
przed naprawą (piksel w pasku, `work/fix-pal.img`, `TOSMONO`/`TOSCOL` z pulpitu):

```
baseline               bar = (154,154,170)   <- motyw STEEL
podczas TOSMONO        bar = (16,16,16)      <- paleta mono programu
podczas TOSCOL         bar = (255,0,0)       <- czerwony programu
```

Do tego mono-program **pinnił wpisy 0 i 15** (globalne, nie swoje!) na czerni/bieli,
co zabierało kolor wpisowi `ink` (0) i wpisowi motywu (15) — a kursor właśnie na
wpisach 0/1 polega.

Naprawa — rozdzielenie zakresów na stałe (paleta ma dokładnie 256 wpisów, więc
rampa musiała się skurczyć; pasek i tak używał tylko czterech wpisów):

| zakres | zawartość |
|---|---|
| 0..19 | kolory systemowe/NC (bez zmian) |
| 20..235 | paleta ikon (bez zmian — zero ryzyka dla ikon) |
| **236..251** | **paleta klienta TOS (16 kolorów)** |
| **252..255** | **rampa UI (KGFX_THEME_RAMP = 4)** |

Zmiany w kodzie:

* `include/kernel_theme.h` — `KGFX_THEME_RAMP` 16 → 4, opis mapy palety i zakazu nakładania się zakresów.
* `system/kernel_theme.c` — `THEME_RAMP_BASE` = **252**; tablice ramp obu motywów przycięte do czterech stopni, których pasek używał (`STEEL`: `0xaab,0x99a,0x889,0x778`) → obraz paska jest **identyczny jak wcześniej**.
* `system/desktop_wm.c` — bez zmian: pętla gradientu miała już `tb<4u`.
* `system/tos_prg.c` — mono (rez 2) nie dotyka już wpisów 0/15: blit mono używa
  `236` (paper) i `237` (ink), a `tos_apply_palette()` pinni właśnie te dwa wpisy.
  Cała osobowość TOS pisze teraz **wyłącznie** w 236..251.

Weryfikacja po naprawie (te same obrazy i scenariusz):

```
baseline            bar = (154,154,170)
TOSMONO (cały czas) bar = (154,154,170)   adopted=1
TOSCOL  (cały czas) bar = (154,154,170)   adopted=2
po zamknięciu       bar = (154,154,170)
```

Treść okien bez zmian: `TOSMONO.PRG` rysuje szachownicę 1-bit (przez nowe wpisy
236/237), `TOSCOL.PRG` 16 słupków + ramkę (przez 236..251); kursor widoczny w obu.

Ruch kursora **w trakcie** programu (kopia robocza symulująca mysz, bo harness nie
rusza kursorem hosta):

```
TOSCOL:  loop=400 (pętla główna zablokowana), kursor 90 -> 62 -> 32 -> 2 px na y=188
TOSMONO: loop=399, wmdump idzie za wskaźnikiem, kursor 542 -> 512 -> ... -> 212 px na y=300
```

Dowody: `zrzuty/kursor-przy-tosmono.gif`, `zrzuty/kursor-podczas-tos.gif`.


## 12. Mysz w TOS: prawdziwa przyczyna i naprawa (korekta §10b)

### 12a. Czym się pomyliłem

W §10b uznałem, że wystarczy przechwycić bajty myszy w ramkowaniu TOS i podać je do
`ksessions_pointer_track()`. **To było błędne i mój test tego nie wychwycił**: kopia
robocza wywoływała `ksessions_pointer_track(-2,0)` *wprost z ticku*, czyli badała
rysowanie przesuwanego kursora, a nie odbiór myszy. Nie była to weryfikacja ścieżki
wejścia.

Prawdziwa przyczyna (potwierdzona niezależnie przez audyt):
`kernel_tos_set_input(kernel_user_terminal_getchar)` podaje osobowości TOS
**interfejs klawiatury** — ta funkcja zwraca wyłącznie znaki (kolejka 0). Ruch myszy
INPUT.SYS odkłada do **kolejki 1**, którą normalnie odbiera `ksessions_pump()`
w pętli pulpitu. Program TOS działa synchronicznie i tę pętlę blokuje, więc nikt
kolejki 1 nie odbierał: współrzędne nie zmieniały się wcale, a pakiet zalegał i
zostawał zastosowany dopiero po powrocie do pulpitu. Dlatego `kdbg_tos_mouse_packets`
audytu pozostawał **0**, mimo że `raw head/tail` rósł — bajty były parsowane, ale
nie trafiały do wskaźnika. Moja gałąź szukająca nagłówka `0xF8` w strumieniu
klawiatury nie mogła zadziałać nigdy.

### 12b. Naprawa

* `system/tos_prg.c` — usunięte przechwytywanie surowych bajtów myszy
  (parsowanie IKBD zostaje wyłącznie w INPUT.SYS).
* Dodane `tos_poll_mouse()`: podczas aktywnego TOS odbiera kolejkę myszy przez
  **istniejące `kernel_raw_mouse_event()`** (ograniczona pętla, ochrona przed
  ponownym wejściem, **tylko pozycja i stan przycisków** — z przerwania nie
  uruchamiamy żadnej akcji pulpitu), podaje `dx/dy` do
  `ksessions_pointer_track()` i zapamiętuje przyciski (`tos_window_mouse_buttons()`
  dla przyszłego `evnt_mouse`/`graf_mouse`). Wywoływane z `tos_poll_input()`, czyli
  z ticku Timer C i ze ścieżki trapów.
* Klawiatura: `tos_poll_input()` korzysta teraz z **`kernel_user_keyboard_event()`**
  (typ / skan / modyfikatory / znak) z tej samej kolejki 0, zamiast zgadywać, czy
  bajt ze strumienia terminala jest skanem, czy znakiem ASCII. Fallback na stary
  hook został tylko dla danych, które widzi wyłącznie terminal (profil tekstowy).
  Zdarzenia zwolnienia klawisza nie trafiają już do kolejki znaków, a klawisze
  funkcyjne aktualizują stan bez produkowania znaku.
* Liczniki diagnostyczne `kdbg_tos_mouse_packets` / `kdbg_tos_mouse_dxdy` liczą
  teraz **realnie odebrane** pakiety (wcześniej były martwe).

### 12c. Weryfikacja metodą audytu (bez dotykania `mouse_x/mouse_y`)

Kopia robocza wysyła pakiet myszy przez **kolejkę zdarzeń jądra**
(`event->send(1, KERNEL_INPUT_MOUSE_PACKET, …)`), czyli dokładnie tam, gdzie
odkłada go INPUT.SYS; test czyta tylko współrzędne i liczniki.

```
PASS  desktop packet moved the pointer (control)   +20,+10 -> (620,18)
PASS  TOSCOL:  pointer moved while the program ran  x 0..799 y 0..405, pakiety 5..163
PASS  TOSMONO: pointer moved while the program ran  x 100..799 y 25..405, pakiety 87..163
PASS  no deferred packet after the runs (pointer stays put)
dxdy of the last TOS packet: 0x00140005 = dx=+20 dy=+5   (znak zachowany)
```

Przez cały czas `kdbg_loop_calls` był **zamrożony** (pętla główna zablokowana), a
program **nie kończył się od ruchu myszy** — spełnione kryteria audytu. Nadal do
zrobienia po stronie użytkownika: próba **fizyczną myszą w oknie Hatari**.


## 13. Widgety okna TOS: przyciski, belka, zamykanie

Zgłoszenie: *„mysz działa, ale nieaktywne są widżety, np. do zamknięcia okna TOSCOL — nie
da się zamknąć okna, przesunąć okna programu, ani usunąć task managera, żeby wymusić
zamknięcie. Po dłuższej chwili TOSCOL sam się zamknął.”*

### 13a. Przyczyna

Kursor i kliknięcia **dochodziły**, ale chrom okna czyta **stan przycisku sesji**
(`ksessions_mouse_buttons()`), który aktualizował wyłącznie `handle_mouse()` — a ten
działa w pętli pulpitu, zablokowanej przez synchroniczny program TOS. WM widział więc
przycisk **stale zwolniony**: belka, „X", zoom, minimalizacja, przeciąganie i task lista
były całkowicie martwe. Dodatkowo kompozytor odtwarzał klatkę tylko przy zmianie pasm
albo ruchu kursora, więc **klik z nieruchomym kursorem nie generował klatki**, w której
WM mógłby ten klik zobaczyć.

### 13b. Co zostało zrobione

* `system/kernel_sessions.c` — `ksessions_pointer_button(down,x,y)`: aktualizuje
  **tylko stan sesji**, który czyta kompozytor (przyciski + zapisana pozycja wciśnięcia),
  **bez kolejek do aplikacji** (żaden klik nie może „wyskoczyć" po powrocie programu),
  i oznacza wskaźnik jako brudny, żeby wymusić klatkę — ale kompozycja nadal odbywa się
  w ścieżce trapów, **nigdy w przerwaniu**.
* `system/tos_prg.c` — `tos_poll_mouse()` przekazuje krawędzie przycisków do WM.
* `system/kernel_wm.c` — WM ufa **zapisanemu** wciśnięciu (`press_pos`), a nie tylko
  stanowi przycisku w danej klatce; dzięki temu krótki klik nie ginie między klatkami
  (podczas TOS klatki są rzadkie), a zalegająca flaga nie zostaje na zawsze.
* `system/kernel_wm.c` + `system/tos_prg.c` — **zamknięcie okna TOS kończy program**:
  `window_close()` woła `kernel_tos_window_closed()` (żądanie wyjścia + flaga
  przemalowania), a osobowość rozwija program jak przy `Pterm0` przy najbliższym
  wywołaniu OS. Bez tego zostawał program bez okna, piszący w zwolnioną powierzchnię.
  Ekran jest przy tym przemalowany (w ścieżce trapów), bo kompozytor zdążył już usunąć okno.

### 13c. Co jest potwierdzone, a co nie

Potwierdzone pomiarami (`/tmp/atos-wm-test.py`, pakiety myszy przez kolejkę zdarzeń):
klik **dociera do kompozytora** — `wm_dbg_press` rośnie, a przy wciśnięciu na belce
`wm_dbg_drag_start` rośnie (przeciąganie zostaje rozpoczęte).

**Potwierdzone ręcznie (użytkownik, fizyczna mysz w Hatari): zamykanie okna „X"
działa** — program TOS jest kończony i wraca pulpit.

Nie udało się natomiast **w pełni odtworzyć gestu zamknięcia** w symulacji: moje
wstrzyknięte wciśnięcie/zwolnienie w przycisk „X" nie zamykało okna w teście.
To była wada **testu**, nie produktu: Wiadomo,
że przeszkadza co najmniej jedno: WM ma ochronę „świeżo otwarte okno" liczoną
w **klatkach** (`kdbg_frames - wm_open_frame < 20`), a podczas programu TOS klatek jest
mało, więc ochrona trwa znacznie dłużej niż 0,4 s. To zostaje do potwierdzenia
**fizyczną myszą** w oknie Hatari; jeśli „X" nadal nie zadziała, następny krok to
instrumentacja `wm_hit_part()` i ścieżki zwolnienia przycisku w `kernel_wm.c`.

### 13d. Ograniczenie, które zostaje

Program TOS, który nie wywołuje OS (czysta pętla bez Vsync/GEMDOS), nie daje punktu,
w którym osobowość może odświeżyć ekran albo zakończyć program — dla niego chrom okna
pozostanie martwy do końca przebiegu. Dema TOS pompują Vsync, więc ich dotyczy to
w niewielkim stopniu, ale prawdziwe programy w czystym spinie nadal będą blokować pulpit.
To ten sam temat co punkt E audytu (integracja z planistą), świadomie poza zakresem.


## 14. VDI w oknie TOS — implementacja + stan weryfikacji (nieudanej)

### 14a. Co zostało zrobione (build zielony)

* `system/tos_vdi.c` — `tos_vdi_get_surface()` (odczyt bieżącego celu VDI, żeby
  można go było przywrócić).
* `system/tos_prg.c` — `tos_vdi_follow_screen()` / `tos_vdi_release_screen()`:
  gdy otwarte jest okno programu TOS w trybie **mono**, VDI rysuje do **prywatnego
  ekranu programu** (`tos_screen`) — tego samego bufora, z którego okno blituje —
  zamiast do framebuffera jądra. Wywoływane przy otwarciu okna, przy zmianie trybu
  (`Setscreen`/`Setrez`) i odwracane przy zamknięciu okna. Uzasadnienie: VDI w tym
  drzewie jest klasyczne, **planarne 1 bpp**, więc nie może pisać do 8-bpp
  powierzchni klienta; pisanie do ekranu programu daje ten sam efekt (dane trafiają
  do okna), a pulpit zostaje czysty.
* `system/tosvdi.c` + reguła `tosvdi` w Makefile — nowe demo **`TOSVDI.PRG`**
  (983 B, `0x601A`): rysuje wyłącznie przez GEM VDI (`trap #2`, `d0=0x73`,
  `d1=&VDIPB`): `v_opnvwk`, `v_clrwk`, `vsl_type/color/width`, 8 × `v_pline`,
  prostokąt, 4 × `v_bar`, `vst_height`, `v_gtext` („VDI IN WINDOW”).

### 14b. Weryfikacja: NIEUDANA — i czego się dowiedziałem

Uruchomienie `TOSVDI.PRG` (podłożone pod istniejący wpis `TOSDEMO.PRG`, żeby ominąć
problem z nowymi wpisami) kończy się **`TOS RC -2`** — także przy starcie z pulpitu
(klawiatura: strzałki + Enter), i to samo w kopii roboczej sterowanej z jądra.
W komunikacie pulpitu widać dosłownie: `TOSDEMO.PRG TOS RC -2`.

Co udało się wykluczyć:

* plik **jest** w obrazie: wpis katalogowy `TOSDEMO PRG`, `size=983`, `cluster=129`,
  `attr=0x20`, a `FAT1[129]=FAT2[129]=0xFFFF` (obie kopie FAT **identyczne**);
* niezależny czytnik FAT (własny, w Pythonie) czyta ten plik w całości, nagłówek
  `0x601A` na miejscu;
* inne programy z tego samego katalogu startują z pulpitu bez problemu
  (`TOSCOL`, `TOSDEMO` oryginalny) — czyli to nie jest ogólna awaria pulpitu,
  tylko coś związanego z **tą konkretną binarką lub tą konkretną instalacją**;
* `-2` w `kernel_tos_run_program()` (system/tos_prg.c:2374) oznacza dokładnie
  `kfat_stat_path() != FAT_FILE_FOUND || size < 28` — więc ATOS **nie znajduje**
  pliku, mimo że jest on na dysku i czytelny.

To jest sprzeczność, której **nie rozstrzygnąłem** — i dlatego nie twierdzę, że
naprawa VDI działa.

### 14c. Następne kroki (kolejność, gdy wrócimy do tematu)

1. **Izolacja winowajcy**: podmienić `TOSDEMO.PRG` na jego *oryginalną* binarkę
   (`build/system/TOSDEMO.PRG`) w świeżym obrazie i uruchomić z pulpitu. Jeśli
   zadziała → winna jest moja binarka `TOSVDI.PRG` (np. coś w nagłówku/relokacjach
   z `mk_tosprg.py` — 53 relokacje); jeśli nie → winna jest ścieżka instalacji
   (instalator).
2. **Ślad loadera**: osobowość ma `tos_trace("LD:enter")` / `"LD:stat"` — włączyć
   trace i odczytać go w sesji tekstowej (konsola pulpitu jest zasłonięta), żeby
   zobaczyć, czy `LD:enter` w ogóle pada i jaka ścieżka zawodzi.
3. **Nowe wpisy katalogowe**: `tools/install_boot_sys.py` zapisuje wpis nowego pliku
   w slocie znacznika końca katalogu (0x00) i **nie odtwarza terminatora**; próba
   dopisania terminatora „na sztywno" uszkodziła obraz (wpis wypadł na końcu
   klastra katalogu), więc zmiana została **wycofana**. Poprawna naprawa: wybierać
   najpierw slot skasowany (0xE5), a przy braku miejsca **dołożyć klaster** do
   łańcucha katalogu. Do tego czasu nowe pliki trzeba instalować pod istniejącą
   nazwą (nadpisanie) — co w tej sesji działało wielokrotnie.


## 15. VDI w oknie TOS — naprawa wg audytu z 7 października (korekta §14)

Audyt użytkownika znalazł prawdziwą przyczynę awarii testu z §14 i listę
niezależnych błędów VDI. Wszystko poniżej zostało wprowadzone **w kolejności
zaleconej przez audyt**; wynik kontrolny: rysunek i **napis** w oknie, `fault_pc=0`.

### 15a. Korekta mojej błędnej diagnozy z §14

`-2` **nie oznaczało** „nie znaleziono pliku": `kernel_tos_run_program()` zwraca
`-2` przy błędnym stat, ale `asf_tos_fault()` ustawia `tos_exit_code=-vector`,
więc **Bus Error też daje -2**. Dodatkowo `kdbg_uf_count` liczy wyjątki natywnych
procesów, więc zero nie wykluczało wyjątku w osobowości TOS. Plik był na dysku i
był uruchamiany — awaria następowała przy pierwszym wywołaniu VDI.

### 15b. Generator PRG (`tools/mk_tosprg.py`) — P1

Generator sklejał surowe `.text` i `.data`, **gubiąc przerwę wyrównania ELF**, i
wpisywał `len(text)` jako `tlen` — a adresy symboli i miejsca relokacji pochodziły
z ELF. Dla `tosvdi.elf` brakowało 2 bajtów, więc pięć wskaźników VDIPB było
czytanych z przesunięciem o pół słowa.

Teraz granice segmentów liczone są **z adresów sekcji**: `tlen = .data_vma -
.text_vma` (razem z `.rodata` i paddingiem), `dlen` sięga do `.bss_vma`, a obraz
jest składany pod adresy ELF. Kontrola: `TOSVDI.PRG` ma `tlen=880, dlen=20,
bss=1056`, dokładnie jak w pomiarze audytu.

### 15c. VDI — P1/P2 wg listy

* **Inicjalizacja**: `tos_vdi_init()` jest wreszcie wołane (w `kernel_main.c`,
  przy instalacji pozostałych hooków TOS). Wcześniej nie było go w binarium
  (linker usuwał nieużywany kod), a `ws[]` startowało zerowe.
* **Clear ≠ close**: `v_clrwk` (3) tylko czyści; `v_clswk`/`v_clsvwk` (2/101)
  zamyka workstation. Wcześniej czyszczenie oznaczało slot jako zamknięty.
* **Uchwyty**: `v_opnvwk` zwraca `contrl[6] = slot+1`, a rysowanie używa
  `vdi_ws()` (slot bieżący) zamiast na sztywno `ws[0]`.
* **DEV_TAB[13]** to **liczba kolorów** (mono = 2), nie „colours-1".
* **Tekst**: `v_gtext` czyta znaki jako **słowa 16-bitowe** i iteruje do
  `contrl[3]` (kontrakt EmuTOS `output_text(CONTRL[3], INTIN)`), bez wymagania
  zera kończącego — wcześniej pierwszy zerowy starszy bajt 'V' kończył rysowanie.
* **`vst_height`**: wysokość z `ptsin[1]` (nie różnica `ptsin[1]-ptsin[3]`),
  pełny `ptsout` komórki znaku.
* **`vsl_width`**: szerokość z `ptsin[0]` (nie z `intin[0]`).
* **Clipping i tryb zapisu**: `vs_clip` i `vswr_mode` są wreszcie **stosowane** —
  `vdi_fill` (a więc linie i wypełnienia) przycina do prostokąta clipu.
* **MFDB / kopiowanie bitmap**: `vrt_cpyfm` ma numer **121** (110 to `vr_trnfm`);
  wskaźniki MFDB czytane są z `contrl[7..8]` i `contrl[9..10]` (nie z `addrin`,
  które w VDI jest zawsze zerowe), `fd_addr` z offsetu **+0**, a kopiowanie
  realizuje **pełne ROP 0..15** i jest bezpieczne przy nachodzących obszarach
  (kopia wstecz, gdy cel leży za źródłem). `vrt_cpyfm` interpretuje `intin[0]`
  jako tryb, a `intin[1]/[2]` jako kolory (wcześniej było to zamienione).
* **Odświeżanie**: trap #2 (GEM AES/VDI) wywołuje teraz `tos_composite_work`,
  tak jak trap #1/#13/#14 — programy rysujące wyłącznie przez VDI nie muszą już
  same pompować Vsync.

### 15d. Wynik kontrolny (obraz produktowy, `TOSVDI.PRG` pod nazwą `TOSDEMO.PRG`)

```
fault_pc = 0            (przed naprawą: Bus Error w asf_tos_aesvdi+0x5c)
tos_vdi_fill_calls = 6889
tos_vdi_gtext_i = 1
```

Na zrzucie `zrzuty/14-vdi-w-oknie.png`: wachlarz linii, prostokąt, cztery słupki
**oraz napis „VDI IN WINDOW"** — wszystko wewnątrz okna TOS, pulpit bez zmian.

### 15f. Domknięcie zgodności MONO (druga iteracja)

* **Szerokość i rodzaj linii są stosowane**: `vdi_line` rysuje `line_width`
  równoległych linii (przesunięcie wzdłuż osi mniejszej) i realizuje wzory
  `line_type` 1–5 (ciągła, kreska, kropka, kreska-kropka, kreska-kropka-kropka).
* **`vsf_interior`/`vsf_style` dla obszarów**: `v_bar` i `v_fillarea` rysują
  desenie 2–7 (poziomy, pionowy, dwie przekątne, krzyż, krzyż ukośny) przez
  maskę liczbową; styl 1 pozostaje szybkim wypełnianiem. Linie i tekst nadal
  używają solidnego `vdi_fill`, bo styl wnętrza ich nie dotyczy.
* **`vr_trnfm` (110)**: dla mono (jedna płaszczyzna) standard i device to ten sam
  raster słowo-w-wierszu, więc transformata kopiuje wiersze i **publikuje
  geometrię** w docelowym MFDB (`w/h/wdwidth/stand/nplanes`). Interleave 2/4
  płaszczyzn należy do pracy nad kolorem.
* **Drugi cykl workstation w demie**: `v_clswk` → `v_opnvwk` (nowy slot) →
  rysowanie przez **nowy** uchwyt. Na zrzucie widać osobny słupek narysowany już
  po ponownym otwarciu — dowód, że księgowość uchwytów działa.

Wynik kontrolny: `fault_pc=0`, `tos_vdi_fill_calls=29925`, `tos_vdi_gtext_i=1`,
a na `zrzuty/14-vdi-w-oknie.png` widać: pogrubione linie, linię kreskowaną,
prostokąt, cztery słupki, **słupek z deseniem**, napis oraz słupek z drugiego
uchwytu.

### 15e. Co zostaje z listy audytu

* **Kolor (2/4 plany)** — przekierowanie VDI nadal obejmuje tylko mono
  (`vdi_fb()` deklaruje `bpp=1`, `vdib_set` ustawia jeden bit); kolor wymaga
  opisu celu z liczbą płaszczyzn i zapisu indeksu koloru do N płaszczyzn.
* `vr_trnfm` dla 2/4 płaszczyzn (interleave standard↔device).
* Ścieżka MFDB/ROP (109/121) jest zaimplementowana, ale **nie ma jeszcze
  własnego testu** — audyt słusznie wymaga osobnej próby bitmap.
* `tools/install_boot_sys.py`: nowy wpis katalogowy zjada znacznik końca
  katalogu (poprawka „na sztywno" uszkodziła obraz i została wycofana).
* Powtarzane otwieranie/zamykanie workstation i przywracanie celu rysowania —
  logika `follow/release` istnieje, brakuje dedykowanego testu.
* `tools/install_boot_sys.py`: nowy wpis katalogowy zjada znacznik końca
  katalogu; poprawka „na sztywno" uszkodziła obraz i została wycofana — do
  zrobienia porządnie (slot 0xE5, ewentualnie dołożenie klastra katalogu).


## 16. Test prawdziwym TOS SysInfo 8.33 + naprawa instalatora

### 16a. Plik i instalacja

SysInfo 8.33 (`SYSINF833.PRG`, 58 648 B, `0x601A`, `tlen=58616`, pakowany
HWAM-SOFT + PFX) został wgrany na obraz testowy jako `\SYSTEM\TEST\SYSINFO.PRG`.

Żeby to było możliwe, naprawiony został **`tools/install_boot_sys.py`**: przy
nowym pliku wybiera teraz najpierw slot skasowany (`0xE5`), potem slot znacznika
końca katalogu (`0x00`) i **gwarantuje terminator** po nowym wpisie — a jeśli slot
jest ostatni w klastrze katalogu, **dokłada nowy klaster** do łańcucha katalogu.
Poprzednia próba (zapis terminatora „w przód" bez tego warunku) wyszła poza klaster
i uszkodziła wolumin; tamta zmiana została wycofana, ta jest sprawdzona: wpis
`SYSINFO PRG` (cluster, size) i terminator po nim są poprawne, a obraz się bootuje.

### 16b. „Znacznik mono" — nie jest potrzebny

Persona ma `tos_mode = 2u` **domyślnie** (`system/tos_prg.c`), czyli `Getrez()`
zwraca 2 = 640x400 mono, a konfiguracja Hatari w testach to `--monitor mono`.
SysInfo zapyta zatem o tryb i dostanie **mono**, tak jak chciałeś — dodatkowego
znacznika nie trzeba ustawiać.

### 16c. Wynik: SysInfo startuje i pada w rozpakowywaniu

```
entry             = 0x04000100   (program wczytany do TT-RAM i wywołany)
kernel_fault_vector = 2          (Bus Error)
kernel_fault_pc   = 0x0400F726   (offset 0xF626 od entry - w środku rozpakowywacza)
kernel_fault_d0   = 0
tos_aes_log_n     = 0            (zero wywołań AES)
tos_vdi_fill_calls= 0            (zero rysowania VDI)
tos_exit_code     = -2           (fault raportowany jako -2, jak w §15a)
```

To jest **kontrolowany, powtarzalny punkt startowy**: program nie „nie odpala",
tylko wykłada się w swoim dekompresorze, zanim wykona jakiekolwiek GEM.

### 16d. Następny krok (do zrobienia)

1. Porównać **TPA i stronę bazową** (`p_hitpa`, `p_fuschr`, `p_bbase`, `p_dbase`,
   flagi TT-RAM) dla tej samej binarki na starszym obrazie `commander-v9/work/work.img`,
   gdzie SysInfo działał (handoff: „rozpakowuje się (HWAM+PFX), dostaje TPA
   w TT-RAM, żyje jako task"). Różnica w `tos_tpa_*`/TT-RAM wskaże regresję.
2. Sprawdzić, czy dekoder nie potrzebuje **ciągłego bloku** TT-RAM większego niż
   `p_hitpa - p_fuschr` (offset faultu ≈ 0xF626 sugeruje wyjście poza blok).
3. Dopiero potem wracać do ścieżki GEM/VDI (liczniki `aes`/`vdi` są zerowe, więc
   VDI nie ma tu jeszcze nic do rzeczy).


## 17. Line-A / A_INIT, instalator FAT16, zegar _hz_200 (audyt 2026-10-07 #2)

### 17a. Korekta diagnozy z §16: to nie był dekompresor

SysInfo nie padał w rozpakowywaniu. Rozpakował się, wywołał **A_INIT** i dostał
**zerowy wskaźnik**, a następnie odczytał przez niego licznik ukrycia myszy
(`TST.W -598(A2)`, A2=0 → adres 0xFFFFFDAA) → Bus Error. `asf_tos_linea_op()`
miał `if(!b)return 0;` **przed** obsługą `A_INIT`, więc A_INIT z A0=0 zwracał 0;
dodatkowo `tos_linea_init()` nie było w ogóle wołane w ścieżce startu i linker je
usuwał. Wniosku „dekompresor" nie wolno było wyciągać z PC ani z zerowych
liczników AES/VDI — program korzysta wcześniej z Line-A.

**Naprawa:** `A_INIT` (op 0) obsługiwane jest **przed** walidacją i odczytem
bloku wejściowego (A0 może być zerem), a strona Line-A jest **zakładana, gdy jej
nie ma** — bez zerowania jej przy każdym kolejnym A_INIT (program nie traci
stanu). Strona zawiera już `M_HID_CT` pod -0x256 = -598, czyli dokładnie to, co
czyta SysInfo.

### 17b. Wynik kontrolny (obraz produktowy, ten sam SysInfo 8.33)

| pomiar | przed | po |
|---|---|---|
| `kernel_fault_vector` | 2 (Bus Error) | **0** |
| `kernel_linea_base` | 0 | **0x000B86B8** |
| `tos_linea_opcount[0]` | 1 (z zerem) | **1, baza poprawna** |
| `tos_aes_log_n` | 0 | **4544 → 7504 (rośnie w pętli zdarzeń)** |
| `tos_vdi_fill_calls` | 0 | **3385** (= 0xD39 z audytu) |
| `rc` | -2 | **0** |
| `kdbg_tos_divzero` | — | **0** |

Zrzut `zrzuty/15-sysinfo-w-oknie.png`: okno SysInfo z jego UI (pasek menu, wiersze
tekstu, dolny pasek statusu). **Tekst jest lustrzany** — to defekt z punktu D
audytu (orientacja glifów), a nie skutek Line-A.

### 17c. Instalator FAT16 — testy graniczne audytu przechodzą

Twoje cztery przypadki (`test_installer.py`) na naprawionym skrypcie:

```
last_slot                rc=0 new_visible=True  ghost=False next=0x3
stale_deleted_after_end  rc=0 new_visible=True  ghost=False next=0xffff
stale_live_after_end     rc=0 new_visible=True  ghost=False next=0xffff
full                     rc=0 new_visible=True  ghost=False next=0x3
```

Poprawki: skan logiczny kończy się na **pierwszym 0x00** (slot 0xE5 wybierany
tylko przed nim), nowy wpis trafia **w** slot terminatora, a gdy to ostatni slot
klastra — katalog przedłuża się o **następny klaster** (z terminatorze na jego
początku; istniejący link nie jest zastępowany nowym). Pełny łańcuch katalogu
jest rozszerzany o klaster; root FAT16 ma stały rozmiar i tylko wolne sloty.
Żywy wpis za terminatorze nie jest już zerowany (zniknął „duch").

### 17d. Zegar _hz_200 i maskowanie wyjątku

`_hz_200` (0x4BA) było publikowane tylko przy wywołaniach usług, więc podczas
pętli liczącej w `Supexec` stało w miejscu i dzielenie przez zero kończyło
benchmark. Teraz zegar jest odświeżany z **tikera Timer C** (działa także wtedy,
gdy gość liczy), a przy pierwszym wywołaniu publikowana jest wartość początkowa.
Wynik: `kdbg_tos_divzero = 0`, `_hz_200` przyrasta w trakcie pracy programu.

Niezależnie od tego **żadne dzielenie przez zero nie jest już maskowane po
cichu**: wyjątek nadal nie zabija programu, ale jest liczony
(`kdbg_tos_divzero`) i raportowany, więc `rc = 0` nie może być brany za dowód
poprawności.

### 17e. Co zostaje (kolejność z audytu)

* **D.1** Spis faktycznej ścieżki rysowania — odczyt `tos_vdi_opcount`,
  `tos_aes_opcount` i `tos_linea_opcount` po uruchomieniu SysInfo (pierwsza próba
  nie wróciła z harnessu, powtarzam). Dopiero potem orientacja glifów i pas
  śmieci; nie zgadywać, którą drogą SysInfo rysuje.
* **ABI Line-A**: A1 ma wskazywać tablicę nagłówków fontów systemowych, A2
  tablicę funkcji Line-A; teraz A1 to surowa bitmapa, A2 tablica wskaźników do
  bitmap.
* `Mshrink(basepage,size)` obsługuje tylko `tos_heap`, odrzuca TPA; `Super()`
  zwraca chwilowy SP kernela bez trybu/przejścia.
* **C**: uruchomienie SysInfo normalnie z pulpitu, dwa razy — mysz, klawiatura,
  zamknięcie, powrót do pulpitu.


## 18. Testy w WIDOCZNYM Hatari + spis ścieżki rysowania (2026-10-07 #3)

### 18a. Dlaczego nie było widać okien (błąd mojego harnessu, nie projektu)

`atos2-tools/atos2_auto.py` otwierał FIFO sterujący **blokująco** (`open(fifo,"w")`)
i pisał do niego bez ochrony. Gdy Hatari zniknął (zamknięte okno, koniec
emulacji), zapis blokował się **na zawsze** — supervisor wisiał zamiast
zrestartować okno, a użytkownik nie widział żadnego okna. Teraz open jest
nieblokujący z ograniczonym ponawianiem, a zapis po zniknięciu instancji zgłasza
`BrokenPipeError` (sprawdzone: po zabiciu procesu zapis zgłasza wyjątek zamiast
wisieć). Dodatkowo: **widoczne przebiegi trzeba puszczać w pierwszym planie** —
próba w tle kończyła się ubiorem procesu razem z komendą; przebieg w pierwszym
planie wytrzymał 200 s i okno żyło.

### 18b. Spis faktycznej ścieżki rysowania SysInfo (pełny, wszystkie opcody)

```
AES: 79=2244, 25(evnt_multi)=848, 114=74, 78=13, 11=8, ... , 100=2, 104=2
VDI: 11(v_bar)=8, 13=3, 22=3, 38=3, 106=3, 12(vst_height)=2, 100(v_opnvwk)=1, 129=1
     v_gtext(8) = 0        <- ZERO tekstu przez VDI
     109/121 (MFDB)  = 0   <- ZERO blitów bitmap
Line-A: tylko A_INIT(0) = 1
tos_vdi_fill_calls = 3385  (~3377 to piksele glifów)
```

Wniosek: SysInfo rysuje tekst **ścieżką AES obiektów** (`aes_obj_draw` →
`tos_vdi_text()`), a nie przez `v_gtext` ani przez blity MFDB. Ta sama funkcja
rysuje **tytuł okna**, który na zrzutach jest **czytelny** — czyli lustro nie
leży w samym rendererze glifów, tylko w tym, **jak** obiekty są rysowane
(justowanie/kolumna współrzędnych) albo w trybie, który program uważa za aktywny.
Dlatego kolejnym krokiem jest eksperyment A/B (wymuszone mono vs kolor + małe
demo AES z tekstem obiektu), a nie zmiana renderera.

### 18c. Widoczny przebieg: co widać w licznikach

```
linea_base = 0x000B86B8    A_INIT zaliczone, brak Bus Error
tos_aes_log_n = 24160      SysInfo zyje w petli zdarzen
tos_vdi_fill_calls = 3385  rysunek wykonany (zgadza sie z audytem)
kdbg_tos_divzero = 1       JEDNO dzielenie przez zero - teraz WIDOCZNE
kernel_fault_vector = 5    wektor ostatniego wyjatku
```

Dzielenie przez zero nadal występuje (mimo zegara z Timer C) — ale zgodnie z
zaleceniem audytu **nie jest już maskowane po cichu**: jest liczone i raportowane,
więc `rc=0` nie udaje sukcesu. Do zbadania osobno (instrukcja i dzielnik).

### 18d. Obraz do testów z pulpitu (punkt C audytu)

`work/sysinfo-desktop.img`: obraz produktowy + **finalny kernel z wszystkimi
naprawami** + `DESKTOP.APP` + dema + `\SYSTEM\TEST\SYSINFO.PRG` (58 648 B) oraz
wpis w `\SYSTEM\LAUNCH.CFG` (`SYSINFO=\SYSTEM\TEST\SYSINFO.PRG`), więc SysInfo
8.33 da się uruchomić **z pulpitu myszą**, a nie tylko diagnostycznym autostartem.
Wpis nowego pliku w FAT ma poprawny terminator (sprawdzone w obrazie).


## 19. Orientacja tekstu, maska rastra, Supexec i nazwa SysTos (2026-10-07 #4)

### 19a. Wdrożone poprawki z audytu orientacji

1. **Wiersze glifu** (`system/tos_vdi.c`): wszystkie trzy ścieżki tekstu
   (`v_justified`, `v_gtext`, `tos_vdi_text` używany przez AES) korzystają teraz
   z **jednego** `vdi_glyph()`, który rysuje `row` od góry komórki:
   `y-h+row` zamiast `y-row-1`. Stara forma sadzała wiersz 0 fontu na dole znaku
   i odbijała każdą literę pionowo. Dowód: `zrzuty/20-systos-tekst-poprawny.png`
   — „SYSINFO COPYRIGHT (C) 1992/93 HWAM, VERSION 8.36", „MEMORY POINTERS:",
   „PRESS RIGHT MOUSE-BUTTON OR SPACE FOR MENU !" są czytelne (wcześniej cały
   tekst był lustrzany).
2. **Maska rastra** (`include/st_raster.h`): getter i setter liczą
   `mask = 0x8000u >> (x & 15u)`. Stara forma `15-(x&15)` dawała przy x=0 maskę
   0x0001, czyli odbijała obraz w każdej grupie 16 pikseli. Test
   `tests/test_st_raster.c` ma teraz **surowe wzorce bajtów**, niezależne od tych
   funkcji: `80 00` dla x=0, `00 01` dla x=15, `80 01` dla obu, przejście na
   kolejne słowo przy x=16 oraz odczyt słowa `80 01` wpisanego ręcznie.
   `make check-st-raster` → **ALL PASS**.
3. **Kompozytor nic nie odwraca** — sprawdzone: `wm_app_flip = 0` i nowa ścieżka
   WM go nie używa; poprawka należy do ścieżki rysowania, nie do blitu okna.

### 19b. Dzielenie przez zero: przyczyna znaleziona, maskowanie usunięte

* `asf_tos_supexec()` (XBIOS Supexec) uruchamia funkcję gościa, ale persona trzyma
  **IPL 7**, a tik kernela ATOS jest **pollingowy w pętli głównej** — więc
  `_hz_200` nie może się zmienić, gdy gość liczy. SysInfo dzieli przez zerowy
  przyrost → wyjątek 5.
* Zmierzone warianty (Hatari):
  - przerwania **IPL 0** na czas Supexec → przeciekł nieoczekiwany **autowektor
    poziomu 4** i kernel stanął: `ATOS SYSTEM EXCEPTION, VECTOR 0000001C,
    PC 0040E8E (TT-RAM), SYSTEM HALTED`;
  - przerwania **IPL 5** (tylko MFP/NMI) → haltu nie ma, ale `_hz_200` **nadal
    stoi** (= tik nie jest przerwaniowy).
* Wniosek: produkt zostaje przy IPL 7 (bezpiecznie), a **wyjątek 5 nie jest już
  maskowany** — bieg kończy się kodem `-5`, co widać na pulpicie jako
  `SYSTOS.PRG TOS RC -5` (`zrzuty/21-systos-rc-minus5.png`).
* Następny krok: przerwaniowy tik **MFP Timer C** (wektor + ISR aktualizujący
  `tos_mfp_ticks`/`_hz_200`), bezpieczny w trakcie synchronicznego biegu TOS.

### 19c. Nazwa SysTos + dlaczego ikony nie było

* Program instalowany jest jako `\SYSTEM\TEST\SYSTOS.PRG` (58 648 B) z wpisem
  `SYSTOS=\SYSTEM\TEST\SYSTOS.PRG` w `\SYSTEM\LAUNCH.CFG`.
* **`desktop_wm.c: #define LBAR_MAX 8u`** — launcher przyjmuje maksymalnie
  **8 wpisów**; dziewiąty (`SYSTOS`, wcześniej `SYSINFO8`) był po cichu
  odrzucany, dlatego ikony nie było. Teraz jest 8 wpisów, a `SYSTOS` zastępuje
  ATOS-owe `SYSINFO`; ikona jest widoczna na pulpicie
  (`zrzuty/22-pulpit-ikon-8-limit.png`).
* Etykieta **mieszaną wielkością** (`SysTos`) **wywala pulpit**
  (`DESKTOP.APP STOPPED`, shell tekstowy) — używamy `SYSTOS`. To osobny błąd
  launchera do naprawy (etykiety powinny być niezależne od wielkości liter).


## 20. Wydajność prezentacji TOS: punkty 1–3 z pomiarem (2026-10-07 #5)

### 20a. Co zmieniono

1. **Kursor odseparowany od treści** (`tos_composite_work()`): ruch myszy ma teraz
   własną ścieżkę (compose+flush tylko uszkodzonych prostokątów kursora) i **nie**
   wymusza już `divider=4` ani skanu ekranu; licznik `kdbg_perf_cursor_only`.
2. **Śledzenie zmian na poziomie wierszy**: `vdi_fill()` zgłasza prostokąty przez
   `tos_note_dirty_rect()` (bitmapa 512 wierszy = 64 B), a `tos_composite_work()`
   prezentuje **osobne ciągi zmienionych wierszy** zamiast jednego pasa od
   pierwszego do ostatniego. Skan zapasowy (dla programów piszących do pamięci)
   jest **adaptacyjny**: gdy nic się nie zmieniło, skanuje jeden pas na przebieg
   (3 200 B), a gdy program aktywnie rysuje — wszystko (32 KB), więc nie gubi
   zmian.
3. **Szybki blit mono** (`kgfx_blit_mono()`): zamiast przesunięcia i testu bitu na
   każdy piksel jest tablica rozwinięcia 256×8 (przebudowywana tylko przy zmianie
   ink/paper) i dwa zapisy 32-bitowe na 8 pikseli, gdy cel jest wyrównany.

### 20b. Pomiar (te same warunki: 68030/32 MHz, mono, bez fast-forward)

Metoda: identyczny program (SysInfo 8.36), identyczny obraz, **dokładnie 2000
wstrzykniętych ruchów kursora**, odczyt liczników jądra (`kernel_user_hires_clock`).

| metryka (2000 ruchów) | przed | po | zmiana |
|---|---|---|---|
| cykle razem | 8 059 | 4 420 | **−45%** |
| cykle na ruch | 16,1 | **8,8** | −45% |
| `recompose` | 1 595 | 669 | −58% |
| `cursor` | 328 | 243 | −26% |
| `deco` | 2 173 | 1 135 | −48% |
| skan przy odświeżeniu treści | ~32 000 B | 3 200 B (pas rotacyjny) lub 0 (wiersze z VDI) | do 10× mniej |
| wykonane prezenty | — | `presents`/`cursor_only` (nowe liczniki) | — |
| piksele konwersji | — | `kdbg_perf_blit_pixels` (nowy licznik) | — |

### 20c. Uczciwe zastrzeżenie

W 20-sekundowym oknie, gdy SysInfo tylko czekał na wejście, `kdbg_perf_dirty_rows`
pozostał 0, a zmiany znalazł skan zapasowy — czyli **ścieżka VDI nie jest jedyną,
którą ten program rysuje późniejsze strony** (wcześniejszy spis pokazał v_bar=8 i
3 385 wypełnień w fazie startowej). Trzeba to jeszcze potwierdzić licznikiem
wywołań hooka; dziś zmierzone zyski pochodzą z punktów 1 i 3, a dla programów
rysujących przez VDI (`v_bar`, `v_fillarea`) działa też oznaczenie wierszy.

### 20d. Do zrobienia (punkty 4–6 audytu)

* usunąć okresowe `kernel_wm_damage_all()` z `asf_tos_aesvdi()`,
* harmonogram prezentacji: po obniżeniu kosztu cel 25 Hz dla zmieniającej się
  treści + niezależna, częstsza obsługa kursora,
* aktywne oczekiwanie `evnt_multi()`/`tos_getkey()` i odpowiedzialność za
  `kvideo_flush()` (`kernel_wm_frame_tick()` już flushuje).


## 21. Punkty 4–6 + domknięcie punktu 2, pomiar i pokaz w Hatari (2026-10-07 #6)

### 21a. Zmiany

* **(4)** `asf_tos_aesvdi()` nie woła już `kernel_wm_damage_all()` przy powrotach
  z `evnt_multi` — to było ostatnie okresowe wymuszanie całego pulpitu. Zostało
  samo `kernel_wm_frame_tick()`, które napędza kompozytor.
* **(5)** ogranicznik treści `/5` (~10 Hz) → `/2` (~25 Hz), po obniżeniu kosztu
  z punktów 1–3; kursor nadal ma własną, niezależną ścieżkę.
* **(6)** usunięte **zdublowane** `kvideo_flush()` po `kernel_wm_compose_now()`
  (flush robi już `kernel_wm_frame_tick()`).
* **(2, domknięcie)** skan zapasowy startuje dopiero po **4 kolejnych
  odświeżeniach bez ani jednego wiersza oznaczonego przez VDI**. Program GEM
  (który sam zgłasza, co rysuje) nie płaci za skan w ogóle; program piszący
  bezpośrednio do pamięci dostaje wykrywanie zmian w ~200 ms.

### 21b. Pomiar (2000 wstrzykniętych ruchów kursora, ten sam program i obraz)

| metryka | baseline | po 1–3 | **final (1–6)** |
|---|---|---|---|
| cykle razem | 8 059 | 4 420 | 4 727 |
| **cykle / ruch** | 16,1 | 8,8 | **9,5** |
| `recompose` | 1 595 | 669 | **482** |
| `cursor` | 328 | 243 | 295 |
| **skan** | ~32 000 B na odświeżenie | 7 072 000 B ✗ | **38 400 B (3 wywołania)** |
| `dirty_rows` (z VDI) | — | 0 | **44** (hook działa) |
| `presents` / `cursor_only` | — | 6 / 26 | 9 / 28 |

Skok „po 1–3" miał próg skanu ustawiony tak, że przy aktywnie rysującym programie
skan wracał do pełnych 32 KB; finalna wersja (próg 4 pustych odświeżeń) zbiła to
do 38 KB w tym samym obciążeniu — **185× mniej skanowania** niż w wersji
pośredniej i ~10× mniej niż baseline na odświeżenie.

### 21c. Pokaz w widocznym Hatari

Sesja 116 s, okno widoczne (`--window`, 1:1, bez `--grab`), monitor mono,
68030/32 MHz, bez fast-forward: pulpit ATOS → SysTos uruchomiony **z ikony**
klawiaturą (8 × Right + Enter) → program działa w oknie. Odczyt liczników:

```
linea_base = 0x0BB7A2      A_INIT zaliczone, brak Bus Error
aes         = 4 430 -> 205 991     petla zdarzen GEM zyje
fill        = 3 385        rysunek wykonany
hook VDI    = 3 385        kazde wypelnienie zglosilo swoje wiersze
dirty_rows  = 44           prezentowane osobne ciagi wierszy
presents    = 68           odswiezen tresci
div0 = 0,  vec = 0         zadnego wyjatku przez caly przebieg
```


## 22. Regresja czcionek z punktu 3 — znalazł użytkownik, naprawiona

### 22a. Objaw i przyczyna

Po wdrożeniu szybkiego blitu mono (`kgfx_blit_mono()`, punkt 3) tekst SysInfo
wrócił jako **pocięty i lustrzany w grupach czterech bajtów**
(`zrzuty/23-czcionki-ZEPSUTE-little-endian.png`). Użytkownik zauważył to okiem,
zanim zrobił to jakikolwiek test.

Przyczyna: nowa ścieżka składała słowo 32-bitowe **little-endian**

```c
d32[k] = (uint32_t)e[0] | ((uint32_t)e[1]<<8) | ((uint32_t)e[2]<<16) | ((uint32_t)e[3]<<24);
```

a 68000 jest **big-endian**, więc w pamięci pierwszy bajt grupy lądował na końcu:

```c
d32[k] = ((uint32_t)e[0]<<24) | ((uint32_t)e[1]<<16) | ((uint32_t)e[2]<<8) | (uint32_t)e[3];
```

### 22b. Dowód naprawy

`zrzuty/24-czcionki-NAPRAWIONE.png` — ten sam SysInfo, ten sam obraz, ten sam
kernel poza tą jedną linią: `SYSINFO COPYRIGHT (C) 1992/93 HWAM, VERSION 8.36`,
`TYPE : ST`, `PROCESSOR : 68030`, `SCREENMEMORY : 32000 Bytes`,
`GEM VERSION : 2.60`, `AES VERSION : 3.30`, `TOTAL ST-RAM : 13312 Kbytes`,
`MOUSE POSITION : 520,0`, `ADDRESS DISPLAY MODE: 24 BIT`.

### 22c. Wniosek na przyszłość

Pomiar wydajności **nie sprawdza poprawności obrazu** — liczby były „dobre", a
obraz zepsuty. Do szybkiego blitu trzeba testu jednostkowego na **kolejność
bajtów** (np. źródłowy bajt 0x80 z ink=0xFF/paper=0x00 ma dać w pamięci
`FF 00 00 00`), a nie tylko porównania kosztu; do czasu jego dodania jedynym
wiarygodnym sprawdzeniem pozostaje zrzut ekranu.


## 23. Zgłoszenie "boot 1.5x wolniejszy" — pomiary i dwie realne poprawki

### 23a. Czego nie udało się odtworzyć

Zmierzone czasy (ten sam obraz bazowy, Hatari TF536/68030/32 MHz, bez
fast-forward, od startu emulatora do `kdbg_desk_rejok=1`):

| kernel | headless | okno |
|---|---|---|
| pierwotny (sprzed sesji, `/tmp/atos-old`) | 19,4 s / 17,9 s | — |
| baseline (przed optymalizacją 1–3) | 18,0 s | 20,2 s |
| obecny (po 1–6) | 17,9 s / 18,5 s / 19,4 s | 18,7 s |

Rozrzut między przebiegami tego samego kernela to ~1,5 s, więc **systematycznego
spowolnienia 1,5× nie ma** w tych konfiguracjach — obecny build bywa nawet
najszybszy. `_hz_200` (0x4BA) jest zerowe w obu wersjach przez cały boot, więc
licznik, na który patrzy użytkownik, nie jest tym, które zmieniałem.

### 23b. Dwie realne wady obciążenia znalezione przy tej okazji (naprawione)

1. **Stałe tempo odświeżania treści.** Po punkcie 5 ogranicznik był na sztywno
   `/2` (~25 Hz) **także wtedy, gdy nic się nie zmieniało** — jądro robiło 2,5×
   więcej kompozycji niż potrzebne, gdy program tylko siedział w pętli zdarzeń.
   Teraz jest **adaptacyjny**: `/2` gdy wiersze się zmieniają, `/8` gdy nie.
2. **Tik kompozytora przy każdym `evnt_multi`.** Usuwając `kernel_wm_damage_all()`
   zostawiłem `kernel_wm_frame_tick()` bez bramki, czyli 16× częściej niż
   poprzednie „co 16. wywołanie". Teraz jest bramkowany (co 4.), a częstotliwość
   wynika z licznika, nie z liczby wywołań AES.

Oba obniżają obciążenie hosta bez pogorszenia reakcji (kursor nadal ma własną,
natychmiastową ścieżkę).

### 23c. Do ustalenia z użytkownikiem

Pomiar nie potwierdza spowolnienia, więc potrzebny jest dokładny scenariusz:
który obraz i który kernel był porównywany, oraz czy zegar BIOS-u liczył wolniej
w tym samym oknie/systemie. Przygotowane obrazy do porównania „na dwa okna":
`work/pristine.img` (kernel sprzed sesji) i `work/adaptive.img` (obecny).


## 24. Zegar SysInfo odświeżał się skokowo co ~2 s — znaleziona przyczyna i naprawa

### 24a. Przyczyna (dwie nakładające się wady, obie moje)

1. **`tos_composite_pending` ustawiał wyłącznie tik Timer C**, a ten nie może
   dojść do głosu, gdy persona trzyma IPL 7. Program, który tylko czeka na
   zdarzenie czasowe AES (zegar SysInfo), nie ustawiał więc flagi i
   `tos_composite_work()` wychodziło na pierwszym `if(!tos_composite_pending)` —
   ekran stał zamrożony, dopóki program nie wykonał wywołania, które flagę
   ustawiało.
2. **Adaptacyjne zejście do `/8`** (punkt 5 z poprzedniej iteracji) dodawało do
   tego opóźnienie: gdy tylko jedna linia się zmieniała, prezentacja czekała do
   8 przebiegów, co dawało właśnie te ~2 s.

### 24b. Naprawa

* `tos_mark_present()` ustawia flagę w **każdym trapcie TOS** (`gemdos`, `bios`,
  `xbios`, `aes/vdi`): program coś zrobił, więc jest co prezentować. Koszt nadal
  ogranicza limiter wewnątrz `tos_composite_work()`.
* Limiter dla treści wrócił do **stałego ~25 Hz** (`/2`), bo wykrywanie zmian
  programów piszących wprost do pamięci musi być równomierne; ograniczony został
  tylko **pełny** skan (co 4. przebieg), a pojedynczy pas to 3,2 KB.
* Kolejność: najpierw sprawdzane są wiersze oznaczone przez VDI
  (`tos_dirty_pending()`), więc zmiana zgłoszona przez API jest prezentowana
  natychmiast, bez czekania na limiter.

### 24c. Dowód

Dwie klatki po ustabilizowaniu strony (`zrzuty/25-zegar-07-55-38.png` i
`zrzuty/26-zegar-07-55-46.png`): `SYSTEM TIME` przesuwa się z **07:55:38** na
**07:55:46**, czyli **+8 s w ~7,2 s czasu rzeczywistego** — zegar chodzi co
sekundę i zgadza się z rzeczywistością. Licznik `kdbg_perf_presents` rośnie
równo o 1 na klatkę (~1,8 s), zamiast stać w miejscu.

### 24d. Wniosek

Zgłoszenie „boot 1.5x wolniejszy" i „zegar odświeża się co 2 s" to najprawdopodobniej
ten sam objaw: zanim flaga prezentacji była ustawiana w trapach, ekran odświeżał
się tylko przy niektórych wywołaniach, więc wszystko *wyglądało* na zwolnione
2×. Pomiar czasu bootu (17,9–19,4 s na trzech kernelach) nie wykazał realnego
spowolnienia — wąskim gardłem była prezentacja, nie szybkość jądra.


## 25. Zegar prezentacji (mój błąd) + blokowy glif + gotowa tablica konwersji

### 25a. Błąd krytyczny: `tos_now_ticks()` (znaleziony przez audyt użytkownika)

Moja wersja liczyła czas z krótkiego odczytu licznika MFP i **dodawała 256 przy
każdym zwykłym zejściu** (`v < last`), więc pojedynczy krok sprzętowy dawał skok
o 255 — czyli **jeden krok przekraczał cały termin prezentacji** (8 jednostek).
Krótki odczyt nie pozwala odtworzyć pominiętych przepełnień, a Timer w projekcie
przeładowuje się wartością 246, nie 256.

**Naprawa:** podstawa czasu to licznik przerwań jądra
(`kernel_timer_ticks`), monotoniczny i nie gubiący przepełnień; okresy wróciły do
jednostek tiku: kursor 1 (50 Hz), treść 2 (25 Hz).

### 25b. Efekt zmierzony (ten sam obraz, ta sama faza)

| metryka | przed | po |
|---|---|---|
| pełne UI SysInfo | ~38,1 s | **~33,7 s** |
| wywołań `vdi_fill` na stronę | 3 385 | **1 142** (−66%) |
| ramki w tym momencie | 2 | **27** |
| wywołania AES po 42 s | **136, zamarznięte** | **8 370** (pętla zdarzeń żyje) |
| stabilnie | — | ~100 ramek, 27 000 wywołań AES / 10 s |

Zepsuty zegar **niweczył wcześniejsze optymalizacje**: terminy prezentacji
zapadały losowo, więc program stał nieruchomo. Po naprawie program realnie
działa, a liczba wywołań VDI spadła dzięki punktowi 25c.

### 25c. Blokowy glif i gotowa tablica konwersji

* `vdi_glyph()` rysuje teraz **spójne odcinki wiersza** (jedno `vdi_fill` na
  odcinek) zamiast jednego wywołania na każdy zapalony piksel — stąd spadek
  3 385 → 1 142 wywołań na stronę.
* `kgfx_blit_mono()` ma tablicę **dwóch gotowych słów 32-bit** na bajt źródłowy
  (kolejność big-endian), więc wyrównana ścieżka to dwa odczyty i dwa zapisy, bez
  przesunięć i OR-ów.

### 25d. Co zostaje z planu (kolejność uzgodniona)

4. Adaptacyjne łączenie zmian (mała lista prostokątów zamiast jednego bounding
   boxa; pełne przerysowanie po progu) + kursor w `tos_damage_take()`.
5. Ograniczenie aktywnego oczekiwania w `evnt_multi` i uzupełnienie `evnt_timer`.
6. Ładowanie: kopiowanie blokowe segmentów, zerowanie tylko używanego ekranu,
   porównanie startu spakowanego HWAM/PFX z rozpakowanym PRG.
7. Wariant gorących funkcji dla 68030 z `-O2` + analiza cache (D-cache obecnie
   wyłączony; włączenie wymaga obsługi MMIO/DMA/samomodyfikującego się kodu).


## 26. Pomiar kliknięcia: klik -> menu (trzy etapy)

Metoda: ten sam obraz i program, prawy przycisk przez `hatari-event rightdown/rightup`,
odczyt liczników jądra co 50 ms, kopia z wznowieniem po wyjątku 5 (żeby program nie
zakończył się w trakcie pomiaru).

| etap | zmierzony czas |
|---|---|
| klik -> dostarczenie zdarzenia do programu | **0,7 s** |
| klik -> pierwszy rysunek menu (gość) | **11,3 s** |
| klik -> pierwsza prezentacja | **12,0 s** |

Rozkład czasu rysowania menu (próba 2):

```
vdi_fill:   5062 wywołań, 7478 jednostek zegara  (~1,5 jedn. = ~1,2 ms na wywołanie)
dyspozytor: 1073 wywołań AES/VDI, 13675 jednostek  (~12,7 jedn. = ~10 ms na wywołanie)
jednostka zegara hires ~= 0,78 ms (10,7 s / 13675)
```

### Wnioski

1. **Menu rysuje NASZ kod, nie SysInfo.** 5062 wypełnień odbywa się wewnątrz
   jednego wywołania AES (objc_draw -> rysowanie obiektów), a cały czas mieści się
   w dyspozytorze (13675 jednostek ≈ 10,7 s). Gość tylko zleca narysowanie.
2. **Pojedyncze `vdi_fill` kosztuje ~1,2 ms** — to o dwa rzędy wielkości za dużo
   jak na zapis kilku pikseli. To jest główny cel optymalizacji.
3. Pozostałe ~45% czasu dyspozytora to narzut chodzenia po drzewie obiektów i
   wywołań pomocniczych (`tos_vdi_text`, `tos_vdi_fill_rect`), też po jednym na
   element.
4. **Dostarczenie kliknięcia 0,7 s** to również nasza strona (stan przycisków
   próbkowany w tikowej ścieżce wejścia) — powinno być kilkadziesiąt ms.

### Następny krok (pomiar wewnątrz `vdi_fill`)

Rozbić te 1,2 ms na trzy części: `vdi_fb()` (wybór celu/pitch/bpp), zgłoszenie
zmiany (`tos_note_dirty_rect` -> `tos_damage_note`) i sam zapis pikseli. Dopiero
to wskaże, którą z nich ciąć; równolegle warto rysować cały napis jednym
zakresem zamiast po odcinku glifu.


## 27. Ekran startowy: logo Ǝ od inicjalizacji do pulpitu

### 27a. Logo

* **ROM/POST** (`ui/post.c`): przywrócone oryginalne ASCII-owe logo odwróconego E
  (moja wcześniejsza podmiana na „Fuji" była błędem w rozpoznaniu znaku).
* **Splash kernela** (`boot_splash()` w `system/kernel_main.c`): rysuje teraz
  **ten sam znak** prostokątami — prawy pion + trzy belki (środkowa krótsza) —
  zamiast trzech „zębów", więc POST i boot pokazują jedno logo.
* **Ikona pulpitu „My Atari"**: podmieniona na `Ǝ` w stylu zestawu ikon
  (`system/ICONS/komputer.png` i `komputer_C.png`, 48x48, paleta bieli/niebieskiego
  z ciemnym obrysem i cieniem 3D jak pozostałe). Oryginał zachowany w
  `AUDYT_2026-10-06_WDROZENIE/ikony/komputer-oryginal.png`.
  **Uwaga:** nie wolno dodawać nowych plików PNG do `system/ICONS/` bez
  regeneracji wpisów w `ICONS.CFG` obrazów — numeracja `ICON_*` się przesuwa i
  wszystkie ikony pulpitu się rozjeżdżają (sprawdzone; dlatego podmiana jest
  „w miejscu").

### 27b. Ekran startowy bez przerysowywania

* `main.c`: normalny boot nie robi już `console_clear()` ani drugiego
  `post_run()` — logo zostaje, zmienia się tylko wiersz statusu.
* `ui/post.c`: `post_boot_progress()` nie czyści ekranu ani nie rysuje logo
  ponownie; podpis i procent zmieniają tylko własne komórki, a odliczanie
  podmienia jedną cyfrę.
* Odliczanie: **2 sekundy** (`AUTO BOOT: 2`), domyślna wartość w configu i
  normalizacja zapisanych 3–5; `delay=0` nadal znaczy „bez czekania".
* Pasek klawiszy: `[F1 SETUP] F2 DIAGNOSTICS F3 HARDWARE F4 BOOT F5 TOS
  F10 SAVE CONTINUE` — zaznaczenie w nawiasach, bo inverse na czarnym tle było
  niewidoczne.


## 28. Logo graficzne (to z końca bootu) już na starcie BIOS-u

Prośba: „to logo z ostatniej fazy ładowania systemu, widoczne dosłownie przez
sekundę, ma być widoczne już przy starcie BIOS-u; znakowe mniej się podoba".

Wykonanie: ekran POST nie rysuje już logo znakami. `ui/post.c` maluje **te same
prostokąty** co `boot_splash()` w kernelu — prawy pion + trzy belki (środkowa
krótsza) — bezpośrednio do framebuffera konsoli ROM-u:

* `console_screen_ptr()` — nowy, dwuliniowy akcesor w `ui/console.c` (generyczny
  sterownik `video_*` nie jest linkowany do ROM-u, a konsola i tak pisze po tym
  samym buforze);
* `post_fill_rect()` — wypełnianie prostokąta w obu trybach ROM-u: **planar mono**
  (Shifter, jeden word na 16 pikseli, maski brzegowe) i **chunky 8 bpp** (karta,
  jeden bajt na piksel).

Dzięki temu znak jest identyczny od pierwszego rysowanego ekranu: BIOS → POST →
splash kernela → ikona „My Atari" na pulpicie. Zrzut:
`zrzuty/31-POST-logo-graficzne-E.png`.


## 29. Logo bez migotania: identyczne piksele od POST do pulpitu

Pytanie: „czy może być cały czas od włączenia po desktop bez migania?"

Zmierzone (zrzuty `32-logo-POST.png` i `33-logo-splash-kernela.png`, bbox białych
pikseli w obszarze logo):

```
POST  (ROM, tekst+grafika): (343,108)-(457,193)  114x85
splash (kernel, grafika)  : (343,108)-(457,193)  114x85   <- te same piksele
```

Przyczyny usunięte:

1. **Różna geometria** — kernel rysował logo w `ly = H/3 - lh/2`, ROM w
   `ly = H/4 - lh/2`, więc znak „przeskakiwał" o ~40 px. Teraz oba liczą tak samo
   (`W/7`, `gh=(gw*3)/4`, `H/4`, grubość `gw/5`), a ROM bierze wysokość z
   aktywnego trybu (`frame_bytes/line_bytes`), nie ze stałej 400.
2. **Zmazywanie logo** — `boot_splash(0)` czyścił cały ekran i rysował znak od
   nowa. Teraz czyści **wokół** prostokąta logo (góra/dół/lewo/prawo), więc
   piksele namalowane przez ROM zostają nietknięte — przejście jest niewidoczne.
3. **Przełączanie trybu** — `kvideo_set_mode_format()` wychodzi wcześnie, gdy
   wymiary i głębia się zgadzają, a ROM uruchamia kartę od razu w trybie
   docelowym (`hw/vofa_rom.c`), więc nie ma drugiego przełączenia trybu.

Co pozostaje (poza logo): ~2-3 s od włączenia, zanim ROM zaprogramuje obraz
(karta ET4000 musi zostać zainicjalizowana — wcześniej nie ma czego pokazać),
oraz podmiana tekstu POST (testy, odliczanie, pasek F) na pasek postępu przy
przekazaniu sterowania. Sam znak nie znika ani nie drga.


## 30. Audyt AES 2026-10-08: krok A i B wdrożone

Źródło: `.../work/aes-audit-20261008/AUDYT.txt` + `result.txt` (testy na hoście,
AddressSanitizer).

### Krok A — granice ABI i logger (audyt P1, poz. 2)

* `appl_init` zapisuje **jedno** słowo wyniku (`intout[0]`), wcześniej cztery
  (dodatkowe 3 słowa potwierdzone testem znaczników).
* `evnt_button`/`evnt_mouse` — **5** słów (mx, my, stan przycisku, kliknięcia,
  kstate); `evnt_mesag`/`evnt_timer` — **1** słowo. Wspólna gałąź zapisywała
  osiem (7 dodatkowych słów dla `evnt_timer`).
* Logger: czyta **5** słów `contrl` (było 6), a `intin`/`intout` ograniczone
  licznikami `contrl[3]`/`contrl[4]`; `intin[0]` tylko gdy licznik > 0.
  To usuwa trzy niezależne odczyty poza buforem z ASan.
* `tos_aes_opcount` liczy **tylko AES**; VDI liczy się we `tos_vdi_call`
  (wcześniej jeden histogram obejmował oba API o pokrywających się numerach).

### Krok B — przycięcie i pozycja obiektów (audyt P1, poz. 1 i 5)

* Kontekst przycięcia AES (`aes_clip_set`/`aes_clip_rect`), ustawiany z
  `intin[2..5]` w `objc_draw` (42) i `objc_change` (47) — zgodnie z wiązaniami
  z dokumentacji (`ob_drxclip..ob_drhclip`, `ob_cxclip..ob_chclip`).
* Wszystkie wypełnienia i ramki przechodzą przez `AES_CLIP_FILL`, a tekst przez
  `aes_obj_text` (przycinanie **znak po znaku**, więc częściowo widoczny napis
  nie maluje poza żądanym prostokątem).
* `objc_draw` liczy **przesunięcie przodków** przed rysowaniem: dziecko (10,20)
  pod rodzicem (100,50) rysuje się w (110,70), nie w (10,20).
* Dirty zgłasza tylko to, co faktycznie namalowane (przycięte prostokąty idą do
  `tos_vdi_fill_rect`).

### Test odbiorczy (tests/test_tos_aes_objects.py, `make check`)

```
bez przycięcia, panel 640x400 : 258080 zleconych pikseli   (liczba z audytu)
clip (32,32,8,16)             : 128                          (oczekiwane 128)
clip (100,100,4,4)            : 16
clip (700,500,4,4)            : 0 i zero wywołań
dziecko (10,20) + rodzic (100,50): bezwzględne (110,70)
```

### Ważny wynik negatywny (uczciwie)

Po wdrożeniu B **menu SysInfo nadal zajmuje ~12 s i 5062 wypełnień** — bo to
menu rysuje sam program wywołaniami VDI, a nie przez `objc_draw`/`objc_change`.
Czyli ignorowanie przycięcia było realnym błędem (i jest naprawione), ale
**nie ono odpowiada za tę konkretną zwłokę**. Koszt tego menu siedzi w narzucie
pojedynczego wywołania VDI i w prezentacji (kroki C/E audytu).


## 31. Przegląd 2026-10-08 (aes-review-20261008-latest): pilne poprawki + linie odcinkami

Źródło: `.../work/aes-review-20261008-latest/ZALECENIA.txt` + `result.txt`.

### Pilne (defekty w poprzednim kroku A/B)

1. **Logger wg właściwego ABI** — `aes_abi_in()/aes_abi_out()`: AES bierze liczby
   z `control[1]`/`control[2]`, VDI z `control[3]`/`control[4]`. Poprzednio
   indeksy VDI były używane dla obu API, więc dla AES czytane były liczniki
   tablic **adresów**: `objc_draw` z wejściem 11,12,13,14 zapisywał 11,0,0,0,
   a wynik nie trafiał do pola `intout`. Odczyty `intin`/`intout` mają teraz
   jawne `if(intin)`/`if(intout)` — to usuwa odczyty przy wskaźniku NULL
   (`evnt_mesag` bez intin, `shel_read` z jednoelementowym buforem).
   Komentarz „VDI control to pięć słów" poprawiony: VDI używa też 5, 6 i 7..10.
2. **Clip tekstu dociera do maski glifu** — `tos_vdi_text_clip()`/`_off()` w
   `tos_vdi.c`, ustawiane przez `aes_obj_text`; `vdi_glyph` przecina każdy
   odcinek z tym prostokątem. Pełna widoczna komórka idzie szybką ścieżką bez
   clipu. Nie ruszamy `vs_clip` aplikacji (osobny kontekst).
   Zmierzone w teście: pełny glif 8x16 z clipem 1x1 → **1 piksel, 0 odcinków
   poza clipem** (przed: 128 pikseli w 16 odcinkach).
3. **Zerowy clip nie rysuje** — `width<=0` lub `height<=0` ustawia **pusty**
   clip (nic nie jest malowane), zamiast wyłączać przycinanie. `objc_change`
   nadal zmienia stan obiektu.
4. **Diagnostyka `v_justified` ograniczona licznikami** — kopie 4/8/12/16 słów
   `intin` i 4/8 słów `ptsin` respektują teraz `control[3]` i `control[1]`.

### Optymalizacja A: linie poziome i pionowe jednym odcinkiem

`vdi_line` dla linii poziomej/pionowej w trybie zapisu `replace` i stylu
ciągłym rysuje **jeden odcinek na krok szerokości**; styl kreskowany/kropkowany
oraz tryby inne niż replace (XOR, overlay) zachowują ścieżkę po pikselu, bo
odcinek nie wyraża ich semantyki.

### Testy (`make check`)

```
tests/test_tos_vdi_draw.py   (nowy):
  obrys 200x120: 4 wywołania vdi_fill i 644 piksele  (audyt: 644 wywołań, 644 px)
  linia 200 px pozioma / 120 px pionowa: 1 odcinek każde
  linia gruba 3 px: 3 odcinki; styl kropkowany i XOR: ścieżka po pikselu
  glif 8x16, clip 1x1: 1 piksel, 0 poza clipem  (audyt: 128 px, 16 odcinków)
  glif bez clipu: 16 odcinków, 128 pikseli; clip 4 kolumny: 64 px
tests/test_tos_aes_objects.py (rozszerzony):
  przez PRAWDZIWĄ gałąź objc_draw: clip 8x16 -> 128 px; width=0 -> 0 px;
  pełny panel -> 258080 px; dziecko (10,20) pod rodzicem (100,50) -> (110,70)
  logger: AES control[1]/[2], VDI control[3]/[4]
```

### Czego NIE udowodniłem

Wpływu tych zmian na czas menu SysInfo **w Hatari**. W ostatnim przebiegu trzeci
klik wywołał małe odświeżenie (154 noty) zamiast wcześniejszej serii 5062, bo
harness nie ma `mousemove` i kursor trafiał w inne miejsce — liczby nie są
porównywalne. Sam host-test dowodzi jedynie, że zniknęły setki wywołań
wypełnienia w rysowaniu linii i że clip tekstu działa.


## 32. Optymalizacje B, C, D z przeglądu 2026-10-08

### C — renderer znaków słowami (największy zysk)

* `emutos_vdi_mono.c`: nowy `mono_glyph_h(...)` z parametrem wysokości;
  dotychczasowe `mono_glyph()` to teraz wrapper na 16 wierszy (zgodność).
  **Wysokość 8 była konieczna**: SysInfo rysuje tekst przy `vst_height 8`, więc
  pierwsza wersja (tylko 16) nie włączała się ani razu — pomiar pokazał
  `glyph_fast=0`.
* `tos_vdi.c` `vdi_glyph()`: dla pełnej komórki 8x8 lub 8x16, powierzchni mono
  planar, trybu `replace` i komórki w całości wewnątrz powierzchni oraz obu
  clipów wywołuje `mono_glyph_h()` — 16 (lub 8) aktualizacji słów zamiast
  jednego `vdi_fill` na odcinek wiersza — i zgłasza **jedną** zmianę dla całego
  znaku. `mono_glyph*` nie przycina bufora, więc pozycja jest walidowana
  wcześniej (zgodnie z zastrzeżeniem audytu). Brzegi, tryby inne niż replace i
  przypadek przycięty zostają na ścieżce odcinków.

### B — blitter tylko powyżej progu

`vdi_fill()` uruchamia blitter wyłącznie dla obszarów `>= TOS_VDI_BLIT_MIN_PIXELS`
(64 px). Poniżej tej wielkości CPU ma szybszą ścieżkę; próg jest pierwszym
przybliżeniem i wymaga pomiaru na sprzęcie (audyt: programowanie rejestrów i
IPL7 dla 1x1 to strata).

### D — kopiowanie rastra słowami i kierunek

* `vdi_cpyfm()`: szybka ścieżka dla `vro_cpyfm` z ROP=3, identycznej geometrii i
  **tego samego wyrównania bitowego** — kopiuje całe słowa z maskami brzegowymi.
  Różne wyrównanie (wymaga wariantu z osobnymi stride) zostaje na ścieżce
  pikselowej.
* Kierunek nakładania: `backwards` liczy się teraz z **kolejności w pamięci** —
  `dy>sy` albo (`dy==sy` i `dx>sx`). Stary warunek wymagał `dx>sx && dy>=sy`,
  więc przesunięcie w dół przy `dx==sx` kopiowało z góry i rozmazywało źródło.

### Pomiar w Hatari (ten sam obraz i program, autostart SysInfo)

| metryka | przed C | po C (z wysokością 8) |
|---|---|---|
| glify szybką ścieżką | **0** | **164** |
| trapy VDI na stronę | 38 | 38 |
| wypełnienia VDI na stronę | 879–1142 | poniżej progu 800 (strona to głównie tekst) |
| przemalowanie po kliknięciu | `fill+1142`, 6,5 s | `fill+692`, ~1,3 s |

Caveat: scenariusz nie jest w 100% powtarzalny (harness nie ma `mousemove`,
więc pozycja kursora różni się między przebiegami), a `vdi_calls` pokazuje, że
jedno wywołanie VDI generuje setki wypełnień — to potwierdza zastrzeżenie
audytu, że liczba `vdi_fill` nie jest liczbą trapów.


## 33. E — prezentacja: kursor dirty, scalanie według kosztu, limit

### Kursor w `tos_damage_take` (jedno przejście)

`tos_damage_t` ma teraz pole `first` — najniższy wiersz, który może być jeszcze
brudny. `tos_damage_note` je obniża, `tos_damage_take` startuje od niego i po
pobraniu pasma podnosi. Wcześniej każde pobranie szukało od wiersza 0, więc przy
N prostokątach te same wiersze przechodzono N razy (ekran to 512 wierszy).

### Scalanie według kosztu zamiast jednej obwiedni

Obwiednia jest właściwa przy **gęstych** zmianach (jedno przemalowanie menu), ale
przy dwóch małych zmianach daleko od siebie kazała konwertować prawie cały
ekran. Teraz zbierana jest **ograniczona lista** (`TOS_PRESENT_RECTS = 8`):

* jeśli lista się przepełni albo obwiednia jest nie większa niż suma pól plus
  `TOS_PRESENT_MERGE_SLACK` (4096 px) → jedna konwersja unii,
* w przeciwnym razie konwertowane są osobne prostokąty.

Liczniki `kdbg_perf_present_split` / `kdbg_perf_present_union` pokazują, która
ścieżka została wybrana. Skan surowych zapisów (wykrywanie bezpośrednich zapisów
do Physbase) **pozostaje** — audyt wprost zabrania jego usuwania.

### Profil na żywo (autostart SysInfo, ten sam obraz)

```
t[s]   fills   note frames   cykle  skany  skan[KB]  blit[px]  rozdz  unia
24.0       0      0      0    1000      0         0    256000      0     0
25.0       0   2000      1    1383      7       156    512000      0     1   <- tlo
33.1       0   3075     81   18571     92      2875   1180824     45    71   <- rysowanie
37.1+      0  +8/2s  +1/2s   +126   +25/2s   +787KB   +1032/2s     -     -   <- spoczynek
```

* Faza rysowania to zmiany **gęste** — wybierana jest unia (71) obok podziałów
  (45), czyli hybryda działa zgodnie z założeniem.
* W spoczynku konwersja to **~1032 px na tik zegara** (dwa małe fragmenty daleko
  od siebie) — to jest zysk z rozdzielania: wcześniej obwiednia objęłaby dużo
  więcej ekranu.
* Cykl w spoczynku: +126 jednostek na ~2 s, skany surowe ~12-25 Hz.
* **Czego nie ma**: skrócenia fazy gęstego rysowania (t≈25-35 s). Tam zmiany są
  gęste, więc unia była już właściwym wyborem, a czas to praca gościa plus
  kompozycja na deadline 25 Hz — nie narzut scalania.


## 34. LDW Power v2.00 — instalacja i próba uruchomienia (2026-10-08)

### Co to jest

`LDW Power v2.00 (1990)(Logical Design Works).zip` zawiera **obraz dyskietki
1,44 MB** (Atari HD: FAT12, 2880 sektorów, 112 wpisów root, sektor startowy
`EB 30` = bootowalna). Rozpakowane 20 plików:

```
LDWPOWER.PRG   474092 B   program główny (nagłówek 601A, tlen=377104
                          dlen=70720 blen=23678; plik ma 2568 B nadmiaru
                          na końcu - artefakt oryginalnej dyskietki)
LDW.HLP         53148 B   pomoc
LDWPOWER.DOC   341001 B   dokumentacja
AUTO\GDOS.PRG    9521 B   loader GDOS
GDOS.FNT\*       ...      czcionki ATTR10..24 + FX80.SYS + META.SYS
ASSIGN.SYS        320 B   konfiguracja GDOS (path = a:\gdos.fnt)
EXAM\*.LDW       ...      przykładowe dokumenty
```

### Instalacja w ATOS

`work/ldw.img` = obraz ATOS + pliki LDW w `\SYSTEM\TEST` (płasko, `ASSIGN.SYS`
poprawiony na `path = a:\system\test`) + `LAUNCH.CFG` z **LDW jako pierwszą
ikoną**. Weryfikacja: `\SYSTEM\TEST\LDW.PRG` = 474092 B, nagłówek `601a`,
`tlen+dlen+blen` zgodne z rozmiarem.

Ikona LDW **pojawia się na pulpicie ATOS** (`zrzuty/34-pulpit-z-LDW.png`).

### Wynik uruchomienia

* Z pulpitu: klawiatura nie przeniosła zaznaczenia (harness nie ma `mousemove`,
  a strzałki nie zmieniły zaznaczonej ikony) — Enter otworzył „My Atari".
* Autostart kernela (`kernel_tos_run_program("\\SYSTEM\\TEST\\LDW.PRG")`):
  program **wczytuje się i startuje** (w logu emulatora widać PC w TT-RAM),
  ale **ginie we własnej inicjalizacji**: `tos_aes_log_n = 0`, czyli **ani jedno
  wywołanie AES/VDI** nie zostało wykonane, `kernel_fault_vector = 0`.
  Jedyna anomalia: odczyt spod adresu `$10`.
* Referencja pod EmuTOS: oryginalna dyskietka **jest bootowalna**, ale jej loader
  wpada w **Illegal Instruction** (`PC=$00011cc`, `basepage=$03de0`,
  `D3=444d4172` = „DMAr") — czyli dyskietka wymaga konkretnego TOS-u/GDOS-u.

### Diagnoza

LDW Power to program **GDOS-owy** (czcionki + `ASSIGN.SYS` + `AUTO\GDOS.PRG`),
a nasz persona TOS **nie ma GDOS** (nic w drzewie nie implementuje rozszerzeń
GDOS). Dodatkowo start ginie *przed* pierwszym wywołaniem GEM, więc czegoś
w środowisku GEMDOS/systemowych wektorów jego loader oczekuje.


## 35. GDOS: warstwa fontów + diagnoza LDW Power

### Co wdrożone (GDOS)

Nowe pliki `include/tos_gdos.h` + `system/tos_gdos.c` (dopisane do `TOS_OBJS`):

* `tos_gdos_assign_parse()` — parser `ASSIGN.SYS`: `path = ...`, linie urządzeń
  (`01p screen.sys` **oraz** `21 fx80.sys` bez `p` — tak jest na tej dyskietce),
  lista plików `.FNT` przypisana do urządzenia; komentarze `;`, CRLF, puste linie.
* `tos_gdos_fnt_probe()` — nagłówek bitmapowego `.FNT`, odczytany **empirycznie**
  z prawdziwych plików: bajt 0 = id (0 bitmapa / 1 outline), bajt 2 = punkt
  (10/12/24 — zgadza się z nazwami plików), offset 4 = nazwa („Dutch", 32 B).
* `tos_gdos_boot_load()` — szuka `ASSIGN.SYS` (root, `\SYSTEM`, `\SYSTEM\TEST`,
  `\GDOS.FNT`), parsuje go i sonduje nagłówki fontów ≤ 64 KB.

Opcody VDI (numeracja z tablicy dyspozytora **EmuTOS-VDI** w drzewie:
`vst_load_fonts`=119, `vst_unload_fonts`=120, `vqt_name`=130, `vqt_fontinfo`=131):

* **119** → `intout[0]` = liczba fontów (to jest sygnał „GDOS jest"),
* **120** → 0,
* **130** → `intout[0]` = długość + nazwa w 16 słowach (2 znaki na słowo),
* **131** → metryki (minade 0x20, maxade 0x7E, wysokość/szerokość znaku),
* **132/133** `v_ftext` / `v_ftext_offset` → tekst przez systemowy generator
  glifów (bajtów fontów bitmapowych jeszcze nie renderujemy — powiedziane wprost).

Nic nie udajemy: outline'y (Speedo), metafiles i sterowniki drukarek **nie** są
zaimplementowane i nie są zgłaszane jako obecne.

### Test (`make check`)

`tests/test_tos_gdos.py` na **prawdziwym** `ASSIGN.SYS` z dyskietki LDW
(`tests/data/ldw-assign.sys`): `path = a:\gdos.fnt`, urządzenie ekranu 1,
drukarki 21, **8 fontów** z poprawnym przypisaniem do urządzeń, plus przypadki
brzegowe (komentarze, CRLF, `NOTAFONT.TXT`, zbyt krótki nagłówek, nazwa
nie-ASCII ucinana). Wszystko PASS.

### Diagnoza LDW Power (ważna korekta)

Uruchomienie z warstwą GDOS nadal kończy się przed pierwszym wywołaniem GEM, ale
teraz wiadomo **dlaczego**: `kernel_fault_vector = 2` (Bus Error) w kodzie gościa
(`PC=$04050d9a`, odczyt spod `$10` = tablica wektorów 68k), `tos_aes_log_n = 0`.

Przyczyna: nasz persona **nie honoruje wektorów wyjątków zainstalowanych przez
program** — w `tos_prg.c` jest to opisane jako „honouring guest vectors is a
separate, documented gap". LDW (jak wiele programów GDOS-owych) instaluje własny
handler wyjątku, a gdy wyjątek następuje, my kończymy program zamiast skoczyć do
jego handlera. GDOS nie był pierwszym blokerem — brak obsługi wektorów gości nim
jest.


## 36. Honorowanie wektorów wyjątków gościa (krok 1) — i co jeszcze blokuje LDW

### Wdrożone

`system/tos_entry.S`, makro `tos_fault_stub` (wektory 2-8) oraz stub Line-F (11):

* W chwili wejścia stubu **ramka wyjątku 68k leży nietknięta na stosie gościa**
  (CPU już ją odłożył), więc gdy slot wektora nie wskazuje już na nasz stub —
  czyli program zainstalował własny handler — oddajemy wyjątek zwykłym `jmp`
  do handlera programu. Program dostaje **dokładnie** taką ramkę, jakiej
  oczekuje jego `rte`.
* Ochrona przed zapętleniem: tabela `tos_fault_stub_table` z ośmioma naszymi
  stubami (2-8, 11) — jeśli slot nadal na nie wskazuje, idziemy starą ścieżką
  (raport + zakończenie programu). Dodatkowo odrzucamy adresy nieparzyste
  i poniżej `0x100`.
* Zastrzeżenie zapisane w kodzie: sprawdzenie zużywa `d0`, `d1`, `a0`, `a1`
  (zapisywane i odtwarzane), więc handler programu zobaczy te rejestry zmienione;
  **ramka, stos i pozostałe rejestry są nietknięte**.

`make check = 0`, kernel linkuje `tos_gdos.o`.

### Czego nie rozwiązało i co ustalono o LDW

LDW nadal kończy się `kernel_fault_vector = 2` (Bus Error) w swoim kodzie, przed
pierwszym wywołaniem GEM. Ustalenia z prób:

* Forwardowanie wektorów **nie zadziałało**, bo LDW **nie zainstalował** własnego
  handlera tego wektora — czyli to nie jest próba przechwycenia wyjątku.
* Sonda pamięci (debugger Hatari, ta sama konfiguracja) pokazuje, że **niska
  pamięć istnieje i zawiera prawdziwą tablicę wektorów**:
  `0x00 -> 602e0206`, `0x08 -> 00e001d6`, `0x10 -> 00e001fc` (adresy w naszym
  kernelu). Czyli RAM i wektory są na miejscu.
* Wyłączenie emulacji MMU (`--mmu off`) **nie zmieniło** objawu.
* Hatari zgłasza `Bus Error reading at address $10, PC=$4050d9a` — PC w TT-RAM
  (kod LDW), więc to odczyt `$10` z kodu gościa.

Wniosek: różnica wobec prawdziwego TOS nie leży w instalacji wektorów ani w
istnieniu pamięci, a w **środowisku dostępu** (tryb/ochrona) albo w tym, że LDW
czyta przez wskaźnik zerowy+offset i na realnym TOS-ie trafia w mapowaną stronę.
Następny krok diagnostyczny: uruchomić w tej samej konfiguracji **nasz własny
program TOS, który czyta niską pamięć** (TOSMONO/TOSPROBE) — jeśli on działa,
problem jest specyficzny dla LDW, a nie dla środowiska.


## 37. Sonda niskiej pamięci (`TOSLOW.PRG`) — i dlaczego jej nie widać

Nowy program testowy `system/toslow.c` → `TOSLOW.PRG` (budowany tą samą ścieżką
co TOSPROBE/TOSVDI). Robi trzy rzeczy:

1. czyta wektor spod `$10` i `_hz_200` spod `$4ba` (przez inline asm — stałe
   adresy w C wywalają `-Werror=array-bounds`, tak samo jak w personie),
2. instaluje własny handler wektora 2 (Bus Error) i sprawdza, czy zapis jest
   widoczny,
3. czyta spod `$00800000` (poza ST-RAM) i oczekuje, że wyjątek trafi do
   zainstalowanego handlera (test forwardowania z sekcji 36).

**Wynik: ekran pulpitu, brak outputu.** Przyczyna jest w personie, nie w
programie: `tos_print()` pisze do prywatnego ekranu TOS tylko wtedy, gdy ten
ekran istnieje (`tos_screen`), a program, który nie wykonał żadnego wywołania
VDI/AES, nie dostaje okna — więc jego tekst (Cconws) nigdzie nie jest widoczny.
To ta sama klasa problemu, co „brak okna = brak informacji zwrotnej”.

Następny krok: gdy program TOS nie ma okna, kierować konsolę persony do konsoli
jądra (widocznej w oknie Hatari), żeby output takich programów był czytelny —
wtedy TOSLOW odpowie na pytanie o niską pamięć i o forwardowanie wektorów.


## 38. Konsola persony dla programów bez okna

`tos_console_byte()`: gdy program TOS **nie wszedł w tryb okna**
(`tos_window_enabled == 0`) albo nie ma prywatnego ekranu, jego output idzie na
**konsolę jądra** zamiast na niewidoczny prywatny ekran. To naprawia klasę
problemów „program coś wypisuje, ale nic nie widać" (dotyczyło TOSLOW i LDW).

Ograniczenie, które zostaje: program uruchomiony **z pulpitu** ma output na
konsoli jądra, ale pulpit tę konsolę zasłania — żeby zobaczyć output, program
trzeba uruchomić z naszego TERMINAL-a albo przed pojawieniem się pulpitu.
Docelowo: okno konsoli dla programów klasycznej konsoli (bez VDI/AES).


## 39. Dlaczego LDW padał: tryb użytkownika vs ochrona niskiej strony

### Pomiar, który to rozstrzygnął

Sonda `TOSLOW.PRG` (czyta `$10` i `$4ba`, instaluje wektor 2, wywołuje bus error)
w tej samej konfiguracji co LDW:

```
WARN : Bus Error reading at address $10, PC=$4000196 op=2410   <- odczyt sondy
kernel_fault_vector = 2
```

PC `$04000196` leży w TPA gościa, opcode `0x2410` to `move.l (a0),d2` z `a0=$10`
— czyli **odczyt niskiej pamięci przez program TOS faultuje**, mimo że sonda
debuggera widzi tam prawdziwą tablicę wektorów (`0x10 -> 00e001fc`).

Przyczyna: `tos_entry.S` wchodzi do gościa z `SR = #0x0500`, czyli w **trybie
użytkownika**, a maszyna TF536 mapuje niską stronę systemową (wektory, `_hz_200`,
`_v_bas_ad` w `0x0000-0x0800`) jako **tylko dla supervisora**. Na zwykłym ST nie
ma ochrony, więc program może te adresy czytać i pisać.

### Poprawka i jej weryfikacja

Wejście gościa ustawione na **tryb supervisora** (`SR = #0x2500`, IPL 5 bez
zmian). Ten sam test:

```
kernel_fault_vector = 0
histogram VDI pusty (program wiruje, pulpit nie wstał)
```

Czyli: odczyt niskiej pamięci **przechodzi**, a skoro program nadal żyje po
celowym bus errorze, to **wyjątek trafił do jego własnego handlera** — co
potwierdza niezależnie forwardowanie wektorów z sekcji 36.

### Zastrzeżenie (świadomy kompromis)

Tryb supervisora oznacza, że program TOS nie jest odseparowany od jądra — tak
samo jak na prawdziwym ST, gdzie TOS też nie chroni pamięci. Alternatywa
(dopasowanie deskryptorów MMU tak, by niska strona była dostępna z user mode)
jest bardziej „poprawna”, ale wymaga grzebania w tablicach, których nie tworzy
nasz kod (pochodzą z konfiguracji TF536/Hatari). Ta zmiana jest więc
kompatybilnościowym kompromisem i jest tak opisana w kodzie.

### LDW po tej poprawce: kolejny, konkretny błąd

```
WARN : Bus Error writing at address $4, PC=$400011c op=2b49
```

Pierwszy błąd (odczyt niskiej pamięci) jest **za nim**; teraz zatrzymuje go
**zapis longa pod `$4`** (adres w tablicy wektorów) z jego własnego kodu. To
następny trop do zbadania — z dokładnym adresem i opcode.


## 40. Sonda etapowa: cała niska pamięć działa poza pierwszymi 8 bajtami

`TOSLOW.PRG` prowadzi teraz dziennik etapów w BSS (`toslow_stage`), a jego
handler wektora 2 ustawia bit `0x80000000`, więc po uruchomieniu odczyt z TPA
mówi dokładnie, co się stało — bez potrzeby widocznego ekranu:

```
t=22.5s  toslow_stage = 0x80000007
```

| etap | operacja | wynik |
|---|---|---|
| 1-2 | odczyt `$10` (wektory) i `$4ba` (`_hz_200`) | OK |
| 3-4 | zapis wektora 2 + odczyt zwrotny | OK |
| 5-6 | **zapis longa** pod `$400` + odczyt zwrotny | OK |
| **7** | **zapis longa pod `$4`** | **Bus Error** |
| 8 | zapis bajtu pod `$500` | nieosiągnięty |

Wnioski:

1. **Niska pamięć jest dostępna** dla programu TOS — czyta i pisze (także longi)
   od `$10` w górę. Wcześniejszy fault na odczycie `$10` zniknął po przejściu na
   tryb supervisora (sekcja 39), więc ta zmiana jest potrzebna.
2. **Forwardowanie wektorów działa**: bit `0x80000000` w znaczniku dowodzi, że
   wyjątek trafił do handlera **zainstalowanego przez program** (sekcja 36).
3. Jedyny niedostępny obszar to **pierwsze 8 bajtów** (`$0..$7` = początkowy SSP
   i PC procesora). To obszar, który na ST jest zwykłym RAM-em, a w tej
   konfiguracji TF536/Hatari jest chroniony (overlay wektorów resetu).
4. **LDW pada dokładnie na tym**: `Bus Error writing at address $4` (sekcja 39).
   Czyli blokerem nie jest już ani GDOS, ani tryb pracy, ani brak pamięci, a
   **ochrona pierwszych 8 bajtów** w emulowanej maszynie — programy, które
   zapisują/odtwarzają całą tablicę wektorów od `$0`, zatrzymują się tutaj.
   Naprawa należy do konfiguracji maszyny (TF536/Hatari), nie do persony TOS:
   persona nie może przemapować adresu, który emulacja trzyma jako ROM.


## 41. Dwie nowe rzeczy: ograniczenie emulatora i błąd w ładowaniu GDOS

### a) Zapis w `$0..$7` — ograniczenie konfiguracji TF536/Hatari

Sprawdzone: przełączenie `--patch-tos` (true/false) **nie zmienia** wyniku;
w źródłach forka nie ma opcji ani gałęzi specjalnej dla tych adresów, jest
natomiast komentarz „also see SysMem_wput for addr < 0x8”. Czyli pierwsze
8 bajtów (początkowy SSP/PC) jest w tej maszynie chronione, podczas gdy resztę
niskiej pamięci program TOS czyta i pisze bez problemu (sonda: etapy 1-6 OK,
etap 7 = zapis longa pod `$4` = Bus Error). LDW pada dokładnie tam.
To ograniczenie emulacji, nie persony — persona nie przemapuje adresu, który
emulacja trzyma jako ROM.

### b) Sonda GDOS znalazła błąd w mojej warstwie GDOS

Nowy program `system/tosgdos.c` → `TOSGDOS.PRG`: woła przez prawdziwy trap VDI
`vst_load_fonts` (119), `vqt_name` (130) i `vqt_fontinfo` (131) i zapisuje wyniki
w BSS, więc host odczytuje je z TPA (`0x04000274` w tym linku).

Wynik na prawdziwym kernelu:

```
font_count = 1            (oczekiwane 8 — tyle fontów jest w ASSIGN.SYS)
name_len   = 6
name       = "System"     (nazwa zapasowa; oczekiwane "Dutch")
fontinfo   = [32,126,16,8,0]   ptsout = [0,0,0,16]
done       = 0x600D600D
```

Czyli **ABI jest obsłużone** (program dostał sensowne odpowiedzi, żadnego
zawieszenia ani faultu), ale **ładowanie po stronie jądra nie działa**:
parser `ASSIGN.SYS` przechodzi testy hostowe (8 fontów, ścieżka, urządzenia),
a w kernelu zwraca 1 — więc problem jest w `tos_gdos_boot_load()`
(znalezienie/odczyt pliku albo sondowanie nagłówków `.FNT`).

To jest dokładnie wartość testu end-to-end: testy hostowe mogły tego nie
wychwycić, bo sprawdzały sam parser, a nie drogę „plik na dysku → warstwa GDOS".
Następny krok: zdebugować tę drogę, korzystając z sondy (powtarzalna, bez
potrzeby widocznego ekranu).


## 42. Audyt LDW/GDOS: punkt 1 i 2 wdrożone (stos, stuby, Kbdvbase, Iorec)

Źródło: `.../work/ldw-gdos-audit-20261008/AUDYT.txt` + `result.txt`.

### 1. Cofnięty nieprawidłowy start supervisor (błąd, który sam wprowadziłem)

`system/tos_entry.S` wraca do `SR = #0x0500` (tryb użytkownika). Program startuje
więc na **stosie użytkownika** przygotowanym przez `kernel_tos_run_program`
(`SP = p_hitpa - 58`, `0(sp)` atrapa, `4(sp)` = basepage), a nie na stosie
supervisora z kontekstem jądra. To wyjaśniało `A5 = 0` i zapis pod `$4`
(instrukcja `move.l A1,4(A5)` w oryginalnym LDW aktualizuje pole basepage).

### 2. Symetryczne stuby wyjątków i przekazanie bez psucia rejestrów

* zapis i odtworzenie to teraz **ten sam** zestaw `%d0-%d1/%a0-%a1` (16 bajtów)
  na **każdej** ścieżce — asymetria 16/12 przesuwała odczyt ramki i psuła `rte`
  w Line-F (znalezione przez audyt),
* przekazanie wyjątku do handlera programu: adres celu ląduje na stosie **pod**
  zapisanymi rejestrami, rejestry wracają z offsetu 4, a `rts` zjada cel —
  **żaden rejestr nie jest naruszony**, a SP wskazuje z powrotem dokładnie na
  ramkę wyjątku CPU, której oczekuje `rte` handlera.

### 3. Kbdvbase (XBIOS 34), Iorec (XBIOS 14) i mousevec

* `tos_kbdvecs_t` — prawdziwa, **zapisywalna** KBDVECS; `mousevec` jest piątym
  wskaźnikiem (offset `$10`), dokładnie tam, gdzie LDW go zapisuje i skąd czyta,
* `tos_iorec_t` — prawdziwe kolejki (bufor + rozmiar + head/tail) dla trzech
  urządzeń,
* oba bloki są **rezerwowane w pamięci programu PRZED** przyznaniem mu reszty
  TPA (program dostaje całą pozostałą przestrzeń jako `p_hitpa`, więc alokacja
  po niej nie miała gdzie powstać — to był mój błąd w pierwszym podejściu),
* dostawa pakietu do `mousevec` odbywa się z **jednego istniejącego właściciela
  myszy** (`tos_poll_mouse_owned`), konwencją Atari: kontekst supervisora,
  pakiet IKBD w `d0` (`buttons<<16 | dx<<8 | dy`), przez `asf_tos_call_mousevec`,
  który zachowuje wszystkie rejestry gościa. **Nie** dodano drugiego czytnika
  bajtów IKBD — zgodnie z zastrzeżeniem audytu o rozrywaniu pakietów.

`make check = 0`, kernel 437 442 B.

### Czego jeszcze NIE zweryfikowałem (uczciwie)

Sonda `TOSLOW` rozszerzona o XBIOS 34/14 raportuje `stage = 0` (przetrwała
wszystkie operacje na niskiej pamięci), ale odczytane wskaźniki `Kbdvbase`
i `Iorec(1)` wychodzą zerowe — a moje skrypty pomiarowe miały **adresy wpisane
na stałe**, które po każdym linku się przesuwają (audyt ostrzegał o tym wprost).
Czyli: kod jest wdrożony, ale **nie mam jeszcze pomiaru potwierdzającego**, że
`Kbdvbase` zwraca niezerowy wskaźnik. To pierwsza rzecz do domknięcia — z sondą,
która zapisuje wyniki i znacznik zakończenia, i z adresami licznymi z ELF
za każdym razem.


## 43. LDW POWER DZIAŁA — przełom po punktach 1 i 2 audytu

Zrzut `zrzuty/35-LDW-POWER-dziala.png`: okno **„LDW.PRO"** z paskiem menu
`CMD | READY | OK | CALC | SCRL | END | NOTE | HELP | DRAW`, białym obszarem
roboczym i kursorem poleceń — program działa w oknie pod ATOS.

Co to odblokowało (kolejność z audytu):

1. **Poprawny stos startowy** (cofnięty mój eksperyment z supervisorem) —
   `4(SP)` = basepage, więc `move.l A1,4(A5)` LDW aktualizuje swoje pole
   basepage, a nie adres `$4`.
2. **`Kbdvbase` (XBIOS 34)** zwracające prawdziwą, zapisywalną KBDVECS
   (`mousevec` = piąty wskaźnik, `$10`) + **`Iorec` (XBIOS 14)** z prawdziwymi
   kolejkami. Wcześniej XBIOS 34 zwracał 0 i LDW czytał `NULL+16`, czyli `$10`.
3. **Symetryczne stuby wyjątków** (16 B na każdej ścieżce) i przekazanie wyjątku
   do handlera programu przez stos + `rts`, bez naruszania rejestrów.

Dowód z logu jądra po uruchomieniu: setki **AES op=114** (`rsrc_obfix` — LDW
poprawia współrzędne własnych obiektów) oraz histogram VDI:
`6 v_pline, 8 v_gtext, 15/16/17 vsl_*, 23 vsf_interior, 32 vswr_mode,
37 vqf_attributes, 39 vst_alignment, 100 v_opnvwk, 106 vst_effects,
107 vst_point, 114 vr_recfl, 115 vqin_mode, 129 vs_clip`.

### Ważna lekcja procesu (mój błąd, który kosztował kilka cykli)

Jedno z wcześniejszych wstawień `case 14/34` **nie zapisało się w pliku**
(patch zgłosił sukces, ale w źródle go nie było). Testy wychodziły „zerowe"
i wyglądały jak błąd pomiaru. Dopiero **weryfikacja grepem po każdej zmianie**
plus sprawdzenie, że licznik istnieje w ELF (a nie tylko w źródle — `--gc-sections`
usuwa nieużywane symbole), pokazała prawdę. Od teraz: po każdym patchu
sprawdzam obecność zmiany w pliku i w bincie.


## 44. Punkt 3 audytu: ładowanie fontów GDOS — wdrożone i zweryfikowane

### Co poprawione

1. **Właściwe API plikowe.** Loader czyta nagłówki przez
   `kfat_read_path_at(path,0,head,sizeof head,&got)` zamiast `kfat_load_path()`,
   które wymaga miejsca na **cały** plik — dlatego 64-bajtowy bufor odrzucał
   wszystkie osiem fontów z dyskietki LDW (536..29696 B).
2. **Pełny `Fonthead`** (układ z EmuTOS `include/fonthdr.h`): `font_id`, `point`,
   `name[32]`, `first_ade`, `last_ade`, `top/ascent/descent/bottom`,
   `max_char_width/cell_width`, `flags`, `form_width/form_height` — czytane
   przez cały nagłówek do `next_font` włącznie.
3. **Kolejność bajtów jest rozstrzygana, nie zakładana**: `point` w sensownym
   zakresie decyduje, a przy remisie `flags`/F_STDFORM. Fonty LDW są
   **little-endian** (`0e 00` = ID 14, `18 00` = 24 pt), standardowe fonty
   ekranowe GDOS są motorolowe (big-endian) — oba warianty przechodzą walidację.
   `font_id` **nie** jest już traktowane jako znacznik bitmapa/outline.
4. **Rozdzielenie urządzeń.** ASSIGN.SYS z tej dyskietki przypisuje cztery
   rozmiary drukarce (21) i cztery metafile (31); **ekran (1..4) nie ma
   dodatkowych fontów**. `vst_load_fonts` raportuje teraz liczbę faktycznie
   wczytanych fontów **ekranu** (czyli 0), a nie 8 wpisów z jednej wspólnej listy.
5. **Liczone tylko faktycznie wczytane** (`loaded` ustawiane wyłącznie po
   walidacji nagłówka) i **stan ponawialny** (NOT_READY/READY/FAILED), więc próba
   wykonana przed gotowością dysku nie utrwala się na stałe.

### Weryfikacja

* Test hostowy `tests/test_tos_gdos.py` (PASS): 8 wpisów ASSIGN.SYS, podział
  urządzeń (ekran 0 / drukarka 4 / metafile 4), nagłówek LE (id=14, 24 pt) i BE
  (id=1, 24 pt, F_STDFORM), odcięcie nazwy nie-ASCII, odrzucenie zbyt krótkiego
  nagłówka i nierealnego rozmiaru punktu.
* **W kernelu, na prawdziwych plikach z obrazu**: `kdbg_gdos_loaded = 8`,
  `kdbg_gdos_rejected = 0` — wszystkie osiem nagłówków wczytanych poprawnie
  (`got = 90 B`, `fsize` zgodny z plikiem).
* Poprzedni pomiar „8 odrzuconych" pochodził ze **starego** loadera (64-bajtowy
  bufor) — czyli dokładnie z błędu opisanego w punkcie 4 audytu.

### Co zostaje

Punkt 4 audytu: ABI `vqt_name` (font ID + jeden znak na słowo), `vqt_fontinfo`
(2 słowa + 10 ptsout), realny wybór fontu, wykrywanie GDOS przez `D0 = -2`
z TRAP #2 — oraz przepisanie sondy `TOSGDOS` według standardowego bindingu
(uchwyt workstation, dokładne bufory). Sonda w obecnej postaci używa
`d0 = 0x73`, więc jej wywołania VDI nie trafiają do rejestru AES/VDI — to
zastrzeżenie audytu, które potwierdzam.


## 45. Punkt 4 audytu: ABI zapytań VDI (wdrożone, pomiar do domknięcia)

### Wzorce wzięte z EmuTOS (nie z pamięci)

`vdi/vdi_text.c` w drzewie dał dokładne kontrakty:

* **`vqt_name` (130)** — `intout[0]` = **font ID**, a nazwa idzie **jednym znakiem
  na słowo**, dopełniona zerami do 32 słów (`FONT_NAME_LEN`). Poprzednio pierwsze
  słowo było długością, a znaki pakowane po dwa — normalny binding czytał z
  „System" początek „ytm", dokładnie jak w Waszym teście.
* **`vqt_fontinfo` (131)** — `intout` = `first_ade`, `last_ade` (**2 słowa**);
  `ptsout` = `max_cell_width`, `bottom`, `thicken/0`, `descent`, `left/0`, `half`,
  `right/0`, `ascent`, `0`, `top` (**10 słów**). Poprzednio pisałem 5 słów
  `intout` i 8 `ptsout`, więc `max_width` wychodziło 0, a końcowe słowa `ptsout`
  zostawały nietknięte.
* **`vst_font` (21)** — zapisuje numer **i** wybiera font przez `tos_gdos_select`
  (0 i 1 = font systemowy), więc `vqt_fontinfo`/`vqt_name` używają metryk
  wybranego kroju, a nie systemowego.
* **`vst_load_fonts` (119)** — `select` to jedno słowo `intin`; wynikiem jest
  liczba **faktycznie wczytanych** fontów ekranu (na tej dyskietce 0, bo jej wpisy
  należą do drukarki i metafile — patrz §44).

### Wykrywanie GDOS — świadoma decyzja

`vq_gdos` używa `D0.w = -2` i `TRAP #2` (nie opcodu w zwykłym VDIPB). Nasza
warstwa GDOS jest minimalna (lista fontów, nazwy, metryki), **nie** jest
Speedo/NVDI, więc uczciwa odpowiedź tej gałęzi to nadal „brak GDOS" (-2) —
zgodnie z Waszym „nie podszywać się pod Speedo/NVDI". Zachowanie jest teraz
udokumentowane, a nie przypadkowe.

### Sonda przepisana na standard

`system/tosgdos.c` otwiera workstation (`v_opnvwk`, `contrl[0]=100`) i używa
zwracanego uchwytu; `vqt_name` sprawdza **font ID** i nazwę czytaną po jednym
znaku na słowo; `vqt_fontinfo` sprawdza 2 słowa `intout` i 10 `ptsout`; oba mają
**słowa-znaczniki 0x5555** postawione tuż za deklarowanymi długościami, żeby
nadmiarowe zapisy (błąd z Waszego testu) zostały wykryte.

### Czego NIE udało się jeszcze zmierzyć

Pomiar sondy znowu trafił w złe adresy (odczyt zwrócił bajty kodu programu), bo
adresy symboli liczę z ELF-a i po każdym linku się przesuwają — dokładnie ta
pułapka, o której pisał audyt. Wniosek inżynierski: **przestać liczyć adresy**,
a zamiast tego niech jądro poda adres bufora wyników (np. przez własny licznik /
wskaźnik), a sonda pisze do niego. To pierwsza rzecz do zrobienia przy następnym
kroku, żeby PASS cokolwiek znaczył.


## 46. Pomiar ABI fontów VDI (stały adres) + LDW w żywym oknie

### Naprawa pomiaru (lekcja z §45)

Sonda nie liczy już adresów symboli z ELF-a (przesuwały się po każdym linku i dwa
razy dały fałszywe zera). Wyniki idą pod **stały adres w TPA**: `0x04004100` —
jądro zna bazę TPA, więc host czyta bez żadnej arytmetyki na symbolach.

### Wynik pomiaru na prawdziwym kernelu

```
uchwyt workstation (v_opnvwk)  = 1
vst_load_fonts (119)           = 0        <- uczciwie: ekran nie ma dodatkowych fontów
vqt_name (130):  font_id       = 14       <- prawdziwe ID z ATTR10EP.FNT
vqt_fontinfo (131): intout     = [0, 32]  <- patrz niżej
                    ptsout     = [8,16,0,2,0,0,0,12,0,14]
sentinel intout[2]  = 0x5555               <- NIETKNIĘTY: brak nadmiarowych zapisów
sentinel ptsout[10] = 0x5555               <- NIETKNIĘTY
```

Co to potwierdza:

* **`vqt_name` zwraca prawdziwe `font_id`** (14 — zgodne z nagłówkiem pliku), a nie
  długość nazwy jak przedtem,
* **`vst_load_fonts` jest uczciwe**: 0 dodatkowych fontów ekranu, bo wpisy tej
  dyskietki należą do drukarki i metafile (§44),
* **znaczniki `0x5555` za deklarowanymi długościami są nietknięte** — czyli
  nadmiarowe zapisy, które wykrył Wasz test (`intout[2]` i `ptsout[8..9]`), są
  usunięte. To był Wasz najmocniejszy dowód na błędne ABI.

Co jeszcze nie zgadza się z kontraktem (do dokończenia):

* nazwa wychodzi jako `Dtc` zamiast `Dutch` — czyli w strumieniu „jeden znak na
  słowo" co drugie słowo jest zerowe (do ustalenia: strona zapisu czy odczytu),
* `vqt_fontinfo` `intout` wyszło `[0, 32]` zamiast `[32, 254]` — przesunięcie o
  jedno słowo (pierwsze słowo zerowe, `first_ade` trafiło w indeks 1).

### LDW w żywym oknie Hatari

Zrzut `zrzuty/36-LDW-zywe-okno.png` pochodzi **z działającego okna** (przez FIFO),
nie z przebiegu headless: okno „LDW.PRO" z paskiem `CMD | READY | OK | CALC |
SCRL | END | NOTE | HELP | DRAW`, obszar roboczy i kursor poleceń, a w tle ikony
pulpitu ATOS.


## 47. Okno 640x400, czyste zamknięcie LDW i znaleziony offset w ABI

### Okno ma dokładnie 640x400 wewnątrz (zmierzone)

```
tos_win_w = 640   tos_win_h = 400   tos_client_w = 640
```

Okno programu to klient + 18 px paska tytułu (`TOS_TITLE`), a wokół niego jest
pulpit w trybie 800x600 — stąd wrażenie, że okno jest większe. Wymóg „dokładnie
640x400 wewnątrz" jest spełniony; jeśli chodzi o **całość** okna (z paskiem),
to trzeba by albo usunąć pasek, albo powiększyć klienta — do decyzji.

### Zamknięcie po kilkunastu sekundach jest CZYSTE

```
kernel_fault_vector = 0     kdbg_tos_divzero = 0
zalogowanych wywołań GEM = 3711
ostatnie 14: AES op=114 (rsrc_obfix) dla obiektów 46..59
```

Program nie faultuje — **wychodzi sam**, po przejrzeniu zasobów (`rsrc_obfix` dla
obiektów 0..59, czyli całego swojego drzewa dialogowego). To jego własna decyzja,
najpewniej po nieudanym teście środowiska (GDOS), a nie awaria persony.

### Znaleziony błąd: przesunięcie wyjść VDI o jedno słowo

Podgląd tego, co **samo VDI zapisało**, rozstrzygnął sprawę:

```
VDI zapisał nazwę: ['0x0044','0x0075','0x0074','0x0063']  = "Dutc"  (1 znak/słowo)
VDI zapisał intout: ['0x0020','0x007E']                    = [32, 126]
```

a program widzi:

```
vqt_name:   'D\x00tch'          <- przesunięte
vqt_fontinfo: intout = [0, 126]
sentinel ptsout[10] = 0x000E    <- nasze `top`, czyli pisaliśmy o słowo dalej
```

Czyli **VDI pisze poprawne wartości, ale pod adres o jedno słowo przesunięty**
względem bufora programu — dla `intout` **i** `ptsout` jednocześnie. To wyjaśnia
zarówno „Dtc"/„Dutc" z Waszego testu, jak i **rozjeżdżające się wyrównanie
tekstu** GDOS. Naprawa jest wąska: znaleźć i usunąć ten offset w przekazaniu
wskaźników VDIPB (persona → VDI), a wtedy ponowić pomiar sondą ze znacznikami.


## 48. Odpowiedzi na trzy uwagi: okno, zamknięcie, wyrównanie tekstu

### 1. Okno 640x400 wewnątrz — spełnione (liczby, nie wrażenie)

```
tos_win_w = 640   tos_win_h = 400   tos_client_w = 640
```

Na ekranie okno wygląda na większe, bo dochodzi **18 px paska tytułu**
(`TOS_TITLE`) i wokół jest pulpit w trybie 800x600. Jeśli „640x400" ma dotyczyć
**całej** ramki, trzeba albo zmniejszyć klient do 640x382, albo zrezygnować
z paska tytułu — do decyzji.

### 2. Zamknięcie po kilkunastu sekundach jest czyste

```
kernel_fault_vector = 0    kdbg_tos_divzero = 0    wywołań GEM = 3711
ostatnie 14: AES op=114 (rsrc_obfix), obiekty 46..59
```

Program nie faultuje — kończy przegląd własnego drzewa zasobów (`rsrc_obfix` dla
obiektów 0..59) i **sam wychodzi**, najpewniej po nieudanym teście środowiska
(GDOS). Następny krok: zalogować jego wywołania **GEMDOS**, żeby zobaczyć ostatnią
operację na pliku/urządzeniu przed wyjściem.

### 3. Wyrównanie tekstu — kandydat znaleziony, ale pomiar jeszcze nie jest spójny

Podgląd po stronie VDI pokazuje wartości **poprawne**: „Dutc" (jeden znak na
słowo) i `intout = [32, 126]`. W jednym przebiegu program widział je jednak
przesunięte o jedno słowo (`ptsout[10]` trzymało nasze `top`), a w kolejnym nazwa
czytała się już poprawnie („Du…"). Czyli **instrumentacja wciąż generuje
artefakty** i nie mam prawa twierdzić, że offset jest udowodniony.

Wniosek: potrzebna jest **jedna samodzielna sonda** — program zapisuje pod stały
adres (0x04004100) swoje własne adresy buforów **i** otrzymane wartości, a host
czyta wyłącznie ten blok, bez sięgania do symboli jądra. To rozstrzygnie
definitywnie, czy offset jest w przekazaniu wskaźników, czy w moim odczycie.

### Uwaga operacyjna

`ps`/`pgrep` w tym środowisku bywają niedostępne, a Hatari blokuje obraz
(„Locking IDE HD file for writing failed"), więc równoległe instancje na tym
samym obrazie nie wstaną. Do pokazywania LDW w oknie używam świeżej kopii obrazu
i osobnego katalogu projektu.


## 49. „LDW.PRG zawiesił system" — przyczyna: pozorne oczekiwanie na zdarzenia

### Co było źle

```
case 23:   /* evnt_mesag */
case 24:   /* evnt_timer */
    if(intout)intout[0]=0;
    return 0;
```

Program GEM po otwarciu okna **czeka na `WM_REDRAW`** i siedzi w `evnt_mesag` /
`evnt_multi` z maską komunikatów. Nasze atrapy odpowiadały „brak zdarzenia"
natychmiast, a `WM_REDRAW` był podawany **tylko raz** (jednorazowa flaga
`tos_redraw_sent`). Program wchodził więc w pętlę bez wyjścia — dokładnie to
widział użytkownik jako „zawieszenie systemu". Audyt przewidział to w punkcie 5
(„oczekiwanie na zdarzenia jest częściowo pozorne").

### Co wdrożone

* **Prawdziwa kolejka komunikatów AES** (8 wpisów, post/take, zerowana na start
  programu).
* **`WM_REDRAW` wysyłany w chwili otwarcia okna** — komunikat, na który program
  czeka tuż po `wind_open`; to on sprawia, że program rysuje.
* **`evnt_mesag` (23) naprawdę czeka** na komunikat: z ograniczeniem czasu
  (domyślnie ~2 s, maks. ~5 s) i twardym limitem iteracji, w pętli odpytywania
  wejścia i odświeżania ekranu, więc pulpit pozostaje żywy. Po przekroczeniu czasu
  oddaje `WM_REDRAW` — tak zachowuje się prawdziwy menedżer okien.
* **`evnt_timer` (24) naprawdę odczekuje** zadane milisekundy (z ograniczeniem,
  żeby śmieciowy czas nie zablokował maszyny).
* **`evnt_multi` bierze komunikaty z kolejki** (zamiast jednorazowej flagi).
* **Brak okna = brak zdarzeń okienkowych**: program konsolowy dostaje odpowiedź
  natychmiast, bez czekania na nic.

### Test

Test `test_tos_console_events.py` podmieniał `tos_msg_take` na „zawsze brak
komunikatu", więc po zmianie czekał w nieskończoność — to był **błąd testu**, nie
jądra. Stub podaje teraz `WM_REDRAW` tak, jak prawdziwe jądro po otwarciu okna.
`make check = 0`, 40 testów PASS.


## 50. LDW działa i czeka na wejście — widoczne na ekranie

Zrzut `zrzuty/37-LDW-dialog-dziala.png` z **żywego okna**: LDW Power narysował
pełny dialog GEM — okno `C:\BGM`, ścieżkę `\SYSTEM`, pola `NAME | DATE | TYPE`,
listę plików dysku (`ACSI.SYS 656`, `BOOT.SYS 198`, `CORE.SYS 100`,
`DESKTOP.CFG 30`, `EVENT.SYS 470`, `GFX.APP 401`, `GUI.APP 629`, `INIT.APP 110`,
`MORE >`) oraz drugie okno z monitem o wybór napędu. Program **czeka na
wejście** — dwa zrzuty wykonane 6 s od siebie są **bajt w bajt identyczne**, więc
ekran nie jest przemalowywany w pętli.

Pomiar liczników (ten sam obraz, przebieg bez okna):

```
t(s) | wywołań GEM | evnt_mesag | fault | divzero
  36 |          577 |          1 |     0 |       0
  52 |         2874 |          1 |     0 |       0
  80 |         6330 |          1 |     0 |       0
```

* licznik GEM **rośnie przez cały czas** — program żyje i pracuje, a nie kończy
  się po kilkunastu sekundach jak przed poprawką,
* `evnt_mesag` obsłużone (dostał swój `WM_REDRAW`) — czyli nowa kolejka
  komunikatów działa,
* **zero faultów** i zero dzielenia przez zero,
* program „wisi" w sensie poprawnym: czeka na zdarzenie, a nie kręci się bez
  wyjścia — dlatego pulpit pozostaje żywy i nie ma już „zawieszenia systemu".

### Widoczny na ekranie błąd wyrównania tekstu

W dolnym oknie widać zniekształcony napis (`DRI_CLICK = _OPEN_DRIUE` zamiast
czytelnego „OPEN DRIVE"). To dokładnie ten sam rozjazd ABI fontów, który
zlokalizowałem w §47: wartość jest zapisywana poprawnie, ale program odbiera ją
przesuniętą o jedno słowo — przy tekście oznacza to przesunięte i pomieszane
znaki. To jest następna rzecz do naprawy i mam na nią powtarzalny pomiar.


## 51. Skrót do LDW na pulpicie + własna ikona programów TOS

### Skrót

Pulpit czyta `\SYSTEM\LAUNCH.CFG` (jedna linia na wpis, `ETYKIETA=ścieżka`,
`*TERMINAL`/`*TRASH` to wbudowane). Dopisany wpis:

```
LDW=\SYSTEM\TEST\LDW.PRG
```

`.PRG` uruchamiane z tego wpisu idzie przez warstwę zgodności TOS w swoim oknie
WM — czyli jest to dokładnie skrót do LDW Power na pulpicie.

### Nowa ikona programów TOS

* `system/ICONS/tos_prg.png` + `tos_prg_C.png` — okno GEM z paskiem tytułu,
  trzema przyciskami, linią tekstu i plakietką dyskietki (program wykonywalny),
  w tym samym motywie „glossy blue" co pozostałe 48 ikon.
* Mapowanie w `ic_id_for()` (system/desktop_wm.c):
  * `ext:PRG/TOS/TTP/GTP` → `ICON_TOS_PRG` (wcześniej wszystkie były zębatką
    `ICON_SYSTEM`, czyli „nieznana aplikacja"),
  * `app:...PRG` bez własnego wpisu → `ICON_TOS_PRG` (dotyczy TOSMONO, TOSCOL
    i każdego skrótu do programu TOS),
  * `app:\SYSTEM\TEST\LDW.PRG` → `ICON_KALKULATOR` (LDW Power to arkusz —
    dostał ikonę kalkulatora/arkusza).

### Ważna poprawka procesu: stabilne ID ikon

`tools/mk_icons_color.py` sortował pliki alfabetycznie, więc **dodanie jednego
PNG przesuwało ID wszystkich następnych ikon** i mieszało cały pulpit (ta pułapka
już raz mnie kosztowała). Generator czyta teraz kolejność ID **z już
wygenerowanego `system/icons_color.h`** i dopisuje wyłącznie nowe nazwy na końcu.
Sprawdzone: ID 0..95 **bez zmian**, nowe na 96/97. Przy okazji pierwsza wersja tej
poprawki sama się pomyliła (bazę wzięła z uszkodzonego nagłówka) — dlatego
weryfikuję to teraz porównaniem listy ID, a nie „na oko".

### Nowe narzędzie: zapis plików do obrazu

`tools/img_put.py` — `install_boot_sys.py` przyjmuje tylko binaria Atari, więc
pliku tekstowego (`LAUNCH.CFG`, `ASSIGN.SYS`) nie dało się wgrać. Nowe narzędzie
zapisuje dowolny plik hosta do wskazanej ścieżki w obrazie AHDI FAT16 (wykorzystuje
istniejący łańcuch klastrów, aktualizuje rozmiar w wpisie katalogowym).

`make check = 0` (40 testów PASS). Zrzut `zrzuty/38-pulpit-ikony-LDW-TOS.png`:
pulpit z ikoną LDW i nowymi ikonami TOSMONO/TOSCOL. Obraz z gotowym pulpitem:
`work/desktop-icons.img`.


## 52. „Brak myszy, nie wklepuje danych, zwiecha" — atrapy zdarzeń myszy

### Przyczyna

```
case 21:   /* evnt_button */
case 22:   /* evnt_mouse  */
    ...  zerowanie wyjść, return 0
```

Programy, które pytają o mysz **pojedynczymi** funkcjami (a nie przez
`evnt_multi`), nigdy nie dostawały ani pozycji, ani przycisków. LDW czekał więc
na zdarzenie, którego nie mógł dostać — z zewnątrz wyglądało to jak zwiecha,
brak myszy i brak reakcji na klawiaturę.

Kontrakty wzięte z EmuTOS `desk/aesbind.h`:

* `evnt_button(clicks, mask, state, &mx, &my, &mb, &ks)` → 4 słowa wyjścia
  (`mx,my,mb,ks`), zwraca liczbę kliknięć,
* `evnt_mouse(flags, x, y, w, h, &mx, &my, &mb, &ks)` → 4 słowa wyjścia, zwraca
  dopasowane flagi (1 = wyszła z prostokąta, 2 = weszła).

### Wdrożone

* `evnt_button` czeka (z ograniczeniem, pompując wejście i kompozytor), aż stan
  przycisków `(mb & mask) == (state & mask)`, potem wypełnia `mx,my,mb,ks`
  i zwraca zdarzenie,
* `evnt_mouse` czeka na wejście/wyjście kursora z zadanego prostokąta i zwraca
  dopasowaną flagę,
* oba kończą się także przy braku okna (nie ma wtedy myszy), po przekroczeniu
  ~1 s oddają sterowanie, i respektują zamknięcie okna,
* liczniki diagnostyczne `kdbg_evbtn_n` / `kdbg_evmouse_n` pokazują, ile zdarzeń
  myszy naprawdę poszło do programu.

### Pomiar na prawdziwym kernelu (autostart LDW)

```
t(s) | evnt_button | evnt_multi | klawisze | fault
  55 |         624 |          0 |        0 |     8
  65 |        1458 |        165 |      286 |     0
  80 |        3216 |        876 |        1 |     0
```

`evnt_button` liczy **tylko obsłużone** zdarzenia, więc 3216 oznacza, że program
naprawdę dostaje stan myszy; `evnt_multi` i ścieżka klawiszy również działają.

### Nowy, prawdziwy problem wykryty przy okazji

`kernel_fault_vector = 8` to **Privilege Violation**: LDW wykonuje instrukcję
supervisora, a `Super()` (GEMDOS 0x20) jest wciąż atrapą, która tylko zwraca
wewnętrzny wskaźnik stosu (punkt 2 Waszego audytu). Na prawdziwym TOS `Super(0)`
zostawia **wywołującego w trybie supervisora** — u nas musi to ustawić zapisane
`SR` gościa w ramce wywołania. To następna poprawka.


## 53. Kontrakty zdarzeń AES poprawione — pomiar na prawdziwym kernelu

### Wdrożone (punkty 3 i 5a/5b audytu wejść)

1. **`evnt_keybd` (20)** zwraca teraz `scan<<8 | ASCII` (tak jak `evnt_multi`) i
   czeka na klawisz (z ograniczeniem, pompując wejście). Wcześniej zwracał samo
   ASCII, więc strzałka w górę dawała `0x0000` zamiast `0x4800`.
2. **`evnt_button` (21) / `evnt_mouse` (22)** piszą wyniki do `intout[1..4]`,
   a nie `[0..3]` — `intout[0]` jest **kodem powrotu** i `asf_tos_aesvdi`
   nadpisuje go po dispatchu (to było źródło wyniku `[1,90,1,4,21845]`).
3. **`evnt_mouse`**: flagi interpretowane zgodnie z kontraktem — **0 = czekaj na
   wejście** do prostokąta, **1 = czekaj na wyjście** (wcześniej traktowałem je
   jak maskę bitową 1/2, przez co przypadek 0 nigdy nie pasował).
4. **`evnt_button`** liczy zmiany stanu przycisków i respektuje żądaną liczbę
   kliknięć, zamiast zwracać pierwszy odczyt stanu.

### Dowód: sonda `TOSIN.PRG` (prawdziwy trap AES, nie wycinek kodu)

Nowy program `system/tosin.c` woła `evnt_button`, `evnt_mouse` i `evnt_keybd`
przez `trap #2` (`d0=0xC8`) i zapisuje wyniki pod stałym adresem `0x04004100`.
Hash jądra i sondy w obrazie sprawdzone **przed** przebiegiem (lekcja z §45–46):

```
zrodlo instalacji: 444432 B sha=46f6e61ec11545af
w obrazie        : 444432 B sha=46f6e61ec11545af   ZGODNE: True
TOSIN.PRG w obrazie zgodny: True
```

Wynik przebiegu:

```
evnt_button: out[0] (kod powrotu) = 1   -> zdarzenie przycisku zadzialalo
evnt_mouse : out[0] (kod powrotu) = 1   -> kursor byl w prostokacie, ENTER zadzialal
evnt_keybd : out[0] = 0x0000            -> klawisz wstrzyknieto ZA POZNO
```

Czyli **przypadek, który u audytu nie działał** (`evnt_mouse` z kursorem
wewnątrz prostokąta → brak zdarzenia) **działa** — zwraca zdarzenie.

### Co jeszcze nie jest potwierdzone

* Współrzędne z `evnt_button` w tym przebiegu to `out[1]=0`, `out[2]=520` —
  wartość 520 jest **poza obszarem klienta 640x400**, więc mapowanie pozycji
  kursora do wnętrza okna TOS wymaga jeszcze sprawdzenia (harness nie umie
  przesuwać myszy, więc kursor był tam, gdzie zostawił go start).
* `evnt_keybd` nie zdążył zobaczyć wstrzykniętego klawisza (sonda kończy się
  przed wstrzyknięciem) — do powtórzenia z klawiszem podanym wcześniej.
* Nadal otwarte z audytu: `Super()` (punkt 4), wspólna kolejka IOREC
  (punkt 2), ABI callbacku myszy + domyślne wektory (punkt 1), `objc_edit`,
  `form_do`, prawdziwe `wind_*` i `evnt_mesag` bez sztucznego `WM_REDRAW`
  (punkt 5).


## 54. Ogólny start programów TOS: gość w trybie supervisora na WŁASNYM stosie

### Co było źle (i dlaczego „Super()" tego nie naprawiał)

Audyt miał rację, że `Super()` nie przełącza trybu. Próba naprawy przez
przebudowę ramki powrotu na nowym stosie **zatrzymywała program** (sonda nie
przechodziła dalej, handler był wołany, ale wykonanie stawało). To jest dokładnie
ostrzeżenie z audytu: niepoprawne przełączenie stosu jest gorsze niż jego brak.

### Rozwiązanie: gość startuje w supervisora, ale na swoim stosie

`asf_tos_enter` ustawia teraz **oba** stosy na bufor programu:

```asm
    move.w #0x2500,%sr          /* tryb supervisora, IPL 5 */
    move.l %a6,%sp              /* a6 = stos programu (ten sam co USP) */
```

Kolejność jest istotna: najpierw S=1, potem ładowanie SSP (SSP jest zapisywalny
tylko w supervisora). Wcześniejszy eksperyment ustawiał **tylko** USP, więc przy
S=1 procesor dalej używał stosu jądra i program czytał basepage z niewłaściwego
miejsca (`A5=0` → zapis pod `$4`) — to była przyczyna tamtej porażki.

`Super()` (GEMDOS 0x20) jest teraz w C, na sprawdzonym czytaniu argumentów,
bez żadnej operacji na stosach:

```
Super(1)        -> -1   (prawda: jesteśmy w supervisora)
Super(0)/Super(n) -> wskaźnik stosu wywołującego
```

### Dowód: sonda `TOSSUP.PRG` (prawdziwy trap #1)

```
Super(1)                  : -1     <- tryb supervisora potwierdzony
sonda przechodzi CALA sekwencje (marker 0x600D600D osiagniety po 36 s)
Super() handler: 0 wywolan w asm (hook usuniety), obsluga w C
```

### Efekt na LDW: błąd uprawnień zniknął

```
t(s) | wywolan GEM | evnt_multi | fault
  30 |         792 |         68 |     0
  55 |        4734 |       1382 |     0
  80 |        8664 |       2692 |     0
```

Wcześniej §52 raportował `fault = 8` (Privilege Violation). Teraz **cały przebieg
ma fault = 0**, a `evnt_multi` obsługuje klawisze i mysz (2692 zdarzenia).
`make check = 0`, 40 testów PASS. Hash jądra w obrazie sprawdzony przed
przebiegiem (lekcja z §53).


## 55. „Pierwsze uruchomienie OK, po restarcie zwis" — flaga wejścia zostawała

### Przyczyna

`tos_input_busy` to strażnik niereentrantnego czytnika wejścia. Jest podnoszony na
wejściu do `tos_poll_input()` i opuszczany na końcu — ale program może **wyjść
z wnętrza** ścieżki wejścia: `tos_poll_mouse_owned()` woła **callback myszy
gościa**, ten może zawołać GEMDOS/AES, a tam `tos_exit_pending()` prowadzi do
`asf_tos_exit()`, które **nie wraca**. Wtedy flaga zostaje na **1**.

Skutek dokładnie taki, jak zgłosił użytkownik: pierwszy program działa, zamyka
się, a **każdy następny nie odczytuje ani myszy, ani klawiatury** — czyli
zachowuje się jak zawieszony. `tos_poll_input()` sam nie ma wczesnego `return`
(sprawdzone), więc źródłem było wyjście z wnętrza właściciela wejścia.

### Naprawa

Na starcie **każdego** przebiegu zerowane są: `tos_input_busy` oraz stan
przycisków myszy. `tos_vdi_release_screen()` jest już wołane przy wyjściu
(sprawdzone: linia 2941), podobnie jak przywrócenie wektorów i zwolnienie TPA.

### Dowód: dwa uruchomienia LDW w jednej sesji

Test: LDW → wymuszone zamknięcie okna → LDW ponownie.

```
t(s) | wywolan GEM | evnt_mouse | evnt_button | evnt_multi | fault
  45 |        1332 |        247 |           0 |          0 |     0
  55 |        2436 |        616 |           0 |          0 |     0
  65 |        3792 |       1068 |           0 |          0 |     0
  75 |        5040 |       1294 |           0 |          0 |     0
  80 |           - |          - |        5358 |          0 |     0
  95 |        6048 |          0 |        1816 |          0 |     0
 110 |           - |       6726 |           0 |       2042 |     0
 120 |        7596 |          0 |        2333 |          0 |     0
```

Oba przebiegi obsługują wejście (`evnt_mouse`, `evnt_button`, `evnt_multi`
rosną), a **fault = 0 przez cały czas** — czyli nie ma już ani błędu uprawnień
z §52, ani utraty wejścia po restarcie. Zero w kolumnie to artefakt odczytu w
trakcie przebiegu, nie zatrzymanie: licznik rośnie dalej w następnej próbce.

`make check = 0`, 40 testów PASS. Hash jądra w obrazie sprawdzony przed
przebiegiem.


## 56. Diagnoza z ŻYWEJ zawieszonej instancji + ABI callbacku myszy

### Odczyt z działającego, zawieszonego Hatari (nie z domysłów)

Korzystając z tego, że użytkownik zostawił okno otwarte, odczytałem pamięć
żywej instancji przez `hatari-debug memdump`:

```
000C2D55: 01 01 00 00 ...
   tos_active     = 1
   tos_input_busy = 1     <-- program siedzi W ŚRODKU ścieżki wejścia
```

Czyli zwis nie jest „programem, który nie rysuje", tylko **programem
uwięzionym w callbacku myszy**: `tos_poll_mouse_owned()` woła procedurę myszy
gościa i nigdy z niej nie wraca.

### Przyczyna: punkt 1 audytu (ABI callbacku myszy)

`asf_tos_call_mousevec` przekazywał w `A0` **adres kodu callbacku**, a spakowaną
liczbę pakietu w `D0`. TOS przekazuje w `A0` **wskaźnik do trzech bajtów
pakietu** (nagłówek `0xF8|przyciski`, potem signed dx, dy). LDW czyta argument
jako adres i dereferuje go — dostawał więc śmieci (a przy zapisie poprzedniego
wektora mógł zachować 0 i wykonać `JSR 0`).

### Naprawa

* `asf_tos_call_mousevec(fn, packet)`: `A0` = wskaźnik do pakietu, ten sam
  wskaźnik trafia też na stos (dla procedur pisanych w C), rejestry gościa
  zachowane,
* pakiet budowany w C: `mouse_pkt[0] = 0xF8|przyciski`, `[1] = signed dx`,
  `[2] = signed dy`; duże delty są **świadomie ograniczane** do ±127 (zamiast
  zawijania rzutowaniem, o co prosił audyt),
* **domyślny `mousevec`** (`asf_tos_default_mousevec`, samo `rts`) instalowany
  w KDBVECS przy każdym przebiegu — program, który zapisze stary wektor i
  zawoła go w łańcuchu, nie skoczy pod adres 0.

### Pomiar po naprawie

```
t(s) | active | input_busy | GEM  | fault
  56 |      1 |          1 |  702 |     0
  60 |      1 |          1 |  984 |     0
  64 |      1 |          0 | 1143 |     0
  68 |      1 |          1 | 1410 |     0
  76 |      1 |          0 | 2016 |     0
  80 |      1 |          0 | 2370 |     0
```

`tos_input_busy` **cyklicznie wraca do 0** (1 → 0 → 1 → 0), czyli ścieżka
wejścia jest zdrowa; w żywej, zawieszonej instancji było **stale 1**. `fault = 0`
przez cały przebieg. Hash jądra sprawdzony przed przebiegiem.

### Uwaga procesowa

Obraz podany użytkownikowi do testu miał kernele **z autostartem** LDW, więc
program startował sam, zamiast czekać na kliknięcie ikony — to mój błąd w
przygotowaniu testu, poprawiony: obraz `work/ldw-desktop.img` startuje do pulpitu.


## 57. KOBOLD 3.51: rozpakowany, zainstalowany, uruchomiony i zdiagnozowany

### Co zrobione

`kob351AC.zip` (414 KB) rozpakowany do `kob351e/`. Na obrazie `work/kobold.img`
zainstalowane: `\KOBOLD\KOBOLD_3.PRG` (192 547 B), `\KOBOLD\ICONS\KB3_ICON.RSC`
i `KB3CICON.RSC`, `CORRECT.PRG`, `TREE_CHK.PRG`, oraz wpis w `LAUNCH.CFG`
(`KOBOLD=\KOBOLD\KOBOLD_3.PRG` → ikona na pulpicie, z nową ikoną programów TOS).

### Wynik uruchomienia

Program **startuje i pracuje** (122 wywołania GEM, 27× `evnt_multi`), a potem
kończy się **dzieleniem przez zero** (wektor 5). Zrzut `zrzuty/39-KOBOLD-start.png`
pokazuje pulpit z ikoną KOBOLD; po wyjściu pulpit odmalowuje się czysto.

### Diagnoza z rejestrów w chwili faultu

```
PC = 0x0401AD02          (kod KOBOLD-a w TPA)
d0 = 0x000000B8   d1 = 0x000000D0   d2 = 1
d3 = 0x000000B8   d4 = 0x00000008   d5 = 0x000000D0
d6 = 0x00000000   <-- ZERO, kandydat na dzielnik
d7 = 0x00000000   <-- ZERO, kandydat na dzielnik
```

`d4 = 8` to szerokość komórki znaku, więc program pracuje na metrykach tekstu i
dzieli przez wartość, którą **jakieś zapytanie zwróciło jako zero**. Ślad
wywołań (ostatnie 64 to `AES op=114`, czyli `rsrc_obfix` na kolejnych obiektach)
pokazuje, że dzieje się to przy przetwarzaniu zasobów dialogu.

### Naprawa wykonana po drodze: `wind_get`/`wind_open`/`wind_set`

Audyt (punkt 5) mówił, że `wind_*` „zwracają sukces bez właściwej obsługi stanu".
Potwierdzone i naprawione:

* `wind_get` dla `WF_WORKXYWH` (pole 1) zwracał **stałą `0x0f`** zamiast
  prostokąta — teraz zwraca prawdziwy obszar roboczy, a selektory 2/3/4 to
  CURR/PREV/FULL, 7 = TOP, 8/9 = FIRST/NEXTXYWH, 10 = ICONIFY,
* `wind_open` i `wind_set` **zapisują** geometrię, więc późniejsze `wind_get`
  odpowiada spójnie,
* żaden prostokąt nie wychodzi zerowy (zerowa szerokość to właśnie zaproszenie
  do dzielenia przez zero).

Efekt: miejsce awarii **się zmieniło** (488 → 122 wywołań, inny PC) — czyli
wartości są teraz inne, ale jeszcze nie poprawne. To jest realna poprawa
zgodności, **nie** usunięcie przyczyny.

### Wniosek

KOBOLD — duży, wymagający program GEM — potwierdza dokładnie to, co przewidywał
audyt: dokładanie pojedynczych atrap przesuwa objaw, nie usuwa problemu.
Najbliższy konkretny krok to **ABI tekstu/fontów** (znany rozjazd o jedno słowo
z §47, tuż obok `vqt_fontinfo`/`vqt_width`); kierunkiem docelowym pozostaje
sesja zgodności TOS oparta o sprawdzone komponenty EmuTOS.
