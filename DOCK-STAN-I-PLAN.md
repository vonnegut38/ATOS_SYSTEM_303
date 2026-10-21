# DOCK (dolny pasek) — CO JEST, A CO MA BYĆ

Dokument przekazania dla kolejnej sesji. Stan po **kroku 3 + menu PPM**: 2026-10-21.
Punkt wyjścia: commit `c554dd4` (krok 2).
Powiązane punkty w `PRZEKAZANIE-2026-10-17.txt`: **10aw** (wymagania + projekt),
**10ax** (cache ikon 16 px), **10ay** (pasek w WM), **10az** (krok 3: skróty),
**10ba** (jasnoszary pasek, unikalne ikony, menu PPM).

---

## 1. CZEGO CHCE UŻYTKOWNIK (wymagania, dosłownie)

- Pasek **nie jest stałym elementem pulpitu** — ma być „przezroczysty"/overlay.
- **Zwinięty** do **ikony po lewej stronie** (dolnej).
- **Klik rozwija go w prawo**; można go zwinąć albo tak zostawić.
- Wysokość: **podobna do górnej belki systemu (16 px) lub do 50% większa (24 px)**.
- **Lewa część**: skróty do aplikacji — można **dodać skrót i uruchomić** aplikację.
- **Prawa część**: ikony **uruchomionych** aplikacji, z **małym kółkiem zębatym** na ikonie.
- **Najechanie bez klikania** → pokazuje się **nazwa aplikacji**.
- **Klik** → **przywrócenie okna na ekran / na pierwszy plan**.
- Dodatkowo: **ikonkę u góry (task/desk) usunąć**, bo pasek przejmuje jej rolę.

---

## 2. CO JEST ZROBIONE (działa, sprawdzone)

### 2.1 Ikony 16 px (fundament, `10ax`)

| element | gdzie | stan |
|---|---|---|
| sekcja 16 px w `ICONS.DAT` | `tools/mk_icons_color.py` — `SIZES=[16,32,48,64]` | ✓ plik 232 389 B (16=13 935, 32=38 321, 48=67 327, 64=112 242 B) |
| drugi cache w jądrze | `system/kernel_graphics.c`: `kgfx_icons_small_prepare(blob,size)`, `kgfx_icons_small_ready()`, `kgfx_icon_small_draw(x,y,id)` | ✓ **98 ikon** w pamięci (`kdbg_icon_small=98`) |
| testy hosta | `tests/test_icons_dat_v5.py` (4 sekcje, rozmiary 16/32/48/64) | ✓ zielone |

Mechanizm: dekodowanie sekcji 16 px **tym samym** loaderem, kopia tabeli do
`icon_small_table[]`, **przejęcie** buforów (`icon_small_alloc_*`), powrót do
rozmiaru głównego. Wywołane **raz**, w gałęzi sukcesu ładowania ikon w
`kernel_main.c`.

### 2.2 Pasek w WM (`10ay`, `system/kernel_wm.c`)

| element | szczegóły |
|---|---|
| stałe/stan | `#define DOCK_H 24`, `static uint8_t dock_open`; liczniki `kdbg_dock_click`, `kdbg_dock_open`, `kdbg_dock_draws` |
| geometria | `dock_geometry()`: `y = screen_h - 26`, `x = 8`; zwinięty `w = 24`, rozwinięty `w = screen_w - 16` |
| rysowanie | `dock_draw()`: tło **`DOCK_BG` = indeks 15 palety (`#AAAAAA`, jasny szary)** w 8bpp / `WM_PAPER` (mono), „hamburger" (3 kreski, czarne na szarym); od kroku 3 po lewej skróty, po prawej 3 zaślepki (uruchomione aplikacje — krok 4) |
| przełączanie | `dock_press()`/`dock_release()`: press zapamiętuje slot, akcja przy **puszczeniu** w tym samym slocie |
| kolejność kompozycji | **na samym końcu** `kernel_wm_frame_tick()` — po oknach, dekoracjach i menu, przed kursorem → pasek jest NAD nimi; rysowany **co klatkę** (pulpit przemalowuje tło i zmazywał pasek) |

**Zmierzone:** klik 1→2→3, `open` 1→0→1, ~22 rysunki/s; zrzuty
`display-demo/dd2.png` (rozwinięty), `display-demo/w-golden-boot.png` (zwinięty).

