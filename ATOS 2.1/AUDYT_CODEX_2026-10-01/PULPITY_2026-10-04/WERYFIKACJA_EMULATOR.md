# Weryfikacja patchy `pulpity-kandydat` w emulatorze — WYNIK: DZIAŁA

Weryfikacja przeprowadzona na **moim** drzewie roboczym (48×48 ikony, motywy,
siatka `GRID_X 28`, moja diagnostyka). Patch zastosowany zgodnie z instrukcją.

## 1. Zgodność bazy

`source-manifest.json` — wszystkie trzy hashe `base_sha256` **zgadzają się co do
znaku** z plikami w drzewie, mimo że od czasu audytu doszły: ikony 48×48 z
`ICONS 8`, paleta 216 wpisów, motywy i przesunięcie siatki. `patch -p1 --dry-run`
przeszedł czysto, potem zastosowany.

## 2. Test dostarczony przez audyt

```
python3 test_desks.py .
session=1 visible canvas=2 expected=2
session=0 visible canvas=1 expected=1
session=1 visible canvas=2 expected=2
session=0 visible canvas=1 expected=1
autostart early=0 after switch=1 (expected 0,1)
```

Przechodzi ✓ — przed poprawką druga sesja nie rysowała żadnej powierzchni.

## 3. Weryfikacja całego systemu w Hatari

**Add Desk w bezruchu — GŁÓWNY BŁĄD USUNIĘTY.** Drugi pulpit **wyświetla się
w całości**: grafitowe biurko, stalowy pasek, wszystkie ikony.

Liczniki (odczyt z pamięci jądra) przed → po:

| licznik | przed | po |
|---|---:|---:|
| `kdbg_desk_set` (Add Desk) | 0 | **1** |
| `kdbg_desk_take` (zabrane po przełączeniu) | 0 | **1** |
| `kdbg_desk_ok` / `_fail` (spawn) | 0 / 0 | **1 / 0** |
| `kdbg_desk_rejok_s[]` (sesja rejestracji) | [1,0,0] | **[1,1,0]** |
| `kdbg_items_ok[]` (narysowane elementy) | [39,0,0] | **[62,38,0]** |
| `kdbg_batch_ok[]` | [2,0,0] | **[3,2,0]** |

Czyli: sesja 1 tworzy się, DESKTOP.APP startuje **w niej**, rejestruje się jako
pulpit sesji 1 i **rysuje 38 elementów** — pełny pulpit. Pasek pokazuje numerek
**„2"**. Ekran: `(16,32,32)` × 456 093 px (przed poprawką było `(0,0,0)` × 481 033).

**Powrót na pulpit 1 — działa** (ALT+SHIFT+F10): numerek wraca na „1", biurko bez
zmian.

## 4. NOWY defekt znaleziony w tej weryfikacji: menu nie wstaje na pulpicie 2

Na pulpicie 1 klik w ikonkę dwóch monitorków otwiera listę (`Desk 1 / Add Desk`,
lista rysowana 3×). **Na pulpicie 2 ten sam klik nie dociera do `desk_click`** —
licznik `kdbg_desk_menu` zostaje na 1, choć pętla kompozytora **działa**
(`kdbg_frames` +33 869 w trakcie testu).

To **nie jest** defekt kompozycji ani klipów — te są już naprawione. To ścieżka
**routingu/ przechwycenia kliknięcia** po przełączeniu sesji, czyli dokładnie
punkt 2 z „Rozbudowa nie jest jeszcze spójna" w RAPORCIE: *„Przełączenie musi
anulować przechwycenie i nakłady starej sesji"*. Podejrzane: stan `drag_active` /
`rsz_active` / `mouse_capture` albo `pressed` w `kernel_wm_frame_tick`.

Wpływ: na drugim pulpicie **nie da się przełączyć z powrotem myszką** (klawisz
ALT+SHIFT+F10 działa). Do naprawy przed uznaniem wielu pulpitów za gotowe.

## 5. Dodane stałe liczniki diagnostyczne

`kdbg_desk_menu` (kliknięcia, które dotarły do ikonki) i `kdbg_desk_list_draws`
(rysowania listy) — obok wcześniejszych `kdbg_desk_*`, `kdbg_batch_ok/rej[]`,
`kdbg_items_ok[]`. Dzięki nim rozdzieliłem „awaria startu" od „awaria kompozycji"
i od „awaria routingu kliknięcia".

## 6. Stan

- Bramka **43 testy zielone**, `test_desks.py` zielony, zero kodu TEMP.
- Klawisze F pulpitu przywrócone do stanu wyjściowego.
- Otwarte punkty z raportu **nadal otwarte**: operacje aplikacji w tle (bramka
  odrzuca rysowanie do własnej powierzchni), globalne stany pojedynczego pulpitu,
  filtrowanie dekoracji/listy zadań, limit slotów i komunikat o niepowodzeniu
  Add Desk.

