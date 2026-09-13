# G1 — multitasking programów TOS (plan wdrożenia)

Cel: **kilka programów TOS jednocześnie**, każdy w swoim oknie, wywłaszczane przez
scheduler ASF — bez psucia działającego T1 (jeden program w kontekście shella).

## Stan wyjściowy
- T1: trapy #1/#2/#13/#14 + Line-A, GEMDOS/BIOS/XBIOS, prywatny ekran 32 KB (globalnie),
  uruchamianie **inline** w kontekście shella (supervisor, blokuje shell).
- T2.1: kompozytor w Timer C rysuje okno z prywatnego ekranu (8bpp ET4000).
- Zagnieżdżony Pexec działa dzięki `tos_state_save/restore` (`tos_state_t`) — gotowy
  fundament pod przełączanie stanu per proces.

## Model docelowy
Program TOS = **proces ASF**. Jądro trzyma per proces:

```c
typedef struct {
    uint8_t  *screen;            /* 32 KB prywatnego ekranu */
    uint8_t   mode;              /* 0=low 1=medium 2=mono */
    uint16_t  palette[16];
    uint8_t  *heap; uint32_t heap_used;
    tos_file_t files[TOS_MAX_HANDLES];
    uint8_t  *dta;
    uint8_t  *basepage;          /* A0 programu (p_cmdlin itd.) */
    int32_t   key_pending;
    int32_t   exit_code;
    uint16_t  date,time;
    int32_t   window_x,window_y,window_w,window_h;
} tos_proc_t;
tos_proc_t tos_proc[TOS_MAX_PROCS];
```

Przy przełączeniu kontekstu: `tos_state_save(prev)` / `tos_state_restore(next)`
(mechanizm już istnieje dla Pexec) — mniej inwazyjne niż przepisywanie wszystkich globali.

## Kroki (każdy testowany na Hatari)

### G1.1 — trapy z trybu użytkownika (fundament)
Trap z user mode odkłada ramkę na **SSP**, a argumenty TOS zostają na **USP**.
Dziś `tos_args()` czyta zawsze `frame+8` → w trybie user zwróci śmieci.
- stuby (`tos_entry.S`) przekazują dodatkowo `%usp`,
- `tos_args(frame,usp)`: bit 13 SR → `frame+8`, w przeciwnym razie `usp`.
Test: `t1full.prg` jako proces user daje identyczny wynik.

### G1.2 — tabela stanów per proces
`tos_proc[]` + alloc/free; `tos_state_save/restore` rozszerzone o basepage, exit_code,
date/time, dir-cache; scheduler woła save/restore przy przełączeniu.
Test: `run t1full.prg` bez zmian wyniku (brak regresji).

### G1.3 — uruchamianie jako proces (nie inline)
`run_tos_program` tworzy proces ASF ze stubem wejścia (A0=basepage, SP=stos programu,
`jmp` do `text`); `Pterm`/fault kończy proces, nie unwind stosu shella.
Test: `start t2test.prg` → shell **nie** jest zablokowany.

### G1.4 — kompozytor wielu okien
dla każdego żywego `tos_proc` z `window_w>0` rysuj okno (kaskada), aktywne na wierzchu,
dirty-rect per okno (checksum już jest).
Test: dwa programy → **dwa okna** jednocześnie.

### G1.5 — focus i wejście
aktywne okno wyznacza, czyj `Physbase` i czyj `key_pending`; klawiatura trafia do aktywnego.
Test: dwa programy czekające na klawisz — znak trafia do aktywnego.

## Ryzyka
| Ryzyko | Mitygacja |
|---|---|
| A5-world/globalne Line-A współdzielone | VDI w kontekście aplikacji pod zamkiem (G3) |
| programy piszące do sprzętu | supervisor + MMU write-protect jądra |
| regresja T1 | każdy krok testowany Hatari; T1 zostaje zielone |
| wydajność N okien | dirty-rect, blitter, limit N (np. 4) |

## Baza kodu
- `system/tos_prg.c` — trapy, stan, runner (najwięcej zmian),
- `system/tos_entry.S` — stuby trapów (USP),
- `system/kernel_exception.c` — scheduler/procesy (hook save/restore),
- `hw/video8.c` — kompozytor wielu okien.