**Złoty zrzut — świadome zmiany:** pasek jest widoczny przy starcie (zwinięty),
więc wzorzec to teraz **`5c1e5546f36edd91147a84a4c943193f2ef294f669705c3d64a0e4d0443a3e08`**
(`display-demo/test-wall-custom.py`, `REF`) — pasek **jasnoszary**. Poprzednie:
`65e67b09…` (pasek niebieski) i `0e821ede…` (bez paska).

### 2.3 Skróty z `LAUNCH.CFG` w WM (krok 3, 2026-10-21)

| element | szczegóły |
|---|---|
| wczytywanie | `dock_shortcuts_load()` leniwie, **przy pierwszym otwarciu** paska: najpierw `\SYSTEM\DOCK.CFG` (własna lista paska), a gdy go nie ma — `\SYSTEM\LAUNCH.CFG` (zgodność z krokiem 3 i pulpitem) |
| parser | linie `ETYKIETA=ścieżka`, `;`/`#` = komentarz, `*…` (builtiny pulpitu) pomijane; max. 8 skrótów, etykieta ≤12 znaków |
| ikona | **katalog `dock_catalog[]`**: każdy znany program/gra ma własną, sensowną ikonę (COMMANDER→`ICON_NC`, SYSINFO→`ICON_INFORMACJE`, TASKMGR→`ICON_KONFIGURACJA`, PLASMA→`ICON_OBRAZ`, ARKANOID→`ICON_ARKANOID`, FLASHBACK→`ICON_PLATFORMOWE`, SYSTOS→`ICON_TOS_PRG`, MODPLAY→`ICON_MUZYKA`, PRE2→`ICON_GRY`…). Dla nieznanych ścieżek fallback wg rozszerzenia; **`|NN`** na końcu wartości nadpisuje ikonę numerem z `ICONS.DAT` |
| rysowanie | `kgfx_icon_small_draw()` w każdym slocie (16 px, y+4), podświetlenie slotu pod myszą; mono = mały glyph okna |
| klik | press zapamiętuje slot, **release w tym samym slocie** uruchamia: `kernel_spawn_user_async()` dla `.APP`, `kernel_tos_start_async()` dla `.PRG/.TOS` (async, żeby nie zablokować pętli jądra) |
| hover | etykieta nad paskiem (`dock_tip_show`), rysowana tylko przy zmianie slotu; stary prostokąt oddawany jako damage |
| liczniki | `kdbg_dock_slots`, `kdbg_dock_launch`, `kdbg_dock_rc`; menu: `kdbg_dock_menu`, `kdbg_dock_add`, `kdbg_dock_remove`, `kdbg_dock_save_rc` |
| fallback | brak `DOCK.CFG` i pusty `LAUNCH.CFG` → `dock_short_defaults()`: COMMANDER, SYSINFO, TASKMGR |

**Zmierzone (2026-10-21):** `kdbg_dock_slots=3` dla testowego configu
(COMMANDER + `*TERMINAL` pominięty + SYSINFO|24 + TASKMGR), ikony 50/24/40;
klik w slot 0 → `kdbg_dock_launch=1`, `kdbg_dock_rc=0`, okno AC.APP na ekranie;
hover rysuje i zmazuje „COMMANDER”. Zrzuty: `display-demo/dock3-open.png`,
`dock3-tip.png`, `dock3-launch.png`.

**Złoty zrzut:** `5c1e5546…` BAJT W BAJT (jasnoszary pasek; skróty wczytują się
dopiero po otwarciu, więc boot jest stabilny).

### 2.4 Menu kontekstowe paska (PPM, 2026-10-21)

| element | szczegóły |
|---|---|
| otwarcie | **prawy przycisk** na pasku (zwiniętym lub otwartym) → popup nad paskiem; PPM na slocie daje dodatkowo „USUN: &lt;nazwa&gt;” |
| pozycje | „USUN: &lt;nazwa&gt;” (tylko slot), „DODAJ POZYCJE...” (katalog), „ZAMKNIJ”; w liście dodawania: przewijanie „DALEJ &gt;”/„&lt; WSTECZ” i „ANULUJ” |
| katalog | `dock_catalog[]` — 16 programów/gier; lista pokazuje tylko nieobecne na pasku, z ikonami |
| zapis | `\SYSTEM\DOCK.CFG` (własny plik paska; **`LAUNCH.CFG` pulpitu zostaje nietknięty**), po zapisie `kblock_cache_clear()` i natychmiastowy reload — pasek zmienia się od razu |
| modalność | przy otwartym menu `kernel_wm_dock_at()` zwraca 1 dla **każdego** punktu; router (`kernel_exception.c`, `dock_wm_capture`) zjada zdarzenie, więc klik poza menu tylko je zamyka i nie klika okna pod spodem |
| rysowanie | `dock_menu_paint()` — ramka `WM_INK`, biały spód, niebieski pasek tytułu, czarne napisy, podświetlenie wiersza `NC_BRIGHT`; 16 px ikony obok pozycji; rysowane po `dock_draw()`, pod kursorem |
| liczniki | `kdbg_dock_menu` (otwarcia), `kdbg_dock_add`/`kdbg_dock_remove`, `kdbg_dock_save_rc` (0 = zapis OK), `kdbg_dock_menu_draws` |
| test | `display-demo/test-dock-menu.py` — PPM, dodanie PRE2, usunięcie COMMANDER, zapis `DOCK.CFG`, brak menu pulpitu pod spodem |