---

# Jedna instancja programu na cały system (na życzenie użytkownika)

## Co jest zrobione

`launch_run()` w pulpicie sprawdzał tylko **własną** listę (`launch[index].pid`), więc
dwa pulpity mogły uruchomić tę samą aplikację dwa razy — a druga kopia walczyła
z pierwszą o sloty procesów, sloty okien i ekran.

Dołożone:

- **Jądro, wywołanie 66** (`activate-or-report`): dostaje ścieżkę aplikacji i zwraca
  `0` = nie działa (klient uruchamia), `1` = działa w tej sesji (klient pokazuje okno),
  `2` = działa na innym pulpicie — **jądro samo przełącza sesję**, bo tylko ono może.
  W `st[0]`/`st[1]` zwraca pid i sesję.
- **`user_find_running_app()`** — szuka żywej instancji po nazwie 8.3, korzystając
  z tych samych helperów co TASKMGR (`kernel_user_process_name/info`).
- **Pulpit** pyta jądro **przed** uruchomieniem i przy `2` pokazuje
  „SWITCHED TO RUNNING", a przy `1` przechodzi do istniejącej logiki pokazania okna.

Numer 55, którego użyłem najpierw, jest zajęty przez `kfat_purge_path`
(zakres `call>=53&&call<=55`) — stąd przeniesienie na 66.

## Czego dowiodłem, a czego nie

**Protokół działa.** Pulpit pokazuje na pasku statusu wynik wywołania: `a=0` (nie
znaleziono → uruchom). Czyli wywołanie **dochodzi do jądra i wraca** poprawnie —
z argumentami we właściwych rejestrach (pierwsza wersja wysyłała ścieżkę w argumencie
liczbowym zamiast we wskaźniku i jądro dostawało NULL — naprawione).

**Tablica procesów jest w porządku.** TASKMGR na tym samym jądrze pokazuje:
`INIT.APP, GFX.APP, GUI.APP, DESKTOP.APP, PLASMA.APP (PID 5, 14K), TASKMGR.APP`.
Czyli działająca PLASMA **jest** widoczna z nazwą — wyszukiwanie ma z czego trafiać.

**Test był jednak wadliwy.** Ostatnia diagnostyka (jądro odsyłało, czego szuka)
pokazała na ekranie `a=0 [TASK|INIT]` — czyli **szukaną nazwą było „TASK", nie
„PLAS"**. Mój tymczasowy klawisz testowy wybrał wpis TASKMGR, nie PLASMA, więc ten
przebieg **nie testował funkcji**, tylko ją mijał. To wyjaśnia `a=0` przy działającej
PLASMIE bez zakładania błędu w wyszukiwaniu.

**Wniosek**: mechanizm jest kompletny i protokół zweryfikowany, ale **brakuje
poprawnego testu** — trzeba uruchomić PLASMĘ jednoznacznie (np. dwuklikiem w jej
ikonę) i sprawdzić, czy drugi klawisz/próba na innym pulpicie daje `a=2`
i przełączenie. Tego jednego przebiegu nie dokończyłem.

## Zostawiony stan

- Wywołanie 66 i całe okablowanie pulpitu **zostają w systemie** — architektura jest
  właściwa, brakuje potwierdzenia testem.
- Stałe liczniki: `kdbg_app_seen` (znalezione instancje), `kdbg_app_switch`
  (przełączenia na inny pulpit) — obok `kdbg_desk_*` i `kdbg_batch_*`.
- Zero kodu TEMP, klawisze F pulpitu przywrócone, bramka 43 testy zielone +
  `test_desks.py` zielony.

---

# Poprawki ikonki przełącznika + kluczowe ustalenie o kliknięciach

## Zrobione na życzenie

1. **Ikonka większa**: `DESK_ICON_W/H` 16×13 → **22×15** (pełna wysokość paska) i nowy
   glif — dwa monitory z paskiem zadań na przednim ekranie, czytelne przy 800×600.
2. **Numerek przezroczysty**: `desk_text()` dostał parametr `opaque`; numerek pulpitu
   rysuje się z `opaque=0` (tylko piksele glifów), a wiersze listy dalej z `opaque=1`,
   bo tam tło musi zgadzać się z podświetleniem wiersza. Biały prostokąt zniknął —
   potwierdzone na zrzucie paska z dwoma pulpitami.

## Ustalenie, które spina dwa objawy