---

## G1.3 — szczegół implementacji (preemptive switch zadań TOS)

Technika jest ta sama, której jądro ASF używa dla procesów user: zadanie
przerwane Timerem C ma **pełny kontekst na własnym stosie**, więc przełączenie
to podmiana SP i zwykłe `rte`.

### Struktura zadania
```c
typedef struct {
    uint8_t   used;
    uint32_t  sp;          /* SP zadania w momencie przerwania (ramka Timera) */
    tos_state_t state;     /* parkowany stan TOS (G1.2) */
    uint8_t  *basepage;
    uint8_t  *entry;
    int32_t   pid;
} tos_task_t;
tos_task_t tos_task[TOS_MAX_PROCS];
```

### Przepływ
1. `run_tos_program(path)` **nie** robi `asf_tos_enter`. Ładuje PRG, buduje
   basepage, alokuje stos, wypełnia `tos_task[]` i **wraca** (shell odblokowany).
2. Timer C (50 Hz): jeśli żyje >1 zadanie → `tos_task_schedule()`:
   - zapisz `tos_state_save(&tos_task[cur].state)`,
   - zapisz `tos_task[cur].sp = <SP w handlerze>`,
   - wybierz następne `next`, `tos_state_restore(&tos_task[next].state)`,
   - zwróć `tos_task[next].sp`; handler ustawia SP i robi `rte`.
3. Pierwsze wejście w zadanie: przygotuj sztuczną ramkę na stosie zadania
   (SR/PC = punkt startowy, A0 = basepage, SP = stos programu) i potraktuj ją
   jak powrót z przerwania — wtedy „pierwsze uruchomienie" i „wznowienie" to
   ten sam kod.
4. `Pterm`/fault: wyzeruj `tos_task[pid].used`, wybierz następne zadanie; jeśli
   brak — wróć do kontekstu shella (jak dziś `asf_tos_exit`).

### Pułapki (do pilnowania przy testach)
- **VBR=0**: trapy są globalne, więc handler musi brać stan z `tos_proc_switch`
  (już jest) — nigdy z domysłów o bieżącym zadaniu.
- **Stos zadania** musi być w pamięci, której nie dotyka `kmem_free` przy wyjściu.
- **Fault** (ILLEGAL) kończy tylko zadanie, nie cały system.
- **Kompozytor**: rysuje okna wszystkich żywych zadań (G1.4), nie tylko bieżącego.

### Testy po półkrokach
- **G1.3a**: `start t2test.prg` → shell przyjmuje kolejne komendy (nie blokuje).
- **G1.3b**: dwa zadania naprzemiennie (każde liczy/animation w swoim oknie).
- **Regresja**: `run parent.prg` (Pexec) i `run t1full.prg` bez zmian.


---

## Stan po naprawie toru wejścia (2026-09-12) — przed G1.3a

Trzy niezależne błędy złożyły się na to, że mysz „udawała klawiaturę":
1. **Zakres nagłówka paczki myszy** był `F8-FB`; Hatari (i IKBD w trybie
   both-mouse-and-joy) emituje też `FE/FF`. Poprawny zakres to **`F8-FF`** —
   poprawione w **obu** miejscach: persona TOS (`tos_next_scancode`) *i*
   INPUT.SYS (`input_packet_header`). To dlatego również shell wpisywał znaki.
2. **Overrun ACIA** gubił bajt i ramka przesuwała się o jeden. `ikbd_raw_pop`
   zwraca status razem z bajtem, więc bit **`OVRN` (0x20)** daje sygnał: po nim
   odrzucamy bajty **do najbliższego nagłówka** (`tos_resync`).
3. **Dwóch konsumentów pierścienia** (Timer C i `Cconin`) — teraz pierścień
   opróżnia **tylko Timer C**, a `Cconin`/`Cconis` czytają kolejkę `tos_keyq`.

Dowód: log `raw(16)` pokazuje idealne paczki `F8 00 02 / F8 FD 01 / F8 FF 00`,
a pulpit przeżywa kliknięcia i ruch myszy. Regresja: Pexec OK, `t1full` OK
(jego grafika Line-A widoczna w oknie T2).