**Zmierzone:** start 3 pozycje → DODAJ PRE2 = 4 (`add=1`, `save_rc=0`) →
USUN COMMANDER = 3 (`remove=1`, `save_rc=0`); `DOCK.CFG` po zakończeniu:
`SYSINFO|24`, `TASKMGR|40`, `PRE2|22`. Zrzuty: `display-demo/dockmenu-main.png`,
`dockmenu-add.png`, `dockmenu-added.png`, `dockmenu-removed.png`.

**Uwaga:** menu zmienia listę **docka** (`DOCK.CFG`), a nie ikony pulpitu
(`LAUNCH.CFG`). To świadome rozdzielenie — pulpit ma własne builtiny
`*TERMINAL`/`*TRASH`, których nie chcemy nadpisywać.

---

## 3. CZEGO BRAKUJE (do zrobienia)

**Zrobione w kroku 3 + menu** (było na tej liście): skróty z `LAUNCH.CFG`,
unikalne ikony 16 px dla programów/gier, klik uruchamiający, podpowiedź z nazwą,
menu PPM z dodawaniem/usuwaniem pozycji (`DOCK.CFG`).

1. **Prawa strona** — brak listy uruchomionych aplikacji, zębatki i
   przywracania na wierzch (krok 4).
2. **Usunięcie ikon u góry** — `task_draw()` (trójkąt listy okien) i
   `desk_draw()` (glyph dwóch monitorów) nadal się rysują (krok 5).
3. **Kosmetyka** — brak podziału wizualnego lewa/prawa; prawe sloty to nadal
   zaślepki, a podpowiedź działa tylko dla lewych skrótów.

---

## 4. JAK DOROBIĆ (konkretny plan, z gotowymi mechanizmami)

### Krok 3 — sloty-sk r óty z LAUNCH.CFG — **ZROBIONE (2026-10-21)**

Całość po stronie **JĄDRA (WM)** — bez nowych opcodów i bez angażowania pulpitu.
Tak jak zalecał plan; poniżej stan faktyczny:

- Format `ETYKIETA=ścieżka` (parser jak w `desktop_wm.c:launch_load()`); `*` na
  początku wartości = builtin **pomijany** (nie ma czego spawnować). Czytanie:
  najpierw `\SYSTEM\DOCK.CFG` (własna lista paska), potem `\SYSTEM\LAUNCH.CFG`;
  **leniwie, przy pierwszym otwarciu** (`dock_shortcuts_load()`).
- Maks. 8 slotów; etykieta ≤12 znaków. Slot 24 px: ikona 16 px na `y+4`
  (`kgfx_icon_small_draw`), bez etykiety w slocie (etykieta = tooltip).
- Ikona: **katalog `dock_catalog[]`** (ścieżka→ikona) daje każdemu programowi
  i grze własną ikonę; dla nieznanych ścieżek fallback wg rozszerzenia.
  **`|NN`** na końcu wartości nadpisuje ikonę numerem z `ICONS.DAT`, np.
  `SYSINFO=…\SYSINFO.APP|24`. Ten sam katalog zasila listę „DODAJ POZYCJE...”.
- Klik: press zapamiętuje slot, **release w tym samym slocie** uruchamia.
  `.APP` → `kernel_spawn_user_async()`; `.PRG/.TOS/.TTP/.GTP` →
  `kernel_tos_start_async()` (wersja async; `kernel_tos_run_program()`
  zablokowałoby pętlę jądra razem z paskiem). W `TOS=0` `.PRG` zwraca `-0x2ff`.
