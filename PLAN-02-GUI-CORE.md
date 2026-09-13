# PLAN-02 — Rdzeń GUI

Cel: graficzne środowisko zgodne z TOS/EmuTOS, ale aplikacje działają
wielozadaniowo we własnych oknach; nowoczesny wygląd i szybkość.

## G1 — menedżer N okien — ZROBIONE

- `include/mono_wm.h`, `system/mono_wm.c`: WM_MAX=8, kolejność Z (`order[]`),
  fokus, `wm_open/wm_select/wm_cycle/wm_close/wm_hit/wm_motion/wm_top/
  wm_next_open`. Model czysty, bez zależności od jądra.
- `tests/test_mono_wm.c`: test hostowy (Z-order, fokus, cykl, pełny ekran,
  zamykanie, pojemność, 10000 iteracji drag/resize) — PASS.
- `system/mono_desktop.c`: desktop rysuje wszystkie okna w kolejności Z,
  pasek zadań dla N okien, trzecie okno wewnętrzne SYSTEM.
- `system/kernel_main.c`: `MONODESK.APP` jako czwarta usługa startowa
  (`SYSTEM.WM`) — auto-start po INIT/GFX/GUI.
- Demo: `atari-030-port/gui-core-nwindows.png` (SYSTEM, LICZNIK, NOTATKA).


## G2 — pasek menu EmuTOS + TASKS — ZROBIONE

- **Menu bar** wziety 1:1 z EmuTOS (`desk/desk_rsc.c`):
  `Desk` (Desktop info...), `File` (Open, Info/Rename..., Search...,
  New folder..., Close folder, Close top window, Cycle windows,
  Select all items, Set file mask..., Delete..., Format floppy...,
  Execute EmuCON, Shutdown), `View` (Show as icons/text, Sort by
  name/type/size/date, No sort, Size to fit), `Options` (Set background...,
  Install icon/application/devices, Desktop configuration...).
- **Ikonka TASKS** w prawym gornym rogu: rozwija liste uruchomionych
  programow (`1 SYSTEM / 2 LICZNIK / 3 NOTATKA *`), klikniecie przelacza.
- **Mysz**: obsluga w desktopie (klik w tytul menu, wybor pozycji, TASKS,
  przeciaganie okien, przycisk zamykania, zmiana rozmiaru). Klawiatura:
  F1-F4 menu, F5 TASKS, strzalki + Enter, Esc.
- **Naprawiony blad**: `static const` tablice **wskaznikow** w `.rodata`
  nie przezywaly zaladowania (aplikacja PC-relative) — `menus[].title` i
  `items[].text` byly smieciami. Dane menu przepisane bez wskaznikow
  (indeksy + literaly w kodzie).
- Zwiekszony budzet rysowania `GUI_DRAW_MAX` 32 -> 96 (menu + dropdown +
  okna nie miescily sie w 32).
- Zrzuty: `atari-030-port/gui-menu-emutos.png`, `gui-tasks-list.png`.


## G3 — wydajność: damage + clipping — ZROBIONE

Problem: każdy render kopiował cały bufor (32 KB) i rysował cały ekran
(tło 640x400 = 256 000 pikseli), a każdy ruch myszy ustawiał `dirty=1`.

Zmiany:
- **Jądro** (`kernel_graphics.c`): nowa komenda `GUI_CLIP` i clipping w
  `kgfx_draw_batch` — rysujemy tylko obszar zniszczeń (odpowiednik
  `wind_get WF_FIRSTXYWH` w EmuTOS AES). Usunięty podwójny bufor
  (32 KB kopii na klatkę).
- **Desktop** (`mono_desktop.c`): damage jako unia prostokątów. Zmiana
  treści okna odświeża tylko jego obszar; przeciąganie — sumę starego i
  nowego położenia. Ruch kursora nie wymusza już renderu.