### Gotowy przepis na G1.3a (kod)

1. `tos_task_t { uint8_t used; uint32_t sp; tos_state_t state; uint8_t *basepage;
   uint8_t *entry; uint32_t pid; }` + `tos_task[TOS_MAX_PROCS]`.
2. Rozdzielić `kernel_tos_run_program` na:
   - `tos_task_load(path, &task)` — stat_path + kmem + relokacje + basepage +
     `tos_state_*` (bez uruchamiania),
   - `tos_task_start(&task)` — pierwsze wejście: na stosie zadania ułożyć
     sztuczną ramkę (SR=0x2000, PC=entry, SP=stos programu, A0=basepage) i
     potraktować ją jak powrót z przerwania (wspólny kod ze wznowieniem),
   - `kernel_tos_run_program(path)` = `tos_task_load` + dodaj do kolejki +
     **return 0** (shell nieblokowany).
3. Hook w Timer C: `tos_task_tick()` — jeśli żyje >1 zadanie:
   `tos_state_save(&tos_task[cur].state)`, `tos_task[cur].sp = <SP handlera>`,
   wybierz `next`, `tos_state_restore(&tos_task[next].state)`, zwróć `sp`,
   handler ustawia SP i robi `rte`.
4. `Pterm`/fault: `tos_task[pid].used=0`; brak zadań → powrót do shella
   (jak dziś `asf_tos_exit`).
5. Kompozytor (G1.4): rysuj okno dla **każdego** `used` zadania (kaskada
   pozycji + focus na aktywnym); `Physbase` aktywnego = jego bufor.

### Testy G1.3a
- `start t2test.prg` → shell przyjmuje kolejne komendy (nie blokuje).
- Regresja: `run parent.prg` (Pexec) i `run t1full.prg` bez zmian.
- G1.3b: dwa zadania naprzemiennie (każde liczy w swoim oknie).


---

## G1.3b krok 2 — doprecyzowanie (pułapki wykryte przy projektowaniu)

1. **Slot dla shella/jądra.** Przełącznik w Timerze C zapisuje SP do bieżącego
   slotu. Gdy biegnie shell, nie ma zadania TOS — więc kontekst shella musi mieć
   **własny slot** (indeks `TOS_MAX_PROCS`, piąty), inaczej zapisujemy SP shella
   do slotu zadania i gubimy shell.
2. **Stan tylko między zadaniami TOS.** `tos_state_save/restore` wolno wołać
   wyłącznie przy przełączeniu TOS↔TOS. Przejście shell↔zadanie podmienia
   **tylko SP** (globalne `tos_*` nadal opisują zadanie, które ma wznowić).
3. **Pierwsze wejście = sztuczna ramka.** Na stosie zadania ułożyć dokładnie to,
   co Timer C odkłada w momencie przerwania: 60 B rejestrów (`d0-d7/a0-a6`) +
   `SR` (supervisor, przerwania włączone) + `PC` (= entry). A0 = basepage,
   SP = stos programu − rozmiar ramki. Wtedy „pierwsze uruchomienie" i
   „wznowienie po kwancie" to **ten sam kod** (`movem` + `rte`).
4. **Wyjście zadania (Pterm/fault) nie może iść przez `asf_tos_exit`** — to
   rozwija stos shella. Potrzebna osobna ścieżka: zwolnij slot + `kmem_free`,
   ustaw `tos_task_current` na slot shella i skocz w powrót z przerwania
   (podmiana SP + `rte`), bez dotykania ramki zadania.
5. **Licznik zadań**: przy `TOS_MAX_PROCS` slotach TOS + 1 shell — przełączaj
   dopiero gdy żyją **≥2 sloty TOS** (albo shell + 1 zadanie i co najmniej jedno
   drugie do przełączenia). W przeciwnym razie zachowanie jak dziś (inline).

### Kolejność wdrożenia (półkroki, każdy testowany)
- **2a**: slot shella + `tos_task_tick` obsługujący go; nadal tylko jedno zadanie
  → test regresji (t1full, Pexec) — zachowanie niezmienione.
