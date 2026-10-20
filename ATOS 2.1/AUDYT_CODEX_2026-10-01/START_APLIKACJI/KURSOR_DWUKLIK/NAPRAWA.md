# ATOS 2.1 — naprawa transportu myszy, dwukliku i kontraktu ikon 48×48

Odpowiedź na `RAPORT.md` (audyt 3 października 2026). Kolejność zgodna z sekcją
„Kolejność pracy” raportu. Oba testy diagnostyczne (`test_mouse_edges.py`,
`test_icon48.c`) przechodzą; bramka projektu: 43 testy zielone.

## 1. Utrata kliknięć w warstwie sesji — NAPRAWIONE

**Było:** `handle_mouse()` nadpisywał pojedyncze `forward_buttons`
(`forward_buttons=buttons|(forward_buttons&0x1cu)`) i ustawiał jedną flagę
`forward_ready`. Cztery zbocza (13,0,13,0) w jednym `ksessions_pump()` dawały
jedno zdarzenie `12` — pulpit widział „puszczony”, nie dwa kliknięcia.

**Jest:** uporządkowana kolejka zdarzeń interakcji w `system/kernel_sessions.c`:

```c
#define KSESS_MOUSE_QUEUE 32u
typedef struct {uint32_t buttons,tick;int32_t x,y;uint8_t edge;} ksess_mouse_ev_t;
```

- Każde **zbocze przycisku** dostaje własny wpis (z własnym x/y i czasem).
- **Ruch** bez zmiany przycisków scala się z poprzedzającym wpisem ruchu, więc
  wędrujący wskaźnik nie wypycha zboczy ze skrzynki klienta.
- `ksessions_mouse_peek()` / `ksessions_mouse_consume()`: zdarzenie opuszcza
  kolejkę dopiero, gdy odbiorca został ustalony i ma miejsce w skrzynce.
- `ksessions_mouse_flush()` czyści kolejkę (przełączenie sesji, drenaż startowy).

**Położenie kursora jest osobno:** `mouse_x/mouse_y/mouse_buttons` dalej opisują
żywy wskaźnik i sterują prezentacją; kolejka to wyłącznie transport interakcji.

## 2. Routing i pojemność odbiorcy przed odbiorem — NAPRAWIONE

`kernel_user_gui_keyboard_wake()` (`system/kernel_exception.c`):

- Najpierw **peek**, potem ustalenie **rzeczywistego** odbiorcy (okno pod pozycją
  ZDARZENIA albo przechwycenie kliknięcia), a dopiero potem **jego** kontrola
  pojemności. Pełna skrzynka zostawia zdarzenie w kolejce — `UP` nie ginie
  cicho i przycisk nie zostaje „wiszący”.
- Współrzędne wiadomości pochodzą z **pozycji zdarzenia**, nie z bieżącej
  pozycji sesji: kliknięcie pozostaje związane ze swoim x/y, nawet gdy wskaźnik
  już odjechał.
- Prezentacja kursora nadal idzie za żywym wskaźnikiem (`kgfx_cursor_move`).

## 3. Protokół KMOU — NAPRAWIONE

- `value[1]` = pozycja **zdarzenia**, `value[3]` = **czas zdarzenia**
  (`kernel_user_hires_clock()`, ta sama jednostka co `hires()` klienta).
- `value[3]` zawierał licznik zgubionych zdarzeń; **żaden odbiorca KMOU go nie
  czytał** (sprawdzone: `ac.c`, `commander.c`, `desktop_wm.c`, `mono_desktop.c`,
  `setup_app.c` — wszystkie czytają tylko `value[1]` i `value[2]`), więc
  kontrakt został zmieniony świadomie. Licznik został w `gui_keyboard_dropped`.
- `pending_push()` w `desktop_wm.c`: przy przepełnieniu odrzucany jest
  **najstarszy KMOU ruchu** (bez zbocza), a gdy nie ma takiego — najstarszy wpis,
  ale z licznikiem `pending_dropped`. Żadnego cichego zgubienia `UP`.
- Stan dwukliku oddzielony od zaznaczenia: `dbl_icon`/`dbl_tick` zamiast
  `sel_icon`/`icon_click_tick`. Skasowanie zaznaczenia (puste tło, inna ikona,
  menu) nie niszczy pierwszego kliknięcia pary, a zaznaczenie z innej drogi nie
  jest brane za pierwszy klik. Próg liczony z czasu **zdarzenia**.