- **Kursor**: bez zmian — to już XOR 16x16 w stylu EmuTOS
  (`vdi/vdi_mouse.c cur_display`), tani. ET4000 (jak każda VGA w trybie
  graficznym) **nie ma sprzętowego kursora sprite'owego**, a Shifter ST też
  nie — więc programowy XOR jest właściwą drogą. Wąskim gardłem był pełny
  render ekranu, nie sam kursor.

### Uwaga o myszy w Hatari

Mysz w GUI dziala dopiero, gdy emulator przechwytuje hosta: **`--grab`**
(albo AltGr+M w oknie). Bez tego ruch hosta nie trafia do emulowanego ST.
Sciezka w systemie jest poprawna: GUI service op 6 -> syscall 47 (trwale
przechwycenie) -> KMOUSE do desktopu -> `mouse()`.


### G3c — kursor plynny i wyglad okien (sesja naprawcza)

**Przyczyna skokow kursora** (znaleziona): `kernel_user_gui_keyboard_wake()`
wołało `kernel_user_yield()` **przed** obsługą myszy — scheduler wpuszczał
desktop do rysowania, więc wskaznik czekał na cały render i skakał o zebraną
delte. Naprawa: `kernel_user_tick_now()` (odczyt ticku bez oddawania
sterowania).

**Regression, którą sam wprowadziłem i naprawiłem**: próba 'pompy' kursora
zmieniła `kgfx_cursor_move()` tak, że po `kgfx_enter_desktop` (które chowa
kursor) wskaznik **nigdy** się nie pokazywał. Przywrócone oryginalne
`kgfx_cursor_move()`.

**Artefakty okien**: przy odświeżeniu treści niższego okna (`dirty>=2`)
zamalowywane było okno wyżej. Naprawa: po `content(i)` rysowane są też
wszystkie okna **nad** nim w kolejności Z.

**Wygląd okien**: aktywny pasek tytułu = czarne tło + biały tekst (czytelne w
mono), nieaktywny = białe tło + czarny tekst. Ramka i sizer bez zmian (styl
EmuTOS).

**Kursor na ET4000 sprzętowo?** Nie. ET4000 (VGA) nie ma kursora
sprite'owego — tylko kursor tekstowy, bezużyteczny w trybie graficznym;
Shifter ST też nie ma. Właściwa droga to programowy XOR 16x16 w stylu EmuTOS
`vdi/vdi_mouse.c` (tani). Wąskim gardłem był scheduler/render, nie rysowanie.

**Pozostaje (jeśli nadal rwie)**: 'pompa' kursora wołana wewnątrz pętli
rysowania (teraz bezpieczna, bo `kgfx_cursor_move` jest poprawione).


## G4 — rysowanie jak XaAES (źródła FreeMiNT) — ZROBIONE

Zbadane źródła: `xaaes/src.km/rectlist.c` (`build_rect_list`), `k_mouse.c`,
`c_mouse.c`. Sedno XaAES: **lista prostokątów = widoczny region okna**
(obszar minus okna leżące wyżej) + rysowanie tylko zniszczeń + łączenie
odświeżeń w czasie (`REDRAW_TIMEOUT`); kursor rysowany przez VDI
`v_show_c/v_hide_c` z save-under, niezależnie od okien.

Wdrożone u nas:
- **G4-1**: `rg_sub()` w desktopie — widoczny region okna (obszar minus okna
  wyżej w kolejności Z). Zmienione okno rysowane tylko w odsłoniętych
  prostokątach; okien wyżej **nie trzeba przerysowywać**.
- naturalna kolejność Z: okno SYSTEM otwierane pierwsze (na spodzie).
- **G4-2**: ramka rysowana jako 4 odcinki, każdy cięty do regionu (wcześniej
  ramka wchodziła w obszar okna wyżej).
- **G4-3**: batching — do 32 komunikatów na klatkę (odpowiednik
  `REDRAW_TIMEOUT`: wiele drobnych zmian trafia w jedno odświeżenie).
