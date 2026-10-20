# Wiele pulpitów (Desk 1, Desk 2, Add Desk) — analiza wykonalności

Bez zmian w kodzie. Odpowiedź na pytanie: ile to zje pamięci i CPU oraz czy system
wydoli 2 aplikacje i 2 pulpity.

## 1. Fundament już istnieje — to nie jest budowa od zera

System ma **model sesji w stylu TTS**:

| co | gdzie | stan |
|---|---|---|
| liczba sesji | `SESSION_COUNT 3` (`include/kernel_sessions.h`) | **3 sesje** |
| terminal tekstowy per sesja | `consoles[SESSION_COUNT]` | jest |
| właściciel klawiatury per sesja | `gui_keyboard_pids[SESSION_COUNT]` | jest |
| **właściciel pulpitu per sesja** | `gui_desktop_pids[SESSION_COUNT]` | **jest** |
| kolejka myszy, pamięć komórek, kolor | `kernel_sessions.c` | jest |
| przełączanie | `ksessions_hotkey_cycle()` | **ALT+SHIFT+F10** |
| sterowanie z zewnątrz | `ksessions_control(op,value)` | jest |
| wskaźnik sesji na pasku | `kernel_wm.c` (rysuje `SESSION_COUNT` kwadratów) | **jest** |

Te trzy małe kwadraty w prawym górnym rogu to **właśnie wskaźnik sesji** — wypełniony
oznacza bieżącą. Czyli „ikonka monitora" już tam jest; brakuje jej **menu**.

## 2. Co realnie blokuje dwa graficzne pulpity

Cztery rzeczy, wszystkie w jądrze:

1. **Bufor pulpitu jest jeden i globalny.** `desktop_framebuffer` + `desktop_is_active`
   — pojedyncze zmienne, nie na sesję.
2. **Lista okien nie zna sesji.** `wm_window_t` **nie ma pola `session`**, a kompozytor
   rysuje wszystkie okna. Okno z Desk 2 pojawiłoby się na Desk 1.
3. **Pule powierzchni są wspólne** (`app_surface[6]`, `wm_surface[8]`) — bez podziału.
4. **`kgfx_session_view()` prezentuje komórki terminala tekstowego.** Dla sesji
   „desktopowej" musi prezentować złożony obraz pulpitu, a nie tekst.

Do tego `wm_desktop_session` to **pojedyncza** sesja-desktop (`0xFF` = brak), a nie
flaga per sesja.

To jest cała istota pracy: **dodać przynależność sesji do okien, filtrować kompozycję
i zrobić bufor pulpitu na sesję.** Mechanizmy wejścia (klawiatura, mysz, focus) są
już rozdzielone per sesja — to zdejmuje z tej roboty może połowę.

## 3. Ile zje pamięci

**Kluczowy fakt: powierzchnia okna to ZAWSZE pełny ekran**, niezależnie od rozmiaru
okna (`wm_surface_bytes()` = `wm_surface_w × wm_surface_h`).