- **2b**: `tos_task_prepare_entry()` (sztuczna ramka) + uruchomienie zadania
  z timera; `run_tos_program` = enqueue + return. Test: `start t2test.prg`
  (shell przyjmuje kolejne komendy).
- **2c**: wyjście zadania (Pterm/fault) ścieżką z pkt 4. Test: dwa zadania
  naprzemiennie + regresja.

### Stan toru wejścia (warunek wstępny — spełniony)
Enter działa seryjnie (4/4 komendy pod rząd bez sklejania). Przyczyna była w
INPUT.SYS: tłumienie duplikatów „do matching break" blokowało klawisz na zawsze,
gdy break zginął. Teraz duplikat jest przyjmowany.


---

## G1.3b krok 2b — doprecyzowanie (Pexec vs multitasking)

Wniosek z projektowania: **nie można zrobić z `run_tos_program` enqueue+return**,
bo `Pexec` z wnętrza programu TOS ma semantykę **synchroniczną** (dziecko kończy
się, rodzic dostaje kod wyjścia). Potrzebne są **dwie ścieżki**:

| Funkcja | Semantyka | Używa |
|---|---|---|
| `kernel_tos_run_program(path)` | synchroniczna, inline (jak dziś) | `Pexec` |
| `kernel_tos_spawn(path)` (nowa) | load + enqueue, wraca natychmiast | shell (`run`/`start`) |

Dodatkowo:
- flaga **`tos_task_nested`** — podczas zagnieżdżonego Pexec przełącznik w Timerze
  C **nie może** przełączać (kontekst rodzica i dziecka to ten sam stos!)
- API dla shella: nowy wskaźnik w `kernel_module_api_t` (na końcu struktury) —
  shell woła go zamiast offsetu 164, gdy chce tryb asynchroniczny,
- stan zadania **dynamicznie** (`kmem_alloc` pointer w slocie, nie `tos_state_t`
  w tabeli) — inaczej BSS puchnie i psuje zagnieżdżony Pexec (sprawdzone w 2a).

### 2b w półkrokach
- **2b‑i**: `tos_task_prepare_entry()` — sztuczna ramka (60 B regs + SR + PC,
  A0=basepage, SP=stos programu). Test: funkcja wywołana w miejscu inline musi
  dać identyczny efekt jak `asf_tos_enter` (na razie tylko przez `run`).
- **2b‑ii**: `tos_task_load()` (stan dynamicznie) + `tos_task_spawn()` (enqueue).
  Test: `start t2test.prg` → shell nieblokowany, zadanie żyje (TASKS LEFT 1).
- **2b‑iii**: start zadania z Timera C (przy ≥2 zadaniach). Test: dwa `start` →
  dwa zadania naprzemiennie; regresja Pexec/t1full.


---

## G1.3b 2b‑ii — dwie decyzje projektowe (przed kodem)

### 1. Rozróżnienie shell/Pexec bez nowego API
`tos_state_depth` (głębokość zagnieżdżenia) już mówi, kto woła:
```c
int32_t kernel_tos_run_program(const char *path)
{
    if(tos_state_depth>0) return tos_run_sync(path);   /* Pexec: synchronicznie */
    return tos_task_spawn(path);                       /* shell: enqueue+return */
}
```
→ **bez** nowego wskaźnika w `kernel_module_api_t` i **bez** zmian w `shell_module.S`.

### 2. Spawn i exit to jedna atomowa zmiana (2b‑ii + 2c razem)
Zadanie startujące z epilogu Timera C (`movem` + `rte`) **nie weszło** przez
`asf_tos_enter`, więc `Pterm`/fault **nie może** iść przez `asf_tos_exit`
(rozwija stos shella). Potrzebna jest osobna ścieżka:
- `tos_task_exit(code)`: zwolnij slot + `kmem_free(mem/total/state)`,
  `tos_task_current = TOS_KERNEL_SLOT`, i **wróć przez epilog przerwania**
  (podmiana SP na slot shella + `rte`) — bez dotykania ramki bieżącego zadania.
Wniosek: **nie wolno** wdrażać `spawn` bez tej ścieżki — pierwszy `Pterm`
uruchomionego zadania wysypałby system.