- Hover: `dock_tip_show()` rysuje nazwę nad paskiem, **tylko przy zmianie
  slotu**; po zjechaniu stary prostokąt idzie jako `wm_damage_add()`, więc tło
  wraca. Podświetlenie slotu pod myszą (8bpp: `NC_BRIGHT`).
- Brak `DOCK.CFG` i pusty `LAUNCH.CFG` → 3 domyślne skróty (COMMANDER,
  SYSINFO, TASKMGR); `DOCK.CFG` z samym nagłówkiem = świadomie pusty pasek.

**Pułapka odkryta przy tym kroku:** sama zmiana `dock_open` **nic nie
przerysowuje**. `kernel_wm_frame_tick()` kończy się na
`if(!wm.damage_count){kvideo_flush();return;}`, a mysz nie generuje damage'u
WM. Dlatego `dock_release()` dodaje damage starego i nowego prostokąta paska +
`wm_force_compose=1`, a `dock_hover_poll()` (wołany co klatkę po
`cursor_sync()`) wymusza kompozycję, gdy zmieni się slot pod wskazaniem.
Bez tego pasek „otwierał się” tylko w licznikach (`open=1`), a na ekranie
zostawała zwinięta ikonka, a `dock_short` w ogóle się nie wczytywał.

**Uwaga:** mapowanie ścieżka→ikona jest teraz **wspólne** dla paska i menu
(`dock_catalog[]` w `kernel_wm.c`). Pulpit ma własne `ic_id_for()`/
`\SYSTEM\ICONS.CFG`; pasek nie czyta tej mapy, żeby nie dodawać I/O na starcie.

### Krok 3b — menu kontekstowe paska (PPM) — **ZROBIONE (2026-10-21)**

- PPM na pasku: `dock_right_click()` → `dock_menu_open_at()`. Na slocie menu ma
  „USUN: &lt;nazwa&gt;”, zawsze „DODAJ POZYCJE...” (katalog) i „ZAMKNIJ”.
- PPM jest **modalne**: `kernel_wm_dock_at()` zwraca 1 dla każdego punktu, gdy
  menu jest otwarte, a router w `system/kernel_exception.c`
  (`dock_wm_capture`) zjada zdarzenie — menu pulpitu pod spodem się nie otwiera,
  a klik poza menu tylko je zamyka.
- „DODAJ POZYCJE...” listuje katalog bez pozycji już obecnych, 7 na stronę
  (`DALEJ >` / `< WSTECZ` / `ANULUJ`). Dodanie/Usunięcie zapisuje
  **`\SYSTEM\DOCK.CFG`** i natychmiast przeładowuje pasek
  (`dock_shortcuts_save/reload`). `LAUNCH.CFG` pulpitu zostaje nietknięty.
- Po zapisie `kblock_cache_clear()` — bez tego następny odczyt widzi stary plik
  (trap 7 z tej listy).
- Menu rysuje `dock_menu_paint()` po `dock_draw()`: ramka, biały spód, niebieski
  tytuł, czarne napisy, ikony 16 px przy pozycjach, podświetlenie wiersza pod
  myszą. Test: `display-demo/test-dock-menu.py`.

### Krok 4 — uruchomione aplikacje (prawa strona)

Wszystko dostępne **w jądrze**, bez syscalli:

- lista procesów: `process_table[]` (jądro) albo syscall 20 +
  `gui_call(68,...)` — w WM prościej bezpośrednio z tablicy procesów
  (`kernel_user_process_info/name` w `kernel_module.c`).
- **przywrócenie na wierzch**: znaleźć okno, którego właścicielem jest pid
  (`window_owner_pid(i) == pid`) i wywołać `wm_raise(&wm, i)` + damage.
  (`foreground_user_process(pid)` istnieje w API modułu, ale **nie ma numeru
  syscalla** — w WM nie jest potrzebne.)
- **zębatka**: kilka pikseli w prawym górnym rogu ikony slotu (`wm_fill_rect`).
- kolejność: uruchomione od prawej do lewej, maks. 5 slotów.

### Krok 5 — usunięcie ikon u góry

- `task_draw()` — rysuje trójkąt listy okien; **usunąć rysowanie** (zostawić
  funkcję/maszynerię dropdownu, bo jest używana przy hit-teście).
- `desk_draw()` — glyph dwóch monitorów; **usunąć rysowanie**; przełączanie
  desków zostaje na `Shift+Alt+F10` i w menu sesji (prawy przycisk).

---

## 5. PUŁAPKI Z TEJ SESJI (oszczędzą czas)

1. **Przełączanie na release, nie na press.** Sam „wiszący" press przy starcie
   przełączał pasek i złoty zrzut był niestabilny (`3319e208` vs `65e67b09`).
2. **Rysować pasek co klatkę.** Pulpit przemalowuje tło i zmazywał pasek;
   rysowanie „na brudno" zostawiało dziury. Przy zwiniętym to 24×24 px — tanio.
3. **Pasek musi rysować WM, nie pulpit.** Pulpit jest POD oknami.
4. **Aplikacje GUI nie są relokowane** — tablica `const char *const` trzyma
   adresy z linkowania i wskazuje w próżnię. Używać tablic znaków w linii
   (`char [N][M]`), jak `user_short_labels[3][12]`.
5. **Limit PC-relative (±32 KB)** dla GUI apps — każdy dodany kawałek tekstu
   może przesunąć dane poza zasięg (`relocation truncated to fit: R_68K_PC16`).
   Wtedy: usuwać martwy kod, nie zwiększać DBL_TICKS itp.
6. **FAT 8.3 = 8 znaków** — `DOCUMENTS`/`DOWNLOADS` (9) nie przechodzą.
7. **Po zapisach do FAT czyścić cache** (`kblock_cache_clear()`), inaczej
   kolejny odczyt widzi nieaktualne dane (to zepsuło listę ikon pulpitu).
8. **Złoty zrzut** (`test-wall-custom.py golden`) — aktualny REF:
   **`5c1e5546…`** (jasnoszary pasek). Każda zmiana UI = świadoma aktualizacja REF.
9. **Zmiana stanu paska nie przerysowuje się sama.** `kernel_wm_frame_tick()`
   wychodzi przy `!wm.damage_count`; po `dock_release()` trzeba dodać damage
   (stary + nowy prostokąt) i `wm_force_compose=1`. Hover wymaga tego samego —
   dlatego `dock_hover_poll()` jest wołany co klatkę po `cursor_sync()`.
   Bez tego `kdbg_dock_open=1`, a ekran dalej pokazuje zwiniętą ikonkę.
10. **Klik w pasek nie może przelatywać do okna pod spodem.** `dock_press()`
    sprawdza cały prostokąt paska (`dock_inside()`), także puste miejsce;
    akcję wykonuje dopiero release w tym samym slocie co press.
11. **`.PRG` z paska idzie przez TOS async.** `kernel_tos_run_program()`
    blokuje pętlę jądra (a z nią rysowanie paska); używać
    `kernel_tos_start_async()` (tylko `CONFIG_TOS=1`).
12. **Pasek rysować po dekoracjach okien.** `dock_draw()` musi być na samym
    końcu kompozycji (po `overlay_blit()`); wcześniej stał przed pętlą
    dekoracji i dolna ramka/uchwyt okna zasłaniały pasek.
13. **PPM na pasku trzeba przechwycić w routerze.** Sam `dock_right_click()`
    w WM nie wystarczy: zdarzenie poleciałoby też do pulpitu i otworzyło jego
    menu pod spodem. `kernel_wm_dock_at()` + `dock_wm_capture` w
    `kernel_exception.c` zjadają cały gest; przy otwartym menu funkcja zwraca 1
    dla każdego punktu (modalność).
14. **Własny plik `DOCK.CFG`.** Zapisywanie `LAUNCH.CFG` z jądra gubiłoby
    builtiny pulpitu (`*TERMINAL`, `*TRASH`) i komentarze. Menu paska zapisuje
    `\SYSTEM\DOCK.CFG`, a pasek czyta go przed `LAUNCH.CFG`; po zapisie
    `kblock_cache_clear()` + natychmiastowy reload.
15. **Jedna mapa ikon.** `dock_catalog[]` (ścieżka→ikona) zasila i sloty, i
    listę „DODAJ”; bez tego każdy `.APP` dostawał ten sam `ICON_SYSTEM`
    i pasek wyglądał jak rząd identycznych zębów.

---

## 6. JAK TESTOWAĆ