Test „jednej instancji" miał polegać na **dwukliku w ikonę PLASMY**. Na pulpicie 1
dwuklik działa (PLASMA startuje, widać jej okno). **Na pulpicie 2 ten sam dwuklik
nie dociera do aplikacji** — status pulpitu pozostaje pusty, czyli `launch_run()`
nie zostało wywołane.

To **ten sam objaw**, co wcześniej znaleziony brak reakcji ikonki przełącznika na
pulpicie 2: kliknięcia nie docierają do pulpitu działającego w sesji 1. Wcześniej
zmierzyłem, że pętla kompozytora tam chodzi (`kdbg_frames` +33 869), a licznik
`kdbg_desk_menu` stoi — więc problem jest w **dostarczaniu wejścia do aplikacji po
przełączeniu sesji**, nie w kompozycji.

To dokładnie punkt 2 z „Rozbudowa nie jest jeszcze spójna" w RAPORCIE audytu:
*„Przełączenie musi anulować przechwycenie i nakłady starej sesji"*. Dopóki to nie
działa, **na drugim pulpicie nie da się klikać** — ani w ikony, ani w przełącznik.
Dlatego testu jednoczesnej instancji nie dało się dokończyć na pulpicie 2: brakuje
wejścia, nie mechanizmu.

## Kolejność, jaka z tego wynika

1. **Wejście po przełączeniu sesji** (kliknięcia do aplikacji w nowej sesji) —
   bez tego drugi pulpit jest tylko do oglądania.
2. **Dokończenie testu jednej instancji** — po punkcie 1 dwuklik na pulpicie 2
   uruchomi ścieżkę `gui_call(66)`.
3. Reszta punktów z raportu (aplikacje w tle, dekoracje/listа zadań, limity slotów).

Stan: bramka **43 testy zielone**, `test_desks.py` zielony, **zero kodu TEMP**,
klawisze F przywrócone.

---

# JEDNA INSTANCJA PROGRAMU — DZIAŁA (potwierdzone w emulatorze)

## Przyczyna, którą wcześniej źle zdiagnozowałem

Test „nie działa" był **wadliwy, nie funkcja**. Dwa błędy w moim stanowisku testowym:

1. Wywołanie 66 wysyłało ścieżkę w **argumencie liczbowym**, a jądro czyta ją ze
   **wskaźnika** (`pointer0`, czyli 4. argument) — jądro dostawało NULL. Naprawione:
   `gui_call(66,0,0,path,where)`.
2. Mój tymczasowy klawisz szukał wpisu w launcherze i **wybierał TASKMGR zamiast
   PLASMA** — ostatnia diagnostyka pokazała to wprost na ekranie: `a=0 [TASK|INIT]`,
   czyli szukaną nazwą było „TASK".

Do tego dwuklik testowy był **za szybki** (dwa kliknięcia 40 ms od siebie) i nie
łapał się w okno dwukliku pulpitu. Rozsunięcie na ~160 ms (`pause_tick` × 8) —
jak u człowieka — uruchomiło ścieżkę.

## Dowód końcowy

Trzy próby uruchomienia PLASMY (raz na pulpicie 1, potem dwa razy po przełączeniu):

- na ekranie jest **jedna** PLASMA i jej okno,
- pasek statusu pulpitu pokazuje **„SHOWN"** — czyli `launch_run()` podniósł
  istniejące okno zamiast uruchamiać proces,
- w poprzednim przebiegu, gdy PLASMA działała na pulpicie 1, a dwuklik padł na
  pulpicie 2, pasek przełącznika pokazał **„1"** — system **sam wrócił na pulpit
  z działającą aplikacją** zamiast tworzyć drugą kopię.

## Trzecia poprawka po drodze: przechwyt kliknięcia jest per sesja

`mouse_capture_pid` w `kernel_user_gui_keyboard_wake()` był **statyczny dla całego
systemu**: przechwycenie zatrzaśnięte na pulpicie 1 mogło zjadać kliknięcia na
pulpicie 2 i wysyłać je do właściciela z poprzedniej sesji. Dodany warunek:

```c
if(mouse_capture_pid &&
   kernel_user_pid_terminal(mouse_capture_pid)!=ksessions_current())
    mouse_capture_pid=0;
```

Doszły też atrapy w `tests/test_gui_focus_routing.py` i
`tests/test_gui_input_backpressure.py` (te testy kompilują wycinek funkcji, więc
potrzebują `ksessions_current`, `kernel_user_pid_terminal`, `SESSION_COUNT`).

Nowy stały licznik: `kdbg_mouse_routed[sesja]` — dokąd trafiają zdarzenia myszy.

## Stan

Bramka **43 testy zielone** + `test_desks.py` zielony, **zero kodu TEMP**,
klawisze F przywrócone. Ikonka przełącznika 22×15, numerek przezroczysty.