| element | rozmiar |
|---|---:|
| jedna powierzchnia 800×600×8bpp | **480 000 B** (469 KB) |
| pula `app_surface[6]` | do 2,88 MB |
| pula `wm_surface[8]` | do 3,84 MB |
| **razem powierzchnie** | **do 6,7 MB** (komentarz w kodzie potwierdza: „8 of them is 3.8 MB") |

Skutki dla Twojego scenariusza:

- **Drugi pulpit**: jego kanwa 480 KB + referencja kompozytora 480 KB ≈ **~0,9–1,4 MB**.
- **Każde okno aplikacji**: kolejne ~480 KB (a okno 340×240 nadal zajmuje pełny ekran!).
- **2 pulpity + 2 aplikacje**: rzędu **+3–4 MB** powierzchni plus kod i sterta aplikacji.

**Werdykt pamięciowy: zapas jest ogromny.** TF536 ma **64 MB TT-RAM**, a powierzchnie
są z TT (`wm_surface_prefer_tt()`), więc 4 MB ST-RAM zostaje nietknięte. Pamięć **nie
jest** tu żadnym ograniczeniem. Jest natomiast marnotrawstwo: okno 340×240 rezerwuje
480 KB — osobna, warta zrobienia optymalizacja (powierzchnie w rozmiarze okna).

## 4. Ile zje CPU

### Stan ustalony (dwa pulpity otwarte)
- Kompozytor rysuje **tylko aktywny pulpit** — koszt na klatkę bez zmian.
- Aplikacja na ukrytym pulpicie **nadal działa** (to osobny proces) i nadal rysuje do
  swojej powierzchni — to jej własny koszt, którego nie da się pominąć bez zatrzymania
  procesu.
- **Zysk**: rysowanie ukrytej aplikacji **nie trafia do VRAM** — nie ma kompozycji ani
  flushu. Czyli ukryta aplikacja kosztuje *mniej* niż widoczna, nie więcej.
- Dodatkowo `ksessions_pump()` raz na klatkę — rząd setek cykli, pomijalne.

### Przełączenie pulpitu
Trzeba wstawić **480 KB** (obraz nowego pulpitu) do VRAM. Przy 800×600×8 tego nie da
się uniknąć:

- prawdziwy 030 @ 32 MHz: ~120 000 zapisów 32-bit, z narzutem VRAM **~40–80 ms**,
- w Hatari: **~70–80 ms** (skalowanie z pomiaru PLASMA: 81 600 B ≈ 13 ms → ~0,16 µs/B).

To 2–4 klatki — odczuwalne „mrugnięcie", ale akceptowalne przy zmianie obszaru pracy.

### Dlaczego nie da się tego zrobić natychmiast (page flip)
Kusi, żeby trzymać oba pulpity w VRAM i przełączać wskaźnikiem ekranu:

- ET4000 ma rejestr **start address** (CRTC 0x0C/0x0D) i w naszym trybie jest on
  **zerowy i programowalny** — czyli flip jest technicznie możliwy,
- **ale to rejestr 16-bitowy**. Druga strona 800×600×8 = 480 000 B = 120 000 dwordów
  = `0x1D4C0`, a to **przekracza `0xFFFF`**. Nie zaadresujemy drugiej strony.
- Przy **640×400×8** (256 000 B = 64 000 dwordów = `0xFA00`) **mieści się** — tam
  przełączanie byłoby **natychmiastowe**.

To jest realny trade-off do rozważenia: *dwa pulpity 640×400×8 z przełączaniem
sprzętowym (0 ms)* kontra *dwa pulpity 800×600×8 z blitem 480 KB (~40–80 ms)*.

### Czy wydoli 2 aplikacje i 2 pulpity?
**Tak, z jednym zastrzeżeniem.** Wąskim gardłem nie jest liczba pulpitów, tylko:
1. **Powierzchnia widocznych okien** — kompozycja i flush rosną z tym, co widać.
2. **Aplikacje animowane w tle** — dwie PLASMY na dwóch pulpitach to **2× koszt
   rysowania**, mimo że widać jedną. To jest właściwy powód, żeby dać aplikacjom
   sygnał „jesteś niewidoczny".

Dobra wiadomość: **ten sygnał już istnieje** — `kernel_user_desktop_visible()`.
Aplikacja w tle może się zapytać i zwolnić (np. PLASMA z 27 fps na 2 fps). Bez tego
„dwa pulpity z animacją" zjedzą podwójny procesor bez żadnego zysku.

## 5. Zakres pracy

| # | zadanie | rozmiar |
|---|---|---|
| 1 | `wm_window_t.session` + filtrowanie w kompozycji i hit-testach | średnie |
| 2 | bufor pulpitu i `desktop_is_active` per sesja | średnie |
| 3 | pule powierzchni rozdzielone per sesja | małe |
| 4 | `kgfx_session_view()` dla sesji desktopowej (prezentacja złożonego obrazu) | średnie |
| 5 | start `DESKTOP.APP` w wybranej sesji („Add Desk") | średnie |
| 6 | menu na wskaźniku sesji: `Desk 1 / Desk 2 / Desk 3 / Add Desk` | małe |
| 7 | zapis konfiguracji pulpitów w `DESKTOP.CFG` | małe |

To byłaby **największa pojedyncza funkcja, jaką robiliśmy** — większa niż motywy.
Ale fundament (sesje, rozdzielone wejście, wskaźnik na pasku) już jest, więc
realnie to jest może połowa robota, którą sugerowałby sam zakres.

## 6. Proponowana kolejność — od najmniejszego ryzyka

**Etap 1 — menu na wskaźniku sesji (małe, od razu użyteczne).**
Klik w badge → `Desk 1 / Desk 2 / Desk 3 / Add Desk`. Przełączanie między istniejącymi
sesjami już działa (ALT+SHIFT+F10), więc to głównie UI: dostajesz TTS-owe przełączanie
konsola ↔ pulpit z myszką, bez ruszania architektury. **Sensowne nawet, jeśli nigdy
nie zrobimy drugiego graficznego pulpitu.**

**Etap 2 — drugi graficzny pulpit.** Wymaga punktów 1–5 (przynależność sesji w WM).

**Etap 3 — aplikacje w tle zwalniają.** Wykorzystać `kernel_user_desktop_visible()`:
ukryta aplikacja obniża tempo animacji. Bez tego dwa pulpity z animacją to podwójny
koszt CPU.

## 7. Podsumowanie liczbowe

| pytanie | odpowiedź |
|---|---|
| ile sesji obsłuży | **3** (dziś), rozszerzalne |
| ile RAM na drugi pulpit | ~1 MB (kanwa 480 KB + kompozycja) |
| ile RAM na drugie okno | ~480 KB (pełny ekran, nawet dla małego okna) |
| razem dla 2 pulpitów + 2 aplikacji | ~3–4 MB z 64 MB TT-RAM — **zapas ogromny** |
| koszt przełączenia pulpitu | 480 KB do VRAM → **~40–80 ms** (realny HW) |
| przełączenie natychmiastowe | **niemożliwe przy 800×600×8**, możliwe przy 640×400×8 |
| koszt stały dwóch pulpitów | **~zero** — komponowany jest tylko aktywny |
| realne wąskie gardło | **animacje w tle** (2× koszt rysowania) — do rozwiązania sygnałem „niewidoczny" |
| czy wydoli 2 aplikacje + 2 pulpity | **tak**, przy założeniu że aplikacje w tle zwalniają |

---

# WDROŻONE (etap 1): ikonka dwóch monitorków + lista pulpitów

## Co działa

1. **Ikonka** — dwa zachodzące na siebie monitorki, wielkości komórki czcionki
   (16×13 px) w pasku górnym, na miejscu starych trzech kwadratów. Rysowana
   prostokątami (`wm_fill_rect`), więc działa i na jasnym pasku CLASSIC, i na
   stalowym STEEL.
2. **Lista po kliknięciu** — `Desk 1` … `Desk N` + `Add Desk`, z podświetleniem
   bieżącego pulpitu. Wiersz 16 px, żeby zmieścił się font 8×16.
3. **Numerek** obok ikonki pojawia się **dopiero przy dwóch lub więcej** pulpitach
   (przy jednym ikonka stoi sama, zgodnie z życzeniem).
4. **`Add Desk`** tworzy nową sesję TTS (`ksessions_control`) i przełącza na nią —
   ten mechanizm istniał wcześniej jako tekstowe menu sesji pod prawym klawiszem;
   teraz ma normalny dropdown.

## Granica architektury, na którą trafiłem

Pasek górny jest własnością **kompozytora**, a kompozytor działa tylko
w sesji-pulpicie:

```c
if(!kgfx_desktop_active()&&!kernel_wm_desktop_session())return;
```

Skutek: po `Add Desk` lądujesz w sesji konsoli tekstowej, gdzie
- pasek nie jest odświeżany (ikonka zostaje w stanie „1 pulpit"),
- klik nie jest obsługiwany (menu nie wstaje),
- konsola maluje się **na wierzchu** paska.

Czyli sam **przełącznik** wymaga, żeby pasek żył także w sesjach konsolowych.
A **drugi graficzny pulpit** wymaga dodatkowo filtrowania okien po sesji
(`wm_window_t` nie ma pola `session`, kompozytor rysuje wszystkie okna) — to jest
etap 2 z analizy wyżej.

## Co konkretnie trzeba dołożyć (etap 2)

1. `wm_window_t.session` + filtrowanie w kompozycji i hit-testach.
2. `wm_desktop_session` → tablica per sesja; `wm_bg_done` per sesja.
3. Pasek + obsługa kliknięć aktywne w **każdej** sesji (zniesienie bramki wyżej).
4. `Add Desk` → najpierw przełączenie na nową sesję, potem
   `kernel_spawn_user_async("\\SYSTEM\\APP\\DESKTOP.APP", …)` w tej sesji.
5. Wymuszenie pełnej rekompozycji przy zmianie sesji (480 KB, ~40–80 ms).

## Efekt uboczny naprawiony

Wyłączony zegar (`WM_CLOCK_APPLET 0`) zamalowywał swój pasek kolorem `WM_PAPER`
— na stalowym pasku był to **biały prostokąt**. Usunięte: nie ma czego zmazywać,
pasek rysuje kanwa pulpitu.

Bramka: **43 testy zielone**.

---

# ETAP 2 — pomiary i stan po dwóch krokach

## Krok 1: filtr sesji w kompozytorze — ZROBIONY, koszt ZERO

**Co zmienione**
- `wm_window_t.session` + `wm_state_t.view_session` (oba domyślnie 0 → przy jednym
  pulpicie zachowanie identyczne, wszystkie testy hosta przechodzą).
- `win_shown()` w `wm_core.c` — jeden helper używany w **trzech** miejscach:
  `wm_rebuild_clip` (okno i zasłaniający), `wm_recompose` (blit), `wm_hit` (trafienia).
- `window_copy()` przenosi `session` przy podnoszeniu okna.
- Jądro: okno aplikacji dostaje sesję swojego procesu
  (`kernel_user_pid_terminal(pid)`), okno konsoli sesję 0, a `wm.view_session` jest
  ustawiane raz na klatkę z `ksessions_current()`.

**Pomiar A/B (te same warunki, 3 próbki po 5 s na przebieg)**

| build | klatki WM / 5 s | cykle pętli WM / 5 s | kompozycje |
|---|---:|---:|---:|
| **A** — bez filtra | 15 548 · 15 589 · 15 586 | 85 857 · 86 048 · 86 058 | **0** |
| **B** — z filtrem | 15 626 · 15 669 · 15 626 | 86 025 · 86 406 · 86 219 | **0** |

Różnica jest **w granicach szumu** (wariancja w jednym przebiegu to 0,3 %). Wcześniejsze
16 667 klatek/5 s pochodziło z innego uruchomienia i było szumem między przebiegami,
nie kosztem zmiany. **Filtr nie kosztuje nic mierzalnego**, a kompozytor w spoczynku
nadal robi dokładnie **0 cykli**.

**Weryfikacja wizualna**: zrzut biurka po zmianie jest **piksel w piksel** zgodny ze
znanym-dobrym stanem (0 różnic w porównywanym obszarze 600×600).

## Krok 2: „Add Desk" uruchamia pulpit w nowej sesji — MECHANIZM JEST, NIE DZIAŁA JESZCZE

**Co zmienione**
- `ksessions_autostart_desktop()` / `ksessions_take_autostart()` — flaga w module sesji.
- Wybór `Add Desk` ustawia flagę (tylko gdy `ksessions_control` się powiódł).
- Pętla główna, **po** zastosowaniu przełączenia kontekstu, woła
  `kernel_spawn_user_async("\\SYSTEM\\APP\\DESKTOP.APP", …)` — proces rodzi się
  w nowej sesji (`kernel_user_birth_terminal()`).

**Wynik testu**: sesja powstaje i następuje przełączenie, ale **drugi pulpit nie
przejmuje ekranu** — widać konsolę tekstową nowej sesji.

**Podejrzenie (do potwierdzenia jednym pomiarem)**: rejestracja pulpitu, opcode 13:

```c
if(!gui_persistent[client_session] || argument0!=gui_keyboard_pids[client_session] ||
   (gui_desktop_pids[client_session] && gui_desktop_pids[client_session]!=argument0))
    user_syscall_result=-0x3e4;
```

Czyli nowy DESKTOP.APP musi być **właścicielem GUI swojej sesji**. Skoro jest
spawnowany, gdy sesja 1 jest bieżąca, powinien się zarejestrować — więc trzeba
sprawdzić, czy spawn w ogóle dochodzi i co zwraca opcode 13 (log konsoli gościa,
nie log Hatari).

## Co dalej

1. Diagnostyka spawnu: czy `kernel_spawn_user_async` zwraca sukces i czy drugi
   DESKTOP.APP dochodzi do `GFX_DESKTOP`.
2. Jeśli blokuje rejestracja — sprawdzić `gui_persistent[]` i `GUI_BEGIN_KEYS`
   dla nowej sesji.
3. Dopiero potem strojenie wyglądu (konsola nowej sesji rysuje się na 640×400 i nie
   czyści reszty ekranu — osobny, widoczny defekt przy przełączaniu sesji).

Bramka po obu krokach: **43 testy zielone**, biurko bez zmian.

---

# ETAP 2 — wynik diagnostyki (stan: prawie działa)

## Co potwierdzone licznikami (stała diagnostyka `kdbg_desk_*`, `kdbg_batch_*`)

| pytanie | licznik | wynik |
|---|---|---:|
| Czy „Add Desk" ustawia żądanie? | `kdbg_desk_set` | **1** |
| Czy pętla główna je zabiera? | `kdbg_desk_take` | **1** |
| Czy spawn DESKTOP.APP się udaje? | `kdbg_desk_ok` / `_fail` | **1 / 0** |
| Czy nowy pulpit przechodzi rejestrację? | `kdbg_desk_rejok` | **+1** |
| W której sesji się zarejestrował? | `kdbg_desk_rejok_s[1]` | **1** ✔ |
| Czy wydaje rysowanie? | `kdbg_batch_ok[1]` | **1** |
| Czy rysowanie jest odrzucane? | `kdbg_batch_rej[1]` | **0** |

Czyli **cały łańcuch działa**: sesja powstaje, drugi DESKTOP.APP startuje, rejestruje
się jako pulpit sesji 1 i wysyła paczkę rysowania, która **przechodzi walidację**.
Pasek pokazuje numerek **„2"** — aktywna jest nowa sesja.

## Błąd naprawiony po drodze

`kgfx_session_view` przywracał **zapamiętany obraz konsoli** sesji, do której
przechodziliśmy. Sesja 1 miała ważny obraz z czasów, gdy była konsolą — więc stara
konsola malowała się na nowym pulpicie. Zmiana warunku (moje pierwsze podejście)
**złamała test** `test_session_redraw`, który pilnuje kontraktu „przywróć obraz
sesji" — słusznie. Właściwa naprawa: **`kgfx_session_forget_cells(session)`** —
gdy pulpit przejmuje sesję, jej stary obraz konsoli jest unieważniany, a kontrakt
przywracania obrazu zostaje nienaruszony.

## Co zostaje

Kanwa drugiego pulpitu składa się jako **czarna**, mimo zaakceptowanej paczki
rysowania. Podejrzenie: mapowanie okno↔powierzchnia dla drugiej sesji
(`wm_surface[1+slot]=app_surface[slot]` + dopasowanie po wskaźniku w `glue_blit`).
Paczka rysuje do powierzchni, ale kompozytor pokazuje inną (świeżo wyzerowaną).
Następny krok: porównać wskaźnik powierzchni, do której rysuje aplikacja, ze
wskaźnikiem, który kompozytor dopasowuje do okna.

## Stan systemu

- Bramka **43 testy zielone**, **zero kodu TEMP** (liczniki są stałą diagnostyką,
  jak pozostałe `kdbg_*`).
- Przy **jednym pulpicie** biurko jest **piksel w piksel** identyczne ze stanem
  wzorcowym (0 różnic).
- „Add Desk" tworzy sesję i uruchamia drugi pulpit; jego kanwa jest jeszcze czarna.
  Powrót: menu na ikonce → `Desk 1`, albo ALT+SHIFT+F10.

---

# ETAP 2 — druga runda diagnostyki: gdzie dokładnie jest problem

## Zmierzone fakty (wszystko działa aż do złożenia obrazu)

| etap | licznik | wynik |
|---|---|---:|
| drugi pulpit rejestruje się w sesji 1 | `kdbg_desk_rejok_s[1]` | **1** |
| wydaje rysowanie | `kdbg_batch_ok[1]` | **1** |
| rysowanie odrzucone | `kdbg_batch_rej[1]` | **0** |
| **ile elementów narysował** | `kdbg_items_ok[1]` | **15** |
| kompozytor się uruchamia | `kdbg_wm_composes` | **1 → 4** |
| koszt kompozycji | `kdbg_recompose_cycles` | 217 → 441 |

Czyli drugi pulpit **rysuje 15 elementów** (tło + ikony + pasek — dokładnie tyle, ile
trzeba na start), kompozytor **przebiega 3 razy**, a na ekranie jest czarno.

## Wniosek

Problem jest **wyłącznie w powiązaniu okno ↔ powierzchnia** przy składaniu:
aplikacja rysuje do swojej powierzchni (potwierdzone licznikiem elementów), ale
kompozytor blituje z innej (świeżo wyzerowanej). Podejrzane miejsce:

```c
wm_surface[1u+slot]=app_surface[slot];          /* kernel_wm_app_surface */
wm.win[w].pid=(uint32_t)(uintptr_t)app_surface[slot];
/* glue_blit szuka: wm_surface[i]==pid */
```

## Czego NIE udało się ustalić i dlaczego

Próba odczytania tablicy `wm` wprost z pamięci **nie zbiegła się**: `win[0].frame`
czytane poprawnie jako `(0,0,800,600)`, ale `count` (offset 512 wg `offsetof`)
zwracał 0, a `screen_w` (offset 1768) też 0 — mimo że `offsetof` policzony
kompilatorem hosta potwierdza te offsety. To oznacza, że albo odczyt pamięci
emulatora trafia w inny obszar, albo struktura w BSS jądra ma inny układ niż
w kompilacji hosta. **Dalsza diagnoza wymaga dumpu tablicy okien z wnętrza jądra**
(nowy opcode diagnostyczny), a nie czytania pamięci z zewnątrz.

## Stan po zakończeniu tej rundy

- Bramka **43 testy zielone**, **zero kodu TEMP**.
- Biurko przy jednym pulpicie: **0 różnic** pikselowych wobec wzorca.
- Stała diagnostyka w jądrze (przydatna dalej):
  `kdbg_desk_set/take/ok/fail`, `kdbg_desk_rejok/rejp/rejk/rejd`,
  `kdbg_desk_rejok_s[]`, `kdbg_batch_ok[]/rej[]`, `kdbg_items_ok[]`.
- Klawisze F pulpitu przywrócone do stanu wyjściowego.

## Rekomendacja na następny krok

Dodać **diagnostyczny opcode GFX** zwracający do aplikacji zrzut tablicy okien
(pid, session, flags, frame) oraz wskaźniki `app_surface[]`/`wm_surface[]`.
Wtedy widać z jednego zrzutu, czy `wm_surface[1+slot]` faktycznie równa się
`win.pid` dla drugiego pulpitu — bez zgadywania po adresach BSS.