### Kolejność finalna (jeden krok, testowany całościowo)
1. `tos_task_load(path,&slot)`: stat_path + kmem + relokacje + basepage +
   `tos_state` **dynamicznie** (`tos_state_t *state` w slocie; slot zostaje mały),
   `mem`/`total` w slocie (do sprzątania),
2. `tos_task_spawn(path)`: load + `tos_task_prepare_entry` + `tos_task_sp[slot]`
   + `used=1` → return 0,
3. Timer C: start/przełączanie gdy `count>=1` i bieżący kontekst to shell
   (albo `count>=2` między zadaniami),
4. `tos_task_exit` + przekierowanie `Pterm`/fault zadań na nią,
5. Test: `run t2test.prg` (shell od razu przyjmuje kolejne komendy) → dwa
   `run` → dwa zadania → dwa okna; regresja `Pexec` (depth>0 sync) i `t1full`.


---

## PRIORYTET: środowisko graficzne w stylu TOS/EmuTOS (decyzja użytkownika)

**Produktem ma być multitaskingowe środowisko graficzne**, nie launcher w shellu.
Docelowy obraz:
- pulpit = interfejs (okna, ikony, menu, przenoszenie, focus, Z-order),
- **shell jako okno** na pulpicie (konsola nie może być prawdziwym ekranem),
- aplikacje TOS i natywne = okna zarządzane przez menedżer okien (G2),
- GEM/VDI/AES (G3/G4) dla zgodności z prawdziwymi programami GEM,
- multitasking (G1.3b) jako warstwa pod spodem, nie jako cel sam w sobie.

### Korekta kolejności
Dla tego celu właściwa kolejność to:
1. **G0** — pulpit trwały + **konsola jako okno**:
   - G0-1: tos_close_window NIE wychodzi z trybu graficznego; przerysowuje pulpit
     (okno aplikacji znika, pulpit zostaje),
   - G0-2: **konsola jako okno** — wyjście kernela/shell/TOS (module_console_puts)
     trafia do **bufora linii w jądarze**, a kompozytor rysuje go przez video8_text;
     dopiero to pozwala pulitowi zostać (konsola w trybie tekstowym pisałaby po
     pamięci karty w 8 bpp i niszczyła pulpit).
2. **G2** — menedżer okien (przenoszenie, focus, Z-order, zamykanie, kaskada).
3. **G1.3b** — scheduler: dwa zadania TOS naprawdę równolegle w oknach.
4. **G3/G4** — VDI/AES ze źródeł EmuTOS.

### Dlaczego G0 przed G1.3b
G0 daje **widoczne środowisko** bez przebudowy wywłaszczania i jest warunkiem jego
sensu: dopóki konsola jest prawdziwym ekranem, każdy program wraca do shella, a nie
do pulpitu. Po G0 scheduler dokłada równoległość wewnątrz środowiska.


---

## STAN NA KONIEC SESJI (2026-09-12) — środowisko graficzne działa

### Zrobione i udowodnione zrzutami
- **T1** (persona TOS): GEMDOS/BIOS/XBIOS/Line-A, pliki, Pexec, fault-recovery,
  klawiatura+shift, p_cmdlin — `t1full` 6/6, Pexec `child exit code = 7`,
  fault -> -4.
- **T2.1**: prywatny ekran 32 KB -> okno na ET4000 (skalowanie 2:1), blokujący
  Cconin, geometria CRTC 0x14 (czcionka wróciła).
- **Cache**: write-through + `SMARTDRV -on/-off` + `DISKPD` + Ctrl+Alt+Del.
- **Wejście**: paczki myszy ramkowane `F8-FF`, jeden konsument pierścienia
  (Timer C) + kolejka klawiszy TOS; filtr kursora shella w logu.
- **G0-2 (środowisko!)**: bufor linii konsoli w jądarze + **okno CONSOLE na
  pulpicie** + **pulpit trwały** (`tos_close_window` nie robi `vid_leave`;
  konsola tekstowa wyciszona na czas pulpitu). Zrzuty: pd1 (okno aplikacji +
  okno konsoli), pd2 (po zamknięciu programu pulpit ZOSTAJE).
