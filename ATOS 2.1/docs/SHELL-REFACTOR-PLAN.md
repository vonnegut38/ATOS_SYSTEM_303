# Shell — plan rozdzielenia modułu (na później, bez refaktoryzacji teraz)

Powód: `system/shell_module.S` ma ~2648 linii assemblera i miesza warstwy.
Refaktoryzacja "dla estetyki" w działającym systemie to ryzyko, więc: **nie
dokładamy już nowych rzeczy do tego pliku**, a rozdzielenie robimy etapami,
każdy z testem regresji (shell + Commander + scenariusze Hatari).

> **STATUS (2026-09-26).** Poniższy tekst jest **kroniką** i zawiera sprzeczne
> wnioski z kolejnych podejść. Ustalenia potwierdzone:
> - Builtin `path` **istnieje** (`shell_module.S:763-767`, `:907-934`);
>   naprawa paniki to zapis/odtworzenie `%d2` (sekcja „PRZYCZYNA ŹRÓDŁOWA").
> - Obserwacja „builtin przestaje się rozpoznawać” była **artefaktem harnessu**
>   (gubiony pierwszy znak: `emory` zamiast `memory`). `console_lines()` i
>   `type_checked()` są naprawione (sekcja „Harness").
> - **Zamknięte (2026-09-26):** `atos2-tools/scenario_path_assert.py` przy
>   zweryfikowanym wejściu (`type_checked`) daje `path` → `path \NOPE` →
>   `path`/`memory` **bez** „PROCESS START FAILED”; wątek `-2` był
>   artefaktem gubionych znaków, nie pamięci/procesów.

## Docelowy podział
1. `shell_parser` — tokenizacja linii, rozpoznanie komendy i argumentów
   (`shell_buffer`, `shell_command_arg`, `shell_same`, historia, edycja linii).
2. `shell_path` — budowa i rozwiązywanie ścieżek oraz PATH
   (`shell_make_path`, `shell_make_exec_path`, `shell_build_path` [już w C],
   `shell_start_add_app`, `shell_path_retry`).
3. `shell_proc` — uruchamianie i kontrola procesów
   (`shell_do_start*`, `shell_start_launch`, foreground/background, `kill`,
   `fg/bg/stop/wait`, obsługa zakończenia).
4. `shell_fs` — komendy systemu plików (`dir/ls/cd/type/cat/md/rd/del/copy`).
5. `shell_term` — terminal/prompt/sesje (`shell_print_prompt`, kursor, redraw,
   `shell_puts`/`put_u32`, `select_session`, `pump`).

Interfejsy między modułami: małe, jawnie wersjonowane nagłówki (jak
`kernel_shell.h`), bez współdzielonych globalnych buforów poza jednym
kontekstem sesji.

## Błąd do naprawy przy okazji (potwierdzony 2026-09-26)
**Repro:** `path` (OK) → `path \NOPE` (nieudany) → `path`
→ `PROCESS START FAILED`; jawne `run \SYSTEM\APP\PATH.APP` działa i kolejne
`path` znowu działa.

Ustalenia: `system_exec_path` w jądrze jest **niezmieniony** przed/po
(`\SYSTEM;\SYSTEM\COMMAND;\SYSTEM\APP;\SYSTEM\TEST`). Czyli zły PATH nie psuje
stringu — psuje się **stan ścieżki szukania / foregroundu**.

## Zawężenie (2026-09-26, po naprawie paniki)
Trzy miejsca wołają `shell_start_failed` (`shell_module.S`):
- **2049** — `shell_simple_program` == 0 (jawna ścieżka/drive → bez PATH),
- **2057** — `shell_make_exec_path` == 0 (wyczerpane wpisy PATH),
- **2071** — attach foregroundu (`api+116`) zwrócił 0.

**Testy rozstrzygające na Hatari (nie zgadywane):**
- `hw` — goła nazwa, **małymi literami** → `PROCESS READY, PID: 4` →
  pełny raport HW → `PID 4 EXIT 01`. **Działa.**
- `command` — goła nazwa → uruchamia Commander. **Działa.**
- `run \SYSTEM\APP\PATH.APP \NOPE` → `PROCESS READY, PID: 4`. **Działa.**
- `path \NOPE` (nazwa bez ścieżki, przez `shell_unknown_command`)
  → `PROCESS START FAILED`. **Nie działa.**
- `shell_simple_program` odrzuca tylko `\`, `/`, `:` — **kropki nie**, więc
  `PATH.APP` **nie** jest odrzucane na 2049.
- `shell_do_path_display` wypisuje 4 wpisy PATH (`get_exec_path` 0..3), więc
  indeksowanie `shell_path_retry-1` jest poprawne.
- `path_component` w `kernel_fat.c:127` **podnosi litery do góry**, więc FAT
  nie jest tu winny; `shell_same` jest wrażliwe na wielkość, ale nazwy
  komend w tabeli są małymi literami.

**Wniosek:** hipotezy „2057 / szukanie PATH” oraz „wielkość liter” są **obalone**.
Awaria jest **specyficzna dla `path` z argumentem**: `path` bez argumentu
trafia w builtin `shell_do_path_display`, ale `path \NOPE` nie pasuje do
`shell_same`, spada do `shell_unknown_command` i kończy się
`PROCESS START FAILED` — mimo że **identyczna droga** dla `hw`/`command`
działa. Różnica sprowadza się do niepustego argumentu (`\NOPE`) i/lub
kolizji nazwy z builtinem.

Następny krok: instrumentacja `shell_do_start` — wartości
`shell_start_filename`/`shell_start_arguments`, `shell_work_path`,
`shell_path_retry` oraz **który** z call-sites 2049/2057/2071 zadziałał.

## Reguły na potem
- Nowe komendy TOS/GEM NIE wchodzą do `shell_module.S` ani `kernel_exception.c`.
  `kernel_exception.c` (~1809 linii) też wymaga rozdziału: dispatch syscalli
  vs obsługa wyjątków vs procesy/terminale.
- Każde wydzielenie = osobny commit z zielonymi kontrolami (`build_atos2.py`).

## Wynik instrumentacji (TEMP-DEBUG, revert)
Znaczniki w trzech call-sites + zrzut `site retry shell_work_path`:

```
tts1 C:\> path \NOPE
PROCESS READY, PID: 4
USAGE: PATH.APP DIRECTORY
PID 4 EXIT 01
tts1 C:\> hw
57 5 \SYSTEM\TEST\hw.APP
PROCESS START FAILED
```

Wnioski twarde:
1. Awaria jest na **site 57** = `shell_make_exec_path` == 0, czyli pętla
   ponawiania wyczerpała wpisy PATH. `retry = 5` → indeks 4 poza zakresem
   (ważne są 0..3); ostatnia zbudowana ścieżka to `\SYSTEM\TEST\hw.APP`.
2. **`path \NOPE` zadziałało** w tym przebiegu (`PROCESS READY` →
   `USAGE: PATH.APP DIRECTORY` → `PID 4 EXIT 01`), a `hw` — które
   w **poprzednim** przebiegu działało — **zawiodło**.
3. Czyli błąd jest **niestabilny / zależny od stanu**, nie deterministyczny.
   To wyjaśnia, dlaczego analiza statyczna call-sites, liter i FAT za każdym
   razem „wychodziła na zero”: logika jest poprawna, a zawodzi **wyszukanie
   istniejącego wpisu** `\SYSTEM\APP\<NAZWA>.APP`.

Podejrzenie: w pętli ponawiania `get_exec_path` pisze do `shell_exec_path`,
a `shell_build_path` dostaje **ten sam** `shell_exec_path` jako `base` i pisze
do `shell_work_path` — współdzielone bufory modułu użyte jednocześnie jako
źródło i bufor pośredni. Do przejrzenia przy wydzielaniu `shell_path`
(zamiast łatać `shell_module.S`, zgodnie z regułami wyżej).

## PRZYCZYNA ŹRÓDŁOWA (potwierdzona) — kod -2, nie PATH
Kody zwrotne `kernel_spawn_user_async` (`kernel_module.c`):
- `-1` plik nie znaleziony / złe atrybuty / zły rozmiar,
- `-2` **brak pamięci** (obraz lub stos),
- `-3` błąd odczytu / verify.

`shell_start_launch` traktowało **każdy** ujemny jak „nie ma pliku”
(`bpl.s shell_start_launched`), więc `-2`/`-3` uruchamiały marsz po całym
PATH i kończyły mylącym `PROCESS START FAILED`.

**Naprawa po stronie shella (zrobiona):**
`cmpi.l #-1,%d0` + `bne shell_start_failed` — tylko `-1` kontynuuje szukanie
w PATH; `-2`/`-3` kończą natychmiast. Zweryfikowane: `path \NOPE` → OK
(`PID 4 EXIT 01`), potem `hw` → `PROCESS START FAILED` **natychmiast**.
Czyli start `\SYSTEM\APP\HW.APP` zwraca `-2`: **pamięć obrazu/stosu
poprzedniego procesu nie jest jeszcze zwolniona**.

**Co zostaje (punkt 5 — stabilność procesów):** zwalniać obraz i stos procesu
w ścieżce zakończenia **zanim** shell wystartuje następny proces. Wtedy `-2`
zniknie i kolejne `hw` po `path \NOPE` zadziała od razu (dziś działa dopiero za drugim razem, gdy pamięć zdąży wrócić).

## Mapa alokacji/zwalniania obrazu i stosu procesu (punkt 5)
Alokacja (`system/kernel_module.c`, `kernel_spawn_user_async`):
- linia **467**: `image = kmem_alloc(file_size, 16, owner=0x0410, ...)`,
- linia **471**: ewentualne `expanded` (ten sam owner `0x0410`),
- linia **474**: `stack = kmem_alloc(stack_size+8, 16, owner=0x0411, ...)`,
- linia **478**: `kernel_user_create_loaded(..., 0x0410u, 0x0411u, child_pid)`
  → `runtime_image_owner = 0x0410`, `runtime_stack_owner = 0x0411`,
- linia **483/484**: `kmem_free(...)` tylko na ścieżce błędu.

Zwalnianie (`system/kernel_exception.c`, teardown procesu):
- linia **567**: heaps → `user_heap_release(&process->memory[i], process->memory_owner)`,
- linia **568**: `kmem_release(process->runtime_stack, process->runtime_stack_owner)`,
- linia **569**: `kmem_release(process->runtime_image, process->runtime_image_owner)`.

**Do sprawdzenia dalej:** czy ten teardown (567-569) wykonuje się **synchronicznie
w chwili zakończenia procesu**, czy dopiero przy odbiorze completion w pętli
kernela. Jeśli dopiero później, kolejny `spawn` widzi pamięć poprzedniego
procesu jako zajętą i zwraca `-2` — co dokładnie zaobserwowałem. Drugie
podejrzenie: `pool_free` odrzuca zwolnienie z powodu niezgodności ownera
(`0x0410`/`0x0411` vs `memory_owner`) i pamięć wycieka.

### Ustalenia z kodu (kolejny krok)
- **Teardown istnieje i jest poprawny:** `kernel_user_end` (`kernel_exception.c`
  **866-875**) zwalnia heaps, `runtime_stack` i `runtime_image` — przez
  `kmem_release(..., runtime_*_owner)`.
- **Obraz i stos są PINOWANE** przy tworzeniu procesu
  (`kernel_exception.c` **859-862**: `kmem_pin(image, runtime_image_owner)`,
  `kmem_pin(runtime_stack, runtime_stack_owner)`).
- **`kmem_free` odmawia zwolnienia pinowanego bloku**
  (`kernel_memory.c` **88**: `if(b->flags&KMEM_PINNED)return 0;`), a
  `kmem_release` (`kernel_memory.c` **94-98**) najpierw zdejmuje pin
  (`b->flags&=~KMEM_PINNED`) i dopiero wtedy woła `pool_free`.
  → **każde zwolnienie obrazu/stosu musi iść przez `kmem_release`, nigdy przez
  `kmem_free`**; inaczej cicho zwraca 0 i pamięć wycieka.
- `pool_free` (`kernel_memory.c` **81-86**) dodatkowo wymaga zgodności ownera
  (`b->owner!=owner` → 0), więc owner zapisany w `runtime_*_owner` musi być
  dokładnie tym samym, którym alokowano (`0x0410`/`0x0411`).

**Wniosek:** skoro teardown jest poprawny, `-2` przy kolejnym `spawn` oznacza
najpewniej **kolejność**: shell startuje następny proces, zanim
`kernel_user_end` zakończonego procesu zdąży zwolnić pamięć (reap/completion
w pętli kernela). Następny krok: ustalić, **kiedy** `kernel_user_end` jest
wołane względem `kernel_dispatch_ready_user`/`kernel_handle_dispatch_completion`
w `kernel_main` — i czy da się je domknąć synchronicznie w chwili exit.

### Korekta (sprawdzone w kodzie)
`kernel_user_end` jest wołane **synchronicznie** z syscalli kończących proces
(`kernel_exception.c` **404, 458, 1113, 1243, 1386, 1428**; np. 1243:
`user_syscall_result=status;kernel_user_end();return status;`). Hipoteza
„odroczony reap” jest więc **obalona** — zwolnienie obrazu/stosu następuje
w chwili exit.

Skoro tak, `-2` przy kolejnym `spawn` oznacza **za mało wolnej pamięci w puli
w tym momencie**: albo fragmentacja, albo `kmem_release` **nie zwalnia**
(niezgodność ownera `0x0410`/`0x0411`, albo blok wciąż `KMEM_PINNED`).

**Następny krok pomiarowy:** po zakończeniu procesu odczytać stan puli
(`kmem_report` / `kmem_get_st_stats` / `kmem_get_tt_stats`; w shellu komenda
`memory`) przed i po, oraz sprawdzić, czy `kmem_release` zwraca 1.

### POMIAR — wycieku NIE ma; psuje się DYSPOZYTOR
`memory` przed i po uruchomieniu i zakończeniu procesu:
- **przed:** `ST-RAM FREE: 3676 KB`, `TT-RAM FREE: 49050 KB`
- **po** (`path \NOPE` → `PROCESS READY, PID: 4` → `USAGE: PATH.APP DIRECTORY`
  → `PID 4 EXIT 01`): **identyczne** `3676 KB` / `49050 KB`.

Czyli **pamięć wraca w całości** — hipotezy „wyciek” i „fragmentacja”
**obalone**.

Ale bezpośrednio po zakończeniu procesu **builtin przestaje się rozpoznawać**:
kolejne `memory` (chwilę wcześniej działało jako builtin i wypisało statystyki)
kończy się `PROCESS START FAILED`, czyli spada do `shell_unknown_command`
→ „uruchom jako program”.

**To przenosi ciężar z pamięci/procesów na DYSPOZYTOR komend w
`shell_module.S`.** Po powrocie z zakończonego procesu coś psuje
`shell_buffer` albo stan dopasowania `shell_same`/`shell_command_arg`.

**Następny krok:** sprawdzić, co nadpisuje `shell_buffer` (albo wskaźniki
`shell_start_filename`/`shell_start_name`) w chwili odbioru zakończenia
procesu — oraz dlaczego `shell_do_memory` (builtin) nie jest już dopasowany
dla identycznego tekstu.

### KOREKTA — to NIE dyspozytor, to HARNESS (2026-09-26)
Zrzut stanu na wejściu do `shell_execute` (`len buffer retry`) dał:
```
6 memory 0            ← "memory" — builtin trafiony poprawnie
18 path \NOPE 0      ← "path \NOPE" — poprawne, PID 4, EXIT 01
5 emory 2             ← wpisane "emory" (zgubione "m"!) → PROCESS START FAILED
```
**Tekst wpisany przez harness to `emory`, nie `memory`** — pierwszy znak
przepadł. `emory` nie jest builtinem, więc `PROCESS START FAILED` jest
**poprawnym** zachowaniem. Dyspozytor jest sprawny.

**Wniosek:** teza „builtin psuje się po zakończeniu procesu” była
**artefaktem syntetycznego wejścia** (znany problem: `hatari-event keydown`
gubi/powiela znaki przez ACIA). Nie ma tu błędu shella do naprawy.

### Harness — `console_lines()` / `type_checked()` (naprawione 2026-09-26)
`atos2-tools/atos2_auto.py`:
- `read_bytes(addr,count)` (parsuje `hatari-debug memdump`) i
  `console_addr()` (symbol `consoles` z `build/system/kernel.elf`) — działały.
- `console_lines()` **naprawione.** Wcześniejsza diagnoza („komórki nie leżą
  na offsecie 0”) była błędna: `term_scroll` to `memmove`, nie ring, więc
  widoczny ekran to wiersze `0..t->row` w `cells[]` (offset 0). Bug to zły
  offset `(25-rows)*cols*2`, który czytał ostatni fizyczny (zwykle pusty)
  wiersz. Teraz funkcja czyta nagłówek `column,row,cols,rows`
  (off 4000/4002/4004/4006, potwierdzone `offsetof`) i zwraca wiersze od 0
  do kursora ze stałym stride `TERM_COLS=80`; `rows_from_cells()` jest czystą,
  testowalną częścią.
- `type_checked()` realnie weryfikuje echo wiersza kursora i ponawia po
  zgubionych znakach; można na nim opierać asercje.

**Zasada dla testów wejścia:** po wpisaniu polecenia i przed `\r` odczytać
echo linii (albo wydłużyć opóźnienia i retry), żeby testy nie raportowały
fałszywych awarii. Wcześniejsze „PROCESS START FAILED” z przebiegów, w których
wejście nie było weryfikowane, są **niepewne**.

## Status 2026-09-27 — `shell_fs` istnieje

Punkt 4 docelowego podziału (`shell_fs`) jest zaczęty i działa:

- `system/shell_fs.c` obsługuje `copy`/`cp`, `move`/`ren`/`rename`,
  `del`/`delete`/`erase`/`rm`, `mkdir`/`md`;
- dispatch to jeden hook w `shell_unknown_command` (`shell_fs_run`),
  bez dokładania komend do `shell_module.S`;
- mutacje FAT wystawione jako v6 ogon `kernel_module_api_t`
  (`copy_path`/`mkdir_path`/`delete_path`/`move_path`);
- **wymóg dla nowych plików C shella:** `-mpcrel` (`SHELL_CFLAGS`) i brak
  wskaźników do własnych literałów w danych statycznych — moduł jest ładowany
  pod adresem runtime (`kernel_module.c`: `entry = image + offset`);
- regresja: `atos2-tools/scenario_shell_fs.py`, strażnik `tests/test_shell_fs.py`.

Pozostałe punkty podziału (`shell_parser`, `shell_proc`, `shell_term`) — bez zmian.