## 4. Kontrakt ikon 48×48 — NAPRAWIONE

Jedna wspólna definicja w `include/gui_draw.h`:

```c
#define GUI_ICON_COLOR_SIZE 48
#define GUI_ICON_COLOR_H    (GUI_ICON_COLOR_SIZE+16)
#define GUI_ICON_MONO_SIZE  32
#define GUI_ICON_MONO_H     (GUI_ICON_MONO_SIZE+16)
```

Ta sama umowa w:

| miejsce | było | jest |
|---|---|---|
| `gui_damage.h` (granice) | sztywne `32`/`48` | `GUI_ICON_COLOR_*` / `GUI_ICON_MONO_*` |
| `kgfx_icon_color()` min. szerokość | `if(w<40)w=40` | `w < ICON_SIZE+8` (56) |
| czyszczenie starych pozycji (drag) | `112,48` | `CELL_ERASE_W/CELL_ERASE_H` |
| siatka pulpitu | `84×52` | `CELL_W/CELL_H` z `GUI_ICON_COLOR_SIZE` |
| picker ikon | komórki 40 px | 56 px |

W `kernel_graphics.c` jest statyczna asercja kompilacyjna
(`ICON_SIZE==GUI_ICON_COLOR_SIZE`), żeby generator i kontrakt nie mogły się
po cichu rozjechać.

## 5. Hover — NAPRAWIONE

`paint_hover()` czyści i przerysowuje **tylko dwie** komórki (starą i nową), przez
nowy `draw_icon(i)`; wcześniej `draw_icons()` rysowało wszystkie ikony, więc
jeden `damage` rozciągał się na cały obszar ikon przy każdym ruchu wskaźnika.
`CELL_ERASE_W == CELL_W`, więc czyszczenie jednej komórki nie podcina sąsiada.

## Testy

| test | przed | po |
|---|---|---|
| `test_mouse_edges.py` (prawdziwe `handle_mouse`/`ksessions_mouse_event`) | 1 zdarzenie `12` | **4 zdarzenia 13,0,13,0** |
| `test_icon48.c` | `right=132 < 148`, `bottom=148 < 164` | **`92,100..148,164`** |
| bramka projektu (43 testy) | — | **OK** |

Zaktualizowane harnessy (sama hydraulika makiet, asercje bez zmian intencji):
`tests/test_sessions.c`, `tests/test_gui_focus_routing.py`,
`tests/test_gui_input_backpressure.py` — ostatni dodatkowo pinuje, że pozycja
zdarzenia (`321,123`) NIE jest podmieniana na żywą pozycję kursora (`600,390`).

## Weryfikacja w emulatorze

Hatari (TF536/ET4000, 800×600×8bpp): `F9` parkuje wskaźnik nad ikoną
„My Atari”, następnie realny `hatari-event doubleclick` (dwa szybkie zbocza
przycisku przez IKBD) otwiera okno **MY ATARI — DRIVES** z dyskami C: i D:.
Przed naprawą oba kliknięcia składały się w jedno zdarzenie „puszczony”, więc
okno się nie otwierało.

Zrzuty: `a0_desktop.png`, `a1_parked.png`, `a2_after_dbl.png`.

## Czego nie wykonano

- **Benchmarku opóźnienia kursora** (mediana/p95, kolor i mono) — raport sam go
  nie wykonał, a wymaga instrumentacji `kdbg` (czas wejścia → dostarczenia KMOU
  → prezentacji). Liczniki są już w miejscu (`mouse_q_dropped`,
  `pending_dropped`, `gui_keyboard_dropped`); brakuje scenariusza zbierającego
  próbki dla 100/250/450/900 ms i zajętej kolejki.
- Prób pełnej skrzynki odbiorcy **innego** niż `gui_keyboard_pid` na żywym
  systemie — pokryte testem jednostkowym (`test_gui_input_backpressure.py`).
- Mono: ścieżka `GUI_ICON` pozostaje 32×32 (ikony ROM, bez skalowania); zmiany
  kontraktu jej nie dotyczą.