- clipping tekstu po znaku (z poprzedniego kroku).


### G5 — kursor: naprawa zagłodzenia pętli głównej — ZROBIONE

Objaw (od użytkownika): kursor rusza się **tylko** przy odświeżaniu okien.

Przyczyna: desktop śpi przez `pause_tick()` → `kernel_user_sleep()` → pętla
`kernel_user_yield()`. Pętla główna jądra (a więc i wywołanie obsługi myszy
`kernel_user_gui_keyboard_wake()`) **nie dostawała sterowania** między
klatkami — kursor aktualizował się tylko wtedy, gdy desktop akurat rysował.

Naprawa (wzorzec z XaAES: pakiety myszy czytane z kolejki):
- `kernel_user_gui_cursor_pump()` — czyta **jeden** pakiet myszy, przesuwa
  wskaźnik i zapamiętuje zdarzenie dla pętli komunikatów,
- wołana z **`kernel_user_yield()`**, czyli przy każdym oddaniu sterowania
  przez dowolny proces (a to dzieje się często), zamiast czekać na pętlę główną,
- `kernel_user_gui_keyboard_wake()` najpierw pompuje, potem doręcza komunikat.

Uwaga metodyczna: lokalnie **nie mogłem tego zweryfikować** — macOS blokuje
`CGEventPost` bez uprawnień Accessibility, więc syntetyczne ruchy/kliknięcia
myszy nie docierają do Hatari. Wnioski opieram na obserwacji użytkownika.


## G6 — GUI wprost na ET4000 (Shifter nie bierze udziału) — ZROBIONE

- **handoff**: nowe pole `video_card` (BIOS ustawia, gdy wykryje ET4000);
  wskaźnik `video_enter` już tam był.
- **kgfx, tryb liniowy 8bpp** (`g_linear`, `g_linear_stride`): piksel = bajt,
  bez planów i read-modify-write; paleta GUI→VGA (`g_linear_palette`).
- **kursor**: w trybie liniowym zapis/odtworzenie bajtów 16x16.
- **tło / wypełnienia / ramki / tekst**: ścieżka liniowa; blitter pomijany.
- **`kgfx_enter_desktop`**: gdy karta obecna → `video8_enter()` (640x480x8
  chain-4) i buforem GUI jest **VRAM karty `0xC00000`** (stride 640).
  Bez karty — dotychczasowy Shifter (fallback).
- **tekst**: wiersz glifu zapisywany **dwoma longwordami** (uwaga: m68k jest
  big-endian — pierwszy bajt to najbardziej znaczący) zamiast 128 operacji
  na piksel.
- **`--gfx-chip et4000w32i`**: Hatari obsługuje, ale akceleracji W32i
  (BitBlt/ROP) **nie emuluje** — tylko identyfikację. Na realnej karcie to
  byłby dodatkowy skok (sprzętowy Blit/Fill/Line).

## Ważne ustalenie o ścieżce obrazu

- GUI jądra rysuje do **ekranu ST** (`SCREEN_BASE=0x10000` /
  `desktop_framebuffer`, Shifter), a konsola BIOS/kernel pisze do **VRAM
  karty ET4000** (`video8`, `0xC00000`).
- Dlatego na karcie widać konsolę, a desktop widać na Shifterze
  (demo zrobione bez karty).
- Do decyzji: albo VOFA ma przełączać się na obraz ST (pass-through), albo
  GUI ma renderować do VRAM karty. To warunek „ładnego GUI na ET4000”.

## Kolejne kroki

- **G2**: nowoczesny motyw (cienie, zaokrąglenia, paleta kolorów) w
  `kgfx_emutos_window` / `kgfx_emutos_background`,
- **G3**: menu, okna dialogowe, pasek zadań z zegarem,
- **G4**: aplikacje TOS w oknach (GEMDOS/VDI/AES w oknie),
- **G5**: ikony pulpitu i menedżer plików.