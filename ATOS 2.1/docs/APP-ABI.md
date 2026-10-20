# ATOS — ABI natywnych aplikacji (1.0 zamrożone 2026-09-26, +1.1, +1.2)

Dokument normatywny dla programów \`.APP\` uruchamianych przez ATOS. Kontrakt jest
**zamrożony**: znaczenie i układ pól/opkodów poniżej nie zmieniają się bez
podniesienia \`ATOS_APP_ABI_MAJOR\`. Zmiany addytywne (nowy opkod, nowe pole
dopisane na końcu struktury) podnoszą \`ATOS_APP_ABI_MINOR\`.

Źródła prawdy w kodzie:
- \`include/app_abi.h\` — stałe, enum opkodów, strażniki \`_Static_assert\`,
- \`system/gui_app_runtime.S\` — obraz \`.APP\`, \`app_entry\`, \`gui_call\`,
- \`linker/app-sys.ld\` — sekcje \`.app_header\` i \`.app_policy\`,
- \`system/kernel_exception.c:kernel_user_syscall()\` — implementacja opkodów.

## 1. Obraz \`.APP\`

Plik zaczyna się 32-bajtowym nagłówkiem (sekcja \`.app_header\`, ładowana pod
adresem 0):

| offset | rozmiar | pole |
|---|---|---|
| 0  | 8  | magic \`"ASYSAPP0"\` |
| 8  | 2  | wersja nagłówka = \`ATOS_APP_IMAGE_VERSION\` (1) |
| 10 | 2  | rozmiar nagłówka = 32 |
| 12 | 4  | koniec części wykonywalnej (\`__app_file_end\`) |
| 16 | 4  | koniec pamięci obrazu (\`__app_memory_end\`) |
| 20 | 4  | adres wejścia (\`app_entry\`) |
| 24 | 4  | rozmiar stosu aplikacji |
| 28 | 4  | zarezerwowane (0) |

Na końcu części zapisywalnej leży sekcja \`.app_policy\`:
\`LONG(0x41505732)\`, \`__app_executable_end\`, \`__app_writable_start\`.
Jądro używa jej do rozdzielenia części wykonywalnej i danych przy starcie.

Sumę kontrolną obrazu (offset 28) wypełnia \`tools/patch_boot_checksum.py\`
dla plików \`ASYSAPP0\`.

## 2. Wejście i wyjście

Runtime (\`system/gui_app_runtime.S\`) linkuje \`app_entry\`:

1. \`bsr app_main\` — aplikacja pisze \`int app_main(void)\`,
2. \`d1 = d0\` (kod powrotu), \`d0 = 0\` (opkod EXIT), \`trap #0\`,
3. \`illegal\` — wyjście nie może wrócić.

Aplikacja jest procesem użytkownika: ma własny stos, heaps (opkody 16/17) i
uprawnienia ograniczone przez jądro. Błąd procesu (bus/address/illegal itd.)
kończy **program**, nie system.

## 3. Konwencja wywołań

\`\`\`c
int32_t gui_call(uint32_t number, uint32_t d1, uint32_t d2, void *a0, void *a1);
\`\`\`
- \`number\` — opkod z tabeli poniżej,
- \`d1\`/\`d2\` — argumenty liczbowe (\`argument0\`/\`argument1\`),
- \`a0\`/\`a1\` — wskaźniki (\`pointer0\`/\`pointer1\`), walidowane przez jądro,
- wynik w \`d0\`; wartości ujemne to kody błędów.

Wszystkie wskaźniki muszą wskazywać pamięć bieżącego procesu; jądro sprawdza
zakres i wyrównanie i zwraca błąd (\`<0\`) zamiast ufać wskaźnikowi.

## 4. Opkody (trap #0)

| # | nazwa | argumenty | wynik |
|---|---|---|---|
| 0 | EXIT | d1=status | nie wraca |
| 1 | WRITE | a0=tekst | 0 |
| 2 | OPEN | a0=ścieżka | fd / <0 |
| 3 | READ | d1=fd d2=ile a0=buf | bajty / <0 |
| 4 | CLOSE | d1=fd | 0 |
| 5 | STAT | a0=ścieżka a1=uint32[2]{size,attr} | 0 / <0 |
| 6 | SEEK | d1=fd d2=offset | offset / <0 |
| 7 | ARGS | d1=pojemność a0=buf | długość |
| 8 | PUTCHAR | d1=znak | 1 |
| 9 | GETCHAR | — | znak |
| 10 | TICKS | — | takty 50 Hz |
| 11 | SLEEP | d1=takty | 0 |
| 12 | SYSTEM_INFO | a0=uint32[6] | 0 |
| 13 | DIR_ENTRY | d1=indeks a0=ścieżka a1=dir_entry_t | 0/1 / <0 |
| 14 | GETCWD | d1=pojemność a0=buf | długość |
| 15 | CHDIR | a0=ścieżka | 0 / <0 |
| 16 | ALLOC | d1=bajty d2=preferuj_tt | wskaźnik / <0 |
| 17 | FREE | a0=wskaźnik | 0 / <0 |
| 18 | GETPID | — | pid |
| 19 | RESOURCES | a0=uint32[6] | 0 |
| 20 | PROC_INFO | d1=slot a0=uint32[4] | 0 / <0 |
| 21 | SPAWN | a0=ścieżka a1=uint32[2]{pid,status} | 0 / <0 |
| 22 | SPAWN_REQ | a0=gui_spawn_t* (synchronicznie) | 0 / <0 |
| 23 | QUEUE | a0=gui_spawn_t* (kolejka) | 0 / <0 |
| 24 | JOB_QUERY | d1=pid d2=czekaj a0=uint32[2] | 0 / <0 |
| 25 | JOB_WAIT | d1=pid a0=int32* | 0 / <0 |
| 27 | CONTEXT | a0=uint32[3]{usp,pc,sr} | 0 / <0 |
| 31 | CLONE | d1= a0=uint32* pid-dziecka | 0 / <0 |
| 32 | CHILD_QUERY | d1=pid d2=czekaj a0=uint32[2] | 0 / <0 |
| 33 | CHILD_WAIT | d1=pid a0=int32* | 0 / <0 |
| 34 | SPAWN_ASYNC | a0=gui_spawn_t* (w tle) | 0 / <0 |
| 35 | SET_PATH | a0=lista PATH (\`+\` dokleja) | 0 / <0 |
| 36 | SEND | d1=pid a0=uint32[4] | 0 / <0 |
| 37 | RECV | a0=gui_message_t* (nieblokująco) | 0 / <0 |
| 38 | RECV_WAIT | a0=gui_message_t* (blokująco) | 0 / <0 |
| 39 | SERVICE_ADD | a0=nazwa | 0 / <0 |
| 40 | SERVICE_GET | a0=nazwa | pid / <0 |
| 41 | SERVICE_DEL | a0=nazwa | 0 / <0 |
| 42 | GFX | a0=gfx_request_t* (tylko SYSTEM.GFX) | 0 / <0 |
| 43 | GFX_INFO | a0=uint32[6] (SYSTEM.GFX) | 0 / <0 |
| 44 | GFX_RESTORE | — (SYSTEM.GFX) | 0 / <0 |
| 45 | GUI_ACQUIRE | d1=pid (SYSTEM.GUI) | 0 / <0 |
| 46 | GET_PATH | a0=buf 64 B | 0 |
| 47 | GUI_ACQUIRE_KEEP | d1=pid (SYSTEM.GUI) | 0 / <0 |
| 48 | GUI_RELEASE | — (SYSTEM.GUI) | 0 / <0 |
| 49 | COPY | a0=źródło a1=cel | 0 / <0 |
| 50 | MKDIR | a0=ścieżka | 0 / <0 |
| 51 | DELETE | a0=ścieżka | 0 / <0 |
| 52 | DIR_PAGE | d1=skip d2=ile a0=ścieżka a1=wpisy | liczba / <0 |
| 53 | TRASH | a0=ścieżka | 0 / <0 |
| 54 | MOVE | a0=źródło a1=cel | 0 / <0 |
| 55 | PURGE | a0=ścieżka | 0 / <0 |
| 56 | SYSTEM_REPORT | a0=system_report_t | 0 / <0 |
| 57 | RAW_BLOCK | d1=sektor d2=ile a0=buf | bajty / <0 |
| 58 | CLOCK | — | takty |
| 59 | LANGUAGE_SAVE | d1= | 0 / <0 |
| 60 | SAVE | a0=ścieżka d1=ile a1=buf | 0 / <0 |
| 61 | SHELL | d1=pojemność a0=cmd a1=out | 0 / <0 |
| 62 | SHELL_STATE | — | stan / <0 |
| 63 | FLOPPY_DIR | d1=pojemność a0=buf | długość / <0 |
| 64 | FLOPPY_READ | d1=pojemność a0=nazwa a1=buf | bajty / <0 |
| 65 | TOS_RUN | a0=ścieżka .PRG/.TOS | kod TOS |
| 67 | HIRES_CLOCK | - | monotoniczne zliczenia Timer C (12288/s, ok. 81 us) |
| 66 | ABI_VERSION | — | \`ATOS_APP_ABI_VERSION\` |

Opkody 26, 28, 29, 30 są niezarezerwowane i nieużywane.

## 5. Struktury współdzielone

| struktura | rozmiar | uwagi |
|---|---|---|
| \`gui_message_t\` | 20 B | \`{uint32 sender; uint32 v[4];}\` |
| \`gui_spawn_t\` | 136 B | \`{char path[64]; char args[64]; uint32 pid; int32 status;}\` |
| \`gui_draw_t\` | 48 B | \`kind\` @0, \`text[32]\` @16 |
| \`atos_app_dir_entry_t\` | 24 B | \`name[13]\`, atrybuty, \`size\` @16, \`datetime\` |

Układ jest pilnowany przez \`_Static_assert\` w \`include/app_abi.h\` oraz host-test
\`tests/test_app_abi.c\` (\`make check-app-abi\`).

## 5a. ABI 1.2 — tryb gry, joystick, pamięć szybka (2026-10-09)

Zmiany addytywne; programy 1.0/1.1 działają bez zmian.

**Tryb wyłączny pełnego ekranu** — opkod GFX 38 (przez `SYSTEM.GFX`, pole
`color` = podoperacja, struktury w `include/atos_game.h`):

| podoperacja | argument `xy` | wynik |
|---|---|---|
| 0 BEGIN | `(w<<16)\|h` trybu 8 bpp, np. 640×480 (0 = bieżący) | `(w<<16)\|h` aktywnego trybu / <0 |
| 1 END | — | 0 (wołane też automatycznie przy wyjściu procesu) |
| 2 FRAME | `atos_game_frame_t*` | 0 narysowano, 1 narysowano + wyślij pełną klatkę, 2 sesja niewidoczna |
| 3 PALETTE | `atos_game_palette_t*` (8 bitów/składową) | 0 / <0 |

BEGIN wymaga, by klient był właścicielem klawiatury swojej sesji
(`GUI_BEGIN_KEYS`) i działa tylko na ET4000.  Na czas trybu kompozytor, pasek
zadań i wskaźnik myszy nie rysują na ekranie tej sesji; END przywraca
poprzedni tryb wideo, paletę systemu i odświeża pulpit.  FRAME kopiuje
(opcjonalnie ×2) prostokąty bufora 8 bpp klienta prosto do ramki prezentowanej
i wypycha zmienione słowa do karty.

**Joystick** — pakiety IKBD `0xFF` (port 1) i `0xFE` (port 0) są dekodowane
przez INPUT.SYS na wirtualne scancody w zwykłym strumieniu klawiszy
(make/break, KKEY): `0x78` góra, `0x79` dół, `0x7a` lewo, `0x7b` prawo,
`0x7c` fire portu 1, `0x7d` fire portu 0.  Wcześniej bajt stanu trafiał do
strumienia klawiszy jako fałszywy scancode (drążek w górę = ESC).

**Pamięć** — `ALLOC` z `d2=1` (preferuj TT/ALT-RAM) ma osobny budżet 16 MB na
proces, pod warunkiem że blok faktycznie leży w pamięci szybkiej
(adres ≥ `0x01000000`); przydział w ST-RAM nadal podlega limitowi 1 MB.

## 6. Zasady kompatybilności

- **MAJOR** rośnie, gdy zmienia się znaczenie istniejącego opkodu, układ/rozmiar
  współdzielonej struktury, format obrazu \`.APP\` lub konwencja wejścia.
- **MINOR** rośnie przy dodaniu nowego opkodu lub pola na końcu struktury, o ile
  stare programy działają bez zmian.
- Aplikacja może sprawdzić wersję przez opkod \`ABI_VERSION\` (66) i odmówić
  startu, jeśli \`MAJOR\` się nie zgadza.
- Ten dokument i \`include/app_abi.h\` zmieniają się **razem** — inaczej host-test
  i \`_Static_assert\` zatrzymają build.


### ABI 1.4 — synchronous window frame (81)

`GUI_FRAME`: d1 is the total `gui_draw_t` count (1..2048), d2 the
number of groups (1..32), a0 the contiguous draw array, a1 an array of
uint16 group lengths (each 1..80, sum equals d1). All memory and counts
are validated before drawing. The target is the caller's own handle-0
window; no caller-supplied owner or framebuffer is accepted. Each group
retains its own damage rectangle, avoiding a ball-to-paddle bounding box.
The kernel draws all groups and composes once synchronously. Return 0
means completed submission, 1 means the session is hidden (retain a full
repaint for return), negative means error. An obscured/minimized window
may have no visible pixels: a successful call is not proof of scanout.
No global video mode or palette change is part of this operation.
Applications must check ABI >= 1.4; legacy SYSTEM.GFX batches still work.
Buffers may be reused on return; the kernel retains no user pointers.

## SLEEP(0): dobrowolne oddanie procesora (2026-10-10)

`gui_call(ATOS_CALL_SLEEP, 0, 0, 0, 0)` przechodzi przez główną pętlę
jądra (m.in. routing klawiatury i myszy), pozostawiając proces gotowy do
wznowienia. Nie czeka na następny takt 50 Hz. Dodatnie czasy nadal usypiają
proces do wyznaczonego taktu. Walidacja stosu i sterty obowiązuje także dla
zerowego czasu. To nie jest odpowiednik odczytu zegara `HIRES_CLOCK`.