```bash
cd display-demo
python3 test-wall-custom.py golden          # zrzut startowy == 5c1e5546…
python3 test-icons-size.py boot48           # ikony 48 px, pomiar
python3 test-dock-step3.py                  # krok 3: DOCK.CFG/LAUNCH.CFG, ikony, hover, start
python3 test-dock-menu.py                   # PPM: DODAJ PRE2, USUN COMMANDER, DOCK.CFG
# pasek: klik w (20, 580) zwija/rozwija; PPM otwiera menu konfiguracji.
# Stan czytać z liczników jądra:
#   kdbg_dock_click, kdbg_dock_open, kdbg_dock_draws,
#   kdbg_dock_slots, kdbg_dock_launch, kdbg_dock_rc,
#   kdbg_dock_menu, kdbg_dock_add, kdbg_dock_remove, kdbg_dock_save_rc
```

`test-dock-step3.py` sprawdza też **pamięć jądra**: `dock_short` (etykiety,
ścieżki, ikony — m.in. `SYSINFO|24` i pominięty `*TERMINAL`), liczbę slotów
oraz to, że klik w puste miejsce paska nie przechodzi do okna pod spodem.
Zrzuty: `dock3-closed/open/tip/launch.png`.

`test-dock-menu.py` otwiera PPM na slocie, dodaje PRE2 z katalogu, usuwa
COMMANDER i po zamknięciu Hatari czyta `\SYSTEM\DOCK.CFG` z obrazu. Zrzuty:
`dockmenu-main/add/added/removed/empty.png`.

Szybkie wstrzykiwanie myszy (7× szybsze niż stare): `Session.inject(x,y,btn)`
— jeden zapis do `kdbg_test_mouse` (patrz `10am`); `click_fast/dbl_fast` na
tym stoją. Klik w pasek działa od `y≈580` (prostokąt `y=574..598`);
PPM = `inject(x,y,2)` + `inject(x,y,0)`.

**CI:** `.github/workflows/ci.yml` buduje `TOS=1` i `TOS=0` + testy hosta;
po każdej zmianie sprawdzić oba joby.

---

## 7. PLIKI, KTÓRE TRZEBA ZNAĆ

| plik | rola |
|---|---|
| `system/kernel_wm.c` | kompozytor, górna belka (`task_draw`, `desk_draw`), **dolny pasek** (`dock_*`), **menu PPM** (`dock_menu_*`), okna |
| `system/kernel_graphics.c` | ikony: główny cache + **mały cache 16 px**, `kgfx_icon_small_draw` |
| `system/kernel_sessions.c` | mysz/klawiatura, mailbox testowy (`kdbg_test_mouse`), pump |
| `system/kernel_exception.c` | router myszy: `dock_wm_capture` — PPM/klik w pasek nie trafia do aplikacji |
| `system/desktop_wm.c` | pulpit: ikony, okno plików (z kolumną boczną „My Atari"), `LAUNCH.CFG` |
| `system/kernel_main.c` | start jądra, ładowanie ICONS.DAT + przygotowanie cache 16 px |
| `display-demo/stress_atos.py` | harness: `Session`, `inject`, `click_fast`, `rd32`, `screen_text` |
| `display-demo/test-dock-step3.py` | test kroku 3: otwarcie, `dock_short`, ikony, hover, start z paska |
| `display-demo/test-dock-menu.py` | test PPM: dodaj/usuń, katalog, zapis `DOCK.CFG` |
| `PRZEKAZANIE-2026-10-17.txt` | pełna historia sesji, punkty 10aw–10ba |

---

## 8. DEFINICJA „GOTOWE" DLA DOCKA

- [x] pasek zwinięty = ikona po lewej; klik rozwija w prawo i zwija z powrotem,
- [x] lewa strona: ≥3 skróty z `DOCK.CFG`/`LAUNCH.CFG`, **unikalne ikony 16 px**,
      klik uruchamia, najechanie pokazuje nazwę (`test-dock-step3.py` zielony),
- [x] **PPM na pasku: menu „DODAJ POZYCJE...”/„USUN: ...”/„ZAMKNIJ”**,
      zapis do `DOCK.CFG` i natychmiastowe odświeżenie (`test-dock-menu.py` zielony),
- [ ] prawa strona: ikony uruchomionych aplikacji + mała zębatka, najechanie
      pokazuje nazwę, klik przywraca okno na pierwszy plan (krok 4),
- [ ] ikony u góry (task/desk) usunięte (krok 5),
- [x] złoty zrzut stabilny w dwóch kolejnych bootach: **`5c1e5546…`** BAJT W BAJT
      (jasnoszary pasek; zmiana koloru była świadoma),
- [ ] CI zielone (TOS=1 i TOS=0) — lokalnie oba profile budują się i testy
      hosta przechodzą; workflow trzeba potwierdzić na GitHubie.
