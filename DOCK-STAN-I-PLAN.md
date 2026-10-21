# DOCK (dolny pasek) — CO JEST, A CO MA BYĆ

Dokument przekazania dla kolejnej sesji. Stan na 2026-10-20, commit `c554dd4`.
Powiązane punkty w `PRZEKAZANIE-2026-10-17.txt`: **10aw** (wymagania + projekt),
**10ax** (cache ikon 16 px), **10ay** (pasek w WM).

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
| stałe/stan | `#define DOCK_H 24`, `static uint8_t dock_open`, `dock_dirty`; liczniki `kdbg_dock_click`, `kdbg_dock_open`, `kdbg_dock_draws` |
| geometria | `dock_geometry()`: `y = screen_h - 26`, `x = 8`; zwinięty `w = 24`, rozwinięty `w = screen_w - 16` |
| rysowanie | `dock_draw()`: tło `NC_BLUE` (8bpp) / `WM_PAPER` (mono), „hamburger" (3 kreski) po lewej, **zaślepki slotów**: 4 po lewej (skróty), 3 po prawej (uruchomione) |
| przełączanie | `dock_click(x,y)` wołane przy **puszczeniu** przycisku (release), nie przy press |
| kolejność kompozycji | na końcu `kernel_wm_frame_tick()`, **po** oknach/dekoracjach → pasek jest NAD oknami; rysowany **co klatkę** (pulpit przemalowuje tło i zmazywał pasek) |

**Zmierzone:** klik 1→2→3, `open` 1→0→1, ~22 rysunki/s; zrzuty
`display-demo/dd2.png` (rozwinięty), `display-demo/w-golden-boot.png` (zwinięty).

**Złoty zrzut — świadoma zmiana:** pasek jest widoczny przy starcie (zwinięty),
więc wzorzec to teraz **`65e67b09f0058fb4157b45431dd4be2de3f41ebe9cdcd155ef2eccb83b03c807`**
(`display-demo/test-wall-custom.py`, `REF`). Poprzedni `0e821ede…` obowiązywał
bez paska.

---

## 3. CZEGO BRAKUJE (do zrobienia)

1. **Sloty-sk r óty (lewa strona)** — pasek nie czyta jeszcze `LAUNCH.CFG` i nie
   ma skrótów; sloty to zaślepki.
2. **Ikony 16 px w slotach** — `kgfx_icon_small_draw()` jest gotowe, ale **nikt
   go nie woła** (dlatego `gc-sections` wycina je z obrazu).
3. **Interakcja ze slotami** — brak kliknięcia (uruchom) i najechania (nazwa).
4. **Prawa strona** — brak listy uruchomionych aplikacji, zębatki i
   przywracania na wierzch.
5. **Usunięcie ikon u góry** — `task_draw()` (trójkąt listy okien) i
   `desk_draw()` (glyph dwóch monitorów) nadal się rysują.
6. **Kosmetyka** — brak podziału wizualnego (lewa/prawa), brak podpowiedzi.

---

## 4. JAK DOROBIĆ (konkretny plan, z gotowymi mechanizmami)

### Krok 3 — sloty-sk r óty z LAUNCH.CFG

**Rekomendacja: całość po stronie JĄDRA (WM).** Pasek jest w WM, więc nie ma
sensu angażować pulpitu i dodawać nowych opcodów.

- Format `\SYSTEM\LAUNCH.CFG`: linie `ETYKIETA=ścieżka` (jak w
  `desktop_wm.c:launch_load()`); `*` na początku wartości = builtin.
- Czytać przez `kfat_load_path()` (jądro ma FAT) — **po** inicjalizacji FAT,
  najlepiej leniwie przy pierwszym otwarciu paska.
- Maks. 8 slotów; etykieta ≤12 znaków (8x16 font).
- Rysowanie: `kgfx_icon_small_draw(x, y, icon_id)` w prostokącie slotu
  (`r.y+4`), obok etykiety lub tylko ikona + tooltip.
- Klik: `kernel_spawn_user_async(path, "", "\\", &pid)` (jądro uruchamia
  proces — ta sama funkcja, której używa boot pulpitu).
- Hover: jeśli wskaźnik w prostokącie slotu → narysować nazwę nad paskiem
  (tło + tekst), **tylko raz na zmianę**, żeby nie migało.

**Uwaga:** mapowanie etykieta→ikona istnieje tylko w pulpicie
(`ic_load()`/`\SYSTEM\ICONS.CFG`). Najprościej: dla skrótów użyć jednej ikony
„aplikacja" (`ICON_...) lub dodać do `LAUNCH.CFG` opcjonalne pole z numerem
ikony.

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
   `65e67b09…` (z paskiem). Każda zmiana UI = świadoma aktualizacja REF.

---

## 6. JAK TESTOWAĆ

```bash
cd display-demo
python3 test-wall-custom.py golden          # zrzut startowy == 65e67b09…
python3 test-icons-size.py boot48           # ikony 48 px, pomiar
# pasek: klik w (20, 580) zwija/rozwija; stan czytać z liczników jądra:
#   kdbg_dock_click, kdbg_dock_open, kdbg_dock_draws  (memdump, rd32)
```

Szybkie wstrzykiwanie myszy (7× szybsze niż stare): `Session.inject(x,y,btn)`
— jeden zapis do `kdbg_test_mouse` (patrz `10am`); `click_fast/dbl_fast` na
tym stoją. Klik w pasek działa od `y≈580` (prostokąt `y=574..598`).

**CI:** `.github/workflows/ci.yml` buduje `TOS=1` i `TOS=0` + testy hosta;
po każdej zmianie sprawdzić oba joby.

---

## 7. PLIKI, KTÓRE TRZEBA ZNAĆ

| plik | rola |
|---|---|
| `system/kernel_wm.c` | kompozytor, górna belka (`task_draw`, `desk_draw`), **dolny pasek** (`dock_*`), okna |
| `system/kernel_graphics.c` | ikony: główny cache + **mały cache 16 px**, `kgfx_icon_small_draw` |
| `system/kernel_sessions.c` | mysz/klawiatura, mailbox testowy (`kdbg_test_mouse`), pump |
| `system/desktop_wm.c` | pulpit: ikony, okno plików (z kolumną boczną „My Atari"), `LAUNCH.CFG` |
| `system/kernel_main.c` | start jądra, ładowanie ICONS.DAT + przygotowanie cache 16 px |
| `display-demo/stress_atos.py` | harness: `Session`, `inject`, `click_fast`, `rd32`, `screen_text` |
| `PRZEKAZANIE-2026-10-17.txt` | pełna historia sesji, punkty 10aw–10ay |

---

## 8. DEFINICJA „GOTOWE" DLA DOCKA

- [ ] pasek zwinięty = ikona po lewej; klik rozwija w prawo i zwija z powrotem,
- [ ] lewa strona: ≥3 skróty z `LAUNCH.CFG`, ikony 16 px, klik uruchamia,
      najechanie pokazuje nazwę,
- [ ] prawa strona: ikony uruchomionych aplikacji + mała zębatka, najechanie
      pokazuje nazwę, klik przywraca okno na pierwszy plan,
- [ ] ikony u góry (task/desk) usunięte,
- [ ] złoty zrzut zaktualizowany i stabilny w dwóch kolejnych bootach,
- [ ] CI zielone (TOS=1 i TOS=0).