- **G1.1** trapy user-mode; **G1.2** `tos_proc[]`; **G1.3a** tabela zadań;
  **G1.3b-1** przełącznik w Timer C (uśpiony); **2a** tablica SP + slot shella;
  **2b-i** builder ramki (układ z kernel_entry.S); **2b-ii-a** `state` jako
  wskaźnik (mniejszy BSS) + `mem`/`total`/`stack`.

### Pending (uporządkowane)
1. **Gubiony Enter (intermittent)** — 2 z 3 komend przechodzą; to wyścig, nie
   stała blokada (timeout 0,5 s pomógł, ale nie eliminuje). Diagnostyka: użyć
   mechanizmu `TOS EXIT KEY`/`raw(16)` na tej ścieżce (jak przy myszy).
2. **G2 — menedżer okien**: przenoszenie, focus, Z-order, zamykanie gadżetem,
   kaskada pozycji nowych okien.
3. **G1.3b-2b-ii-b**: `tos_task_load` + `tos_task_spawn` + **`tos_task_exit`**
   (razem, atomicznie — patrz sekcje wyżej) + rozróżnienie shell/Pexec przez
   `tos_state_depth`.
4. **G3/G4**: VDI/AES ze źródeł EmuTOS (zgodność z GEM).
5. Kosmetyka: pełne odświeżanie okna konsoli (na razie rysowane przy otwarciu
   okna), przycisk zamykania jako klik, ikony pulpitu jako launcherow.

### Narzędzia (do dalszej pracy)
- `atari-030-port/run-bios-vga.sh` — boot; `--cmd-fifo` + `hatari-event` do
  wpisywania; `hatari-debug screenshot` do zrzutów.
- `LOGDMP.PRG` + XBIOS `0x00FE` cmd 9 — hex-zrzut bufora konsoli.
- XBIOS `0x00FE`: 0/1 cache, 6 cache-off+clear, 7 okno ON, 8 okno OFF.


---

## Środowisko graficzne — stan zweryfikowany (koniec sesji)

### Działa i potwierdzone zrzutami
- pulpit trwały + ikony (FLOPPY A:, HARDDISK C:, TRASH) + pasek statusu
  (ASP DESKTOP | RUN <APP.PRG> | CTRL+ALT+D = RESET),
- okno aplikacji z paskiem tytułu (nazwa programu) i gadżetem zamknięcia,
- **żywe okno CONSOLE** — pisanie w konsoli NA PULPICIE działa (zrzut: `help`
  wypisany w oknie konsoli na pulpicie graficznym),
- przełączanie konsola <-> program: `run app.prg` otwiera okno, klawisz zamyka
  i wraca do pulpitu z konsolą,
- kaskada pozycji okien (co uruchomienie +16 px, cykl 3).

### REGUŁA (nauczka z zawieszenia)
**Nigdy nie robić ciężkiego rysowania w Timer C.** Pierwsza wersja odświeżania
okna konsoli była w handlerze timera i zawiesiła maszynę (handler nie nadążał;
pulpit częściowy, brak okna aplikacji). Poprawnie: odświeżanie w **głównej pętli
jądra** (kontekst shella) i **tylko gdy zmienił się licznik linii logu**:
`tos_console_refresh()` wołane z `kernel_main.c`. Timer robi wyłącznie:
`tos_poll_input`, kompozycję 320x200 i przełączanie zadań.

### Tor wejścia — domknięty z danymi
- jądro nie gubi: `IKBD TOTAL/DROPPED` + statusy bez `OVRN`,
- `keypress` w Hatari potrafi zgubić break (pełny bufor IKBD, cicho gubiony),
  dlatego w testach używamy `keydown`/`keyup` z odstępem,
- nasz timeout duplikatów (0,5 s) + stemplowanie każdego make to poprawna obrona.

### Narzędzia
- `LOGDMP.PRG` -> XBIOS `0x00FE` cmd 9 (hex logu konsoli) i cmd 10
  (pierścień IKBD: TOTAL/DROPPED + data:status),
- `run-bios-vga.sh --cmd-fifo` + `hatari-event` + `hatari-debug screenshot`.

