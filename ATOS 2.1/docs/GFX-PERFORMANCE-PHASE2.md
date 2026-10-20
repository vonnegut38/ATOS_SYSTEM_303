# ATOS 2.0 — Desktop, ETAP 2: precyzyjny pomiar i rozstrzygnięcie anomalii

Kontynuacja `docs/GFX-PERFORMANCE-PHASE1.md`. Zakres tego etapu: **zegar
wysokiej rozdzielczości w kernelu, ponowny pomiar obu profili i rozstrzygnięcie
anomalii `COPYMONO`/`VLINE`** z ETAPU 1.

---

## 1. ETAP 2.1 — zegar wysokiej rozdzielczości (ABI 1.1)

Problem z ETAPU 1: jedyny zegar dostępny aplikacji, `ticks()` (opkod 10), to
`kernel_user_yield` — kosztuje ~2 ms i przełącza zadanie; dodatkowo licznik
50 Hz ma 20 ms rozdzielczości. Sam opkod 58 (`CLOCK`) nie przełącza, ale nadal
daje tylko całe takty 20 ms, bo Timer C jest **pollingowany** (`timer_poll`
dolicza najwyżej jedno zawinięcie na wywołanie).

Rozwiązanie: nowy opkod **67 `HIRES_CLOCK`** (ABI **1.1**, zmiana addytywna):

- `system/kernel_module.c:` `kernel_user_hires_clock()` łączy licznik zawinięć
  z bieżącą fazą wolno biegnącego licznika MFP Timer C,
- jednostka: **1 zliczenie Timer C = 1/12288 s ≈ 81,4 µs** (preskaler /200,
  przeładowanie 246 — dokładnie tak, jak programuje `system/timer_module.S`),
- wynik jest **monotoniczny** (clamp), więc wyścig z zawinięciem nie cofa czasu,
- zliczenia zawinięć są nadal doliczane tylko przy odczycie, więc wołający
  rzadszy niż 20 ms może je zgubić (profiler czyta znacznie częściej).

Zmiany: `include/app_abi_version.h` (minor 0→1), `include/app_abi.h`
(`ATOS_CALL_HIRES_CLOCK=67`), `system/kernel_exception.c` (dispatch),
`include/gui_app.h` (`hires()`), `tests/test_app_abi.c`, `docs/APP-ABI.md`.
`kernel_user_system_info()` raportuje teraz najwyższy opkod = 67.

## 2. ETAP 2.2 — pomiar (best-of-3)

`GFXBENCH.APP` mierzy teraz `hires()` na każde żądanie, w 3 przebiegach po
0,5 s, i raportuje przebieg z **największą liczbą iteracji** (najmniej
zakłócony). `POLL` to koszt samego `hires()`; `net = µs/żądanie − POLL`.

### Shifter (68030 @16 MHz, STE, 640×400 mono)

| Prymityw | n | net µs/żądanie |
|---|---:|---:|
| POLL | 1100 | 455 (baza) |
| CALL pixel | 145 | 3012 |
| PIXEL | 144 | 3019 |
| HLINE 640 px | 137 | 3198 |
| VLINE 400 px | 104 | 4381 |
| FILL32 | 138 | 3184 |
| FILL128 | 123 | 3620 |
| FRAME 200×100 | 96 | 4754 |
| TEXT1 | 121 | 3686 |
| COPYMONO 320×64 | 59 | 8068 |
| COPYSHFT 300×64 | 232 | 1707 |
| FILLHALF 640×200 | 66 | 7206 |
| CLEAR 640×400 | 59 | 8108 |

### ET4000/VOFA (TF536 68030 @32 MHz, MegaSTE)

| Prymityw | n | net µs/żądanie |
|---|---:|---:|
| POLL | 2350 | 213 (baza) |
| CALL | 281 | 1572 |
| PIXEL | 294 | 1489 |
| HLINE | 280 | 1578 |
| VLINE | 126 | 3763 |
| FILL32 | 263 | 1695 |
| FILL128 | 200 | 2297 |
| FRAME | 148 | 3183 |
| TEXT1 | 254 | 1756 |
| COPYMONO | 112 | 4261 |
| COPYSHFT | 395 | 1056 |
| FILLHALF | 109 | 4375 |
| CLEAR | 87 | 5594 |

## 3. ETAP 2.3 — anomalie: wynik rozstrzygający

Dodano sondy skalujące dla kopii (Shifter, net µs):

| Sonda | rozmiar | słowa | net µs |
|---|---|---:|---:|
| CMONO1 | 320×1 wyr. | 20 | 3436 |
| CMONO16 | 320×16 wyr. | 320 | 4358 |
| COPYMONO | 320×64 wyr. (reverse) | 1280 | 8068 |
| CMONOB | 320×64 wyr. (forward) | 1280 | 7896 |
| CSHFT1 | 300×1 przes. | 19 | 3767 |
| COPYSHFT | 300×64 przes. | 1216 | 1707 |

Wnioski:

1. **Pozorny liniowy wzrost kopii wyrównanej.** Koszt rósł z liczbą słów
   (`(8068−3436)/(1280−20) ≈ 3,7 µs/słowo`), co sugerowało hotspot. Weryfikacja
   na hoście (§4.1) **tego nie potwierdziła**: `mono_copy_words` jest już dobre
   (330 ns/wywołanie), a moja alternatywa była wolniejsza i została cofnięta.
   Czyli ten wzrost to również wpływ dispatchu/rywalizacji o magistralę, a nie
   wada pętli. Kierunek przejścia (reverse/forward) i tak nie ma znaczenia
   (8068 vs 7896).
2. **Reszta pomiarów tonie w szumie dispatchu.** `COPYSHFT` (1216 słów) wyszło
   *szybciej* niż `CALL` (1 piksel), a `CSHFT1` (19 słów) *wolniej* niż
   `COPYSHFT` (1216 słów). To fizycznie niemożliwe dla samej rasteryzacji —
   znaczy, że **opóźnienie round-tripu usługi waha się systemowo ~2×** między
   wierszami (rytm 50 Hz / kwant schedulera), a nie że kopie są darmowe.
3. **Stały koszt żądania (~1,5–3,5 ms) dominuje** nad rasteryzacją dla typowych
   prymitywów. Przy 50 Hz to ~6 żądań/klatkę (Shifter) i ~13 (ET4000).

## 4. Wniosek dla architektury

Mikrooptymalizacja pojedynczych rutyn (np. `mono_copy_words`) da co najwyżej
kilka ms na dużych kopach, ale **nie usunie wąskiego gardła**, którym jest
round-trip usługi na każde żądanie. Następny krok to **API wsadowe** — jedno
żądanie = lista operacji (`gui_draw_t` już istnieje w ścieżce WM, opkod 18),
tak aby menedżer okien wysyłał jedną porcję na klatkę zamiast dziesiątek
osobnych żądań.

### 4.1 Weryfikacja na hoście (uczciwy mikrobenchmark)

Mikrobenchmark hosta (`tests/bench_emutos_copy.c`, `gmake bench-emutos-copy`)
musi wołać funkcje **przez wskaźniki `volatile`** — inaczej IPA specjalizuje
funkcję statyczną dla stałych argumentów i zaniża ją kilkukrotnie, a wynik jest
bezużyteczny (pierwsze dwa pomiary dały 2,4x i 2x różne od siebie).

Wynik wiarygodny (320×64, wywołania pośrednie):

| funkcja | ns/wywołanie |
|---|---:|
| `mono_copy_words` (obecna, produkcyjna) | 330 |
| moja wersja „ze krokiem ze znakiem" | 463 |
| `mono_copy_shifted` | 2726 |

Wnioski:

- **Implementacja `mono_copy_words` jest już dobra** — moja alternatywa okazała
  się **o 40% wolniejsza** (indeksowanie zamiast postinkrementacji wskaźnika),
  więc została **cofnięta**. To dokładnie pokazuje, czemu „działaj logicznie"
  wymaga pomiaru przed deklaracją.
- `mono_copy_shifted` jest realnie **~8x droższe** od `mono_copy_words`; to
  ono jest kosztem przesuwania okien o niezerowy offset piksela.
- Zmierzona na ATOS „powolność" `COPYMONO` była **artefaktem dispatchu**, nie
  kodu kopii. Na 68000 koszt kopii jest zdominowany przez **przepustowość
  pamięci ekranu** (kontencja Shiftera).

Realne przyspieszenie kopii to **sprzętowy Blitter** — zaimplementowany w
ETAPIE 2.4 (§5). Dodano też brakujący test różnicowy
`tests/test_emutos_copy.c` (20000 przypadków wyrównanych i przesuniętych, z
nakładaniem i strażnikami pamięci), wpięty w `make check-graphics`/`make check`.

Do precyzyjnego rozdzielenia kosztów poszczególnych rutyn potrzebny jest
**pomiar w kernelu** (bez IPC), np. rozszerzenie `kgfx_self_test`/nowy
przyrostek diag wywoływany przez SYSTEM.GFX, korzystający z `hires()`.

## 5. ETAP 2.4 — wykrycie Blittera i sprzętowa kopia

### Warunek: wykrycie Blittera w komputerze

1. `hw/machine.c: machine_detect()` — `blitter_present` z odpornego na bus
   error odczytu rejestru Blittera (`probe_read8(BLITTER_CTRL)`);
   `blitter_enabled = present`, nadpisywane przez NVRAM (`g_config.blitter`).
2. `kgfx_init()` → `kblit_init(present,enabled)`.
3. **`kblit_self_test()` to właściwy warunek użycia.** Sprawdza wypełnienie
   **oraz 12 przypadków kopii** (wyrównane, SKEW>0, SKEW<0, różne rozpiętości
   słów, nakładanie w obie strony, pojedyncze słowo docelowe, ruch w pionie) na
   małym buforze w jądrze. Wynik to dwa niezależne znaczniki:
   - `active` — wypełnienia (jak dotąd),
   - `copy_ok` — kopia.
   Nieudany test kopii **nie wyłącza fill-a**; tylko kopia wraca na CPU.
4. `kgfx_copy_rect()` (mono) próbuje `kblit_copy_planar()`, a gdy ta zwróci 0,
   spada na `emutos_vdi_mono_copy_words`/`_shifted` (nadal pokryte
   `tests/test_emutos_copy.c`).
5. Nowy bit `KGFX_CAP_BLITTER_COPY` (0x40) i komunikat startowy
   `BLITTER FILL+COPY` lub `BLITTER FILL ONLY (COPY SELFTEST n)`.

### Algorytm

`kblit_copy_planar()` to port `bit_blt()` z EmuTOS `vdi/vdi_raster.c`:
`SKEW = (d_xmin - s_xmin) & 15` plus flagi **FXSR/NFSR** z 8-przypadkowej
tabeli, endmaski z `d_xmin%16`/`d_xmax%16` (scalane, gdy `XCOUNT==1`),
`HOP=2` (source-only), `OP=3` (replace) oraz kierunek (lewo↔prawo,
góra↔dół) dobrany tak, by nakładające się źródło i cel były bezpieczne.

### Wynik (oba profile, CAPS=125 = 61 + BLITTER_COPY)

| profil | COPYMONO CPU | COPYMONO Blitter |
|---|---:|---:|
| Shifter @16 MHz | ~8,5 ms | **4,5 ms** |
| ET4000/TF536 @32 MHz | ~4,3 ms | **2,5 ms** |

- Self-test kopii zdał **wszystkie 12 przypadków** na emulowanym Blitterze w
  obu profilach (także TF536, gdzie akceleratory bywają z nim niezgodne — i
  właśnie self-test jest tą barierą).
- `COPYSHFT` jest teraz spójne z `COPYMONO` (~4,5/2,5 ms); wcześniejsze
  „1,7 ms" było artefaktem dispatchu, co potwierdza diagnozę z §4.
- Pozostałe ~3 ms (Shifter) to nadal nieusuwalny dispatch usługi.

### Fallback

Gdy Blittera nie ma lub `copy_ok==0`, `kblit_copy_planar` zwraca 0 na starcie,
a `kgfx_copy_rect` korzysta ze ścieżek CPU. Profil **bez** Blittera trudno
pokazać w Hatari (STE z `--blitter off` i tak dekoduje rejestry, a Mega ST nie
bootuje z tym BIOS-em), więc ścieżkę wyłączoną gwarantuje ten sam warunek
`!active||!copy_ok` oraz niezależnie testowane `emutos_vdi_mono_copy_*`.

### 5.1 Przyrostowy flush kompozytora

`kernel_wm_frame_tick()` kończył każdą kompozycję pełnoekranowym
`kvideo_damage(0,0,screen_w,screen_h)`, przez co `kvideo_flush()` skanował
wszystkie 400 linii, mimo że wcześniej zgłaszał damage tylko dla zmienionych
okien. Teraz damage zgłaszają same malowane regiony:

- okna — z `wm.damage[]`, jak dotąd (bez zmian),
- dekoracje (ramka + belka tytułu) — prostokąt `frame` każdego okna,
- pasek appletów — `(screen_w−66, 1, 64, 7)`,
- tło pulpitu (dither) — pełny ekran **raz**, przy wejściu w tryb pulpitu
  (`bg_done`).

`kvideo_flush()` pozostaje **na końcu** (po dekoracjach), więc nic namalowanego
po nim nie ginie.

**Ważne:** profil startuje z `COMPOSITOR (WM): DISABLED`, więc ta ścieżka jest
dziś nieaktywna — to przygotowanie pod włączenie WM. **Aktywna** ścieżka
desktopu (`kernel_graphics.c` + `kgfx_draw_batch`) już raportuje damage przez
`mark_dirty` i jest flushowana przyrostowo przez `kvideo_flush()` w pętli
głównej (`kernel_main.c:353`), więc tam nie było czego poprawiać.

### 5.2 Włączenie kompozytora za flagą (`CONFIG_WM`)

Aby nie ruszać działającego profilu tekstowego, kompozytor jądra jest teraz
opt-in:

- `Makefile`: `WM ?= 1` → `-DCONFIG_WM=$(WM)` (**domyślnie 1** od czasu,
  gdy desktop na batchu zadziałał w §5.4; `WM=0` przywraca zachowanie sprzed
  kompozytora). Komunikat startowy: `COMPOSITOR (WM): READY / ON DESKTOP ENTRY`.
- `kernel_wm.c: kernel_wm_enable()` — produkcyjny start kompozytora bez
  zapisów testowych; `kernel_wm_frame_tick()` ma dodatkową bramkę
  `if(!kgfx_desktop_active()&&!kernel_wm_desktop_session())return;`, żeby przy
  WM=1 nie malował appletów/dekoracji po konsoli w trybie tekstowym.
- `kernel_graphics.c: kgfx_enter_desktop()` wywołuje `kernel_wm_enable()`
  tylko pod `#if CONFIG_WM`; włącza to dopiero wejście w tryb desktop
  (opkod 13), więc domyślny profil jest bez zmian.

Weryfikacja:

- **WM=0 (domyślnie)**: obraz przebudowany i wgrany, boot + benchmark bez zmian
  (`CAPS=125`, `COPYMONO` ~4,5 ms, `gmake check` EXIT=0).
- **WM=1 (eksperymentalne)**: kompiluje się i faktycznie włącza kompozytor —
  po `run desktop.app` widać **zegar appletu** w prawym górnym rogu (czyli
  `kernel_wm_frame_tick` żyje), ale **pulpit jest czarny**. Legacy
  `DESKTOP.APP` rysuje bezpośrednio / przez `kgfx_draw_batch`, a przy WM=1
  `kgfx_draw_batch` kieruje rysowanie do powierzchni okna WM, której kompozytor
  nie pokazuje w tej konfiguracji. To potwierdza, że przełączenie desktopu na
  WM to osobny etap: aplikacja desktopu musi rysować **do swojej powierzchni
  WM**, a nie wprost do ramki.

Obraz `work/atos-v2.img` został przywrócony do **WM=0** (stan domyślny).

### 5.3 Ścieżka desktopu na WM — co naprawiono i co zostało

Legacy `DESKTOP.APP` rysuje **bezpośrednimi** opkodami, więc przy WM=1 dawał
czarny ekran. Zamiast przepisywać go w asemblerze, dodano batch-owego klienta
`WMDEMO.APP` (`system/wm_demo.c`): wykonuje ten sam protokół wejścia w
desktop (SYSTEM.GUI register + begin_keys, GFX opkod 13) i rysuje **jednym**
`gui_draw_t` przez opkod 18 — czyli ścieżką, której oczekuje kompozytor.

Przy tej okazji znaleziono i naprawiono **dwa realne błędy integracji**:

1. `kernel_graphics.c: kgfx_draw_batch()` **ignorował `kgfx_set_target()`** —
   backbuffer był na sztywno związany z `desktop_framebuffer`, więc batch
   zawsze rysował do ekranu, a nie do powierzchni okna WM (okna były puste).
   Teraz backbuffer jest używany tylko dla celu widocznego
   (`target==desktop_framebuffer`), a powierzchnia off-screen jest malowana
   w miejscu; w trybie bez WM zachowanie jest niezmienione
   (`||!kernel_wm_enabled()`).
2. `kernel_wm.c: kernel_wm_enable()` nie ustawiał `wm_surface_pitch/w/h`
   (robiła to tylko rejestracja konsoli), więc w trybie desktop okna
   aplikacji komponowały się z pitch 0. Teraz ustawiane przy starcie
   kompozytora (640×400 mono).

3. `kernel_wm.c: kernel_wm_console_hide()` przenosił **`win[0]`**, zakładając,
   że to okno konsoli. W trybie desktop aplikacja rejestruje swoje okno
   **pierwsza**, więc `win[0]` był oknem aplikacji i hide wysyłał je na
   `frame.x=-2048` (treść znikała/przesuwała się), a konsola zostawała na
   ekranie. Teraz hide dopasowuje okno **po powierzchni konsoli**
   (`win[i].pid==console_surface`), a nie po indeksie. To była właściwa przyczyna
   widocznego „zawijania".

Efekt (zweryfikowany zrzutem pamięci ekranu `0x10000` i zrzutem Hatari): przy
WM=1 `WMDEMO.APP` daje **poprawny pikselowo pulpit** — tło z ditheringiem,
ikony DISK/TRASH, okno „ATOS WM DEMO" na (80,40) z belką i ramką oraz zegar
appletu. Ścieżka `Desktop → batch (opkod 18) → kernel_wm_app_surface →
kompozytor` działa end-to-end.

### 5.4 `DESKTOP.APP` przepisany na batch

Legacy desktop (asembler, rysowanie bezpośrednie) zastąpiono przez
`system/desktop_wm.c` — desktop rysujący wszystko **jednym** `gui_draw_t`
(opkod 18): tło z ikonami, pasek menu, okno z belką, przycisk i ramka, plus
**przeciąganie okna za belkę** ze zdarzeń myszy SYSTEM.GUI.

- `build/system/DESKTOP.APP` buduje się teraz z `desktop_wm.c`
  (`gui_app_runtime` + `GUI_APP_CFLAGS`).
- Stary `system/desktop_app.S` **zostaje** jako `DESKTEST.APP` (jego
  self-test myszy), więc pokrycie nie znika; `gmake check` nadal przechodzi.
- Ten sam binarny desktop działa na **obu** ścieżkach:
  - `WM=0` (domyślnie, legacy): batch maluje bufor pulpitu. Poprawiono też
    `kgfx_draw_batch`, by backbuffer kopiował do **faktycznego celu**
    (`target`), a nie na sztywno do `desktop_framebuffer` — dzięki temu
    ścieżka Shiftera również się wyświetla,
  - `WM=1`: batch trafia do powierzchni okna WM i jest komponowany.

Weryfikacja: `run desktop.app` daje poprawny pulpit w obu trybach — zrzuty
`atos2-tools/shots/desktop-wm.png` oraz zrzut pamięci ekranu `0x10000`.

### 5.5 Sprzętowa kopia także dla 2/4bpp

`kblit_self_test()` uogólniono: sprawdza teraz kopię dla **1, 2 i 4 planów**
(układ interleaved, ten sam zestaw przypadków ze skew, nakładaniem i
strażnikami), a `kgfx_copy_rect()` kieruje do Blittera **każdą** liczbę
planów (fallback CPU dla mono pozostaje bez zmian). `CAPS=125` na Shifterze
(bit `KGFX_CAP_BLITTER_COPY`) oznacza, że przeszły wszystkie trzy warianty —
test jest syntetyczny na buforach w kernelu, więc waliduje 2/4bpp niezależnie
od trybu wyświetlania.

Nieudany test kopii nadal wyłącza **tylko** kopię (`copy_ok=0`), zostawiając
akcelerację wypełnień — dokładnie tak zadziałało, gdy w pierwszej wersji
pomyliłem liczbę linii testu (CAPS spadł do 61, kopie wróciły na CPU, fill
działał dalej).

### 5.6 Okno konsoli na pulpicie

W trybie desktop konsola była twardo ukrywana (`console_hidden=1`), więc
`kernel_wm_console_present()` nigdy nie rejestrowało okna. Zmiany:

- `kernel_wm_console_present()` **dopisuje** okno na końcu listy WM zamiast
  wołać `wm_init()` i wpisywać się w `win[0]` — to kasowało już zarejestrowane
  okno aplikacji (desktop znikał).
- `console_raise()` utrzymuje okno konsoli **nad** powierzchniami aplikacji
  (indeks okna i indeks powierzchni są niezależne; `glue_blit` dopasowuje
  powierzchnię po wskaźniku pid).
- `kernel_wm_frame_tick()` nie ukrywa już konsoli i woła `kconsole_flush()`
  co iterację, bo pętla główna pomija go w trybie desktop — bez tego okno
  odświeżało się dopiero po 320 znakach i pozostawało czarne.
- Okno kotwiczone na `src_y=0` (render układa siatkę 80×18 od góry
  powierzchni 640×400).

Routing klawiatury: `kernel_main.c` kieruje klawisze do shella, gdy okno
konsoli jest widoczne, więc **z pulpitu można normalnie wpisywać komendy**.

Weryfikacja: `run desktop.app`, potem `help` → okno konsoli pokazuje listę
komend i prompt `tts1 C:\>` (`atos2-tools/shots/desktop-console2.png`).

### 5.7 Fokus i klikanie okien

- Pełnoekranowa powierzchnia aplikacji (canvas desktopu) dostaje w WM bit
  „tło" (`flags&4`); `wm_hit()` ją pomija, więc przypadkowy klik nie wynosi
  jej nad okno konsoli.
- Kliknięcie w **okno konsoli** daje fokus klawiatury shellowi; kliknięcie
  poza nim wraca do aplikacji desktopu. `kernel_main.c` kieruje klawisze na
  podstawie `kernel_wm_console_focused()`.
- Okno konsoli jest normalnym oknem WM: kliknięcie belki pozwala je przesuwać
  (drag/raise z `wm_core`), a `console_raise()` trzyma je nad canvasem.

Testy: `check-wm-core` sprawdza, że okno z bitem tła nie jest hit-testowane;
`test_foreground_input` sprawdza routing klawiatury dla fokusu
shell↔aplikacja. **Ograniczenie:** harness Hatari nie pozycjonuje myszy (tylko
`doubleclick|rightdown|rightup`), więc sama interakcja kliknięcia jest
pokryta testami jednostkowymi, a nie testem end-to-end.

### 5.8 Desktop a okna aplikacji

Wcześniej aplikacja rysowała **pełnoekranowy canvas**, a WM tylko go
prezentował. Teraz:

- Nowy opkod GFX **20** (`kernel_wm_set_window(pid,x,y,w,h)`): proces zgłasza
  prostokąt okna. Broker (`gfx_service_app.S`) dopuszcza opkod 20, a
  `kernel_exception` zapisuje geometrię.
- `kernel_wm_app_surface(pid)` rejestruje wtedy **prawdziwe okno**: dekorowane
  (belka + ramka od kernela), przesuwalne (drag w `wm_core`) i hit-testowalne
  (fokus z §5.7). Bez geometrii zostaje stary pełnoekranowy canvas tła
  (bit „tło", niepodnoszony kliknięciem).
- Klient rysuje **współrzędne okna** (0,0 = lewy górny róg klienta); kompozytor
  mapuje to na `(x, y+16)`.

Autoryzacja GFX: opkody **18** (batch) i **20** (geometria) są teraz dozwolone
dla **każdego żywego klienta aktywnej sesji**, nie tylko właściciela klawiatury
GUI — inaczej druga aplikacja nigdy nie namaluje swojego okna. Weryfikacja
„aktywna sesja" nadal trzyma aplikacje z tła z dala od widocznego ekranu.

`WINDEMO.APP` (`system/windemo.c`) to demonstruje: **nie** wchodzi w desktop,
tylko zgłasza okno 300×180 i rysuje w nim przez batch. Uruchamiany w tle
(`start windemo.app`) obok działającego desktopu daje trzy niezależne obiekty
na ekranie: **tło pulpitu (WM)**, **okno konsoli (shell)** i **okno aplikacji**,
osobno komponowane, przycinane i podnoszone
(`atos2-tools/shots/desktop-multiwindow.png`).

**Wydajność (test):** kompozytor jest ograniczony do 1 kompozycji na tick
(50 Hz) i **nie robi nic, gdy nie ma damage**. Pomiar bezczynności: ~1,6
kompozycji/s (`wm_frames` +6 w 3,8 s, `wm_damage_pixels` +896 — tylko zegar
appletu). Rysowanie aplikacji to jedno żądanie wsadowe na zdarzenie, nie na
klatkę, więc koszt dispatchu (~3 ms) nie jest płacony co ramkę.

**`run` vs `start`:** `run` (pierwszy plan) po udanym spawnie próbuje
**przyłączyć** dziecko jako właściciela GUI/klawiatury. Gdy desktop już rządzi,
przyłączenie się nie udaje i shell drukuje mylące „PROCESS START FAILED" —
choć proces **wstaje** (np. `sysinfo.app` po desktopie działa w trybie
konsolowym). Drugą aplikację należy uruchamiać przez **`start`** (tło); wtedy
spawn i rysowanie okna działają. To zachowanie shella, nie kompozytora.

### 5.9 Dopracowanie UX pulpitu (konsola, kursor, odświeżanie)

- **Konsola na górze z linią marginesu**: okno konsoli to teraz
  `(0,16,640,304)` — jedna linia tekstu od góry, belka 16 px, klient 288 px =
  pełna siatka 80×18. Poprzednie 272 px ucinało ostatni wiersz (a z nim kursor).
- **Kursor**: wskaźnik myszy rysuje `kernel_wm_frame_tick()` jako **ostatnią
  warstwę** — chowany przed kompozycją, pokazywany po niej — więc jest widoczny
  na pulpicie i nie jest nadpisywany przez okna. Kursor tekstowy konsoli
  (podkreślnik) też się mieści dzięki pełnym 18 wierszom klienta.
- **Tylko zmieniony obszar**: usunięto unieważnianie cache komórek konsoli z
  `kgfx_set_target()` oraz z obsługi opkodu 18. Wcześniej **każde** rysowanie
  aplikacji kasowało cache, więc kolejna prezentacja konsoli renderowała całą
  siatkę i zgłaszała damage całego okna. Teraz `kgfx_console_render()` sam
  pilnuje ważności per-cel (`last_target`), a prezentacja konsoli dodaje damage
  tylko dla zmienionego pasa (`dx,dy,dw,dh`).

### 5.10 Minimalizacja i lista zadań (pasek górny)

Zamiast klasycznego paska zadań: **ikona w pasku** (obok zegara) otwiera
**listę wszystkich okien** (tytuł + stan), będącą jednocześnie przełącznikiem
zadań i schowkiem na zwinięte.

- `wm_window_t`: pole `title[24]`, flaga **bit 3 = zminimalizowane** oraz
  `restore` (zapamiętana geometria).
- **GFX opkod 21** = tytuł okna; przechowywany **per-PID** i nakładany przy
  rejestracji okna, więc aplikacja może ustawić go **przed pierwszym
  rysowaniem** (inaczej okno jeszcze nie istnieje).
- `wm_hit_part`: nowa część `WM_PART_MINIMIZE` — prawy górny kwadracik belki
  (16 px).
- `frame_tick`: klik w ikonę otwiera/zamyka listę; klik w wiersz przywraca i
  podnosi okno; klik w gadget minimalizacji chowa okno (flaga 8, widoczność
  wyczyszczona) i dodaje jego prostokąt do damage. Konsola ma tytuł `SHELL`,
  a `WINDEMO` ustawia `WINDEMO` przez opkod 21.
- Ikona i lista rysowane są **po kompozycji** (żeby ich nie nadpisać), a damage
  obejmuje wyłącznie prostokąt ikony/listy — zgodnie z „tylko zmieniony obszar".

Testy: `check-wm-core` sprawdza gadget minimalizacji oraz to, że
zminimalizowane okno nie jest hit-testowane.

### 5.11 Zamykanie okna i przelaczanie klawiszem

- **Zamykanie**: prawy skraj belki (16 px) to gadget **close**, sasiednie 16 px
  to **minimize** (**WM_PART_CLOSE** / **WM_PART_MINIMIZE**). Zamkniecie usuwa
  okno z listy WM, zwalnia jego powierzchnie i dodaje do damage jego prostokat;
  tla pulpitu nie mozna zamknac.
- **Przelaczanie klawiszem**: nowy opkod GFX **22** = **kernel_wm_cycle()** —
  podnosi kolejne okno (przywracajac zwinięte). Desktop (**DESKTOP.APP**) wola go
  po **F9**; opkod, jak 18/20/21, jest dostepny dla kazdego klienta aktywnej sesji.
- Gadzety sa **narysowane** na belce (dwa kwadraciki + kreska minimize), zgodnie
  ze strefami klkania.
- **wm_window_copy()** kopiuje okna w **window_close()** przez **volatile** —
  kernel jest freestanding, a zwykle przypisanie struktury sciagalo **memcpy**.

Testy: **check-wm-core** sprawdza obie czesci belki (CLOSE = prawy skraj,
MINIMIZE = nastepne 16 px); **check-wm-core** i **check-wm-blit** sa teraz w
**make check**. Ograniczenie: harness nie rusza myszy ani nie wysyla F-klawiszy,
wiec sama interakcje pokrywaja testy jednostkowe.

### 5.12 Deduplikacja tla i odswiezanie tylko zmienionego fragmentu

- **Bez dublowania tla**: gdy dziala aplikacja-pulpit (pelnoekranowy canvas,
  flaga 4), `glue_blit(pid=0)` **nie maluje juz ditheringu WM** — canvas i tak
  pokrywa ekran, wiec dither byl tylko nadpisywany (marnowana praca + migotanie).
- **Lista zadan bez canvasu**: tlo pulpitu nie jest pozycja na liscie (nie ma po
  co sie do niego przelaczac). Wiersze listy sa mapowane przez `task_rows[]`,.
- **Tylko zmieniony fragment**: `kernel_wm_set_title()` nie wola juz
  `kernel_wm_damage_all()` (pelny ekran) — odswieza wylacznie pasek zadan
  (`task_draw()`, ikona + lista).
- **Znaleziony blad**: galez pelnoekranowego canvas **nie ustawiala flag okna**
  (zostawaly wartosci po poprzednim oknie), co tullo sie z nieprzewidywalnym
  komponowaniem. Teraz `flags=7` jest ustawiane jawnie.

Przy okazji: `kernel_wm_damage_all()` zostaje tylko w miejscach jednorazowych
(start kompozytora, wejscie w tryb desktop), a nie w sciezce rysowania.

### 5.13 Przelacznik sesji (tts): znacznik tekstowy -> ikona graficzna

Znacznik `[1]v` w prawym gornym narozniku to **przelacznik sesji (tts)**. W trybie
tekstowym zostaje bez zmian; w trybie graficznym (pulpit/WM) jest zbedny jako
tekst i dublowal pasek, wiec zostal zastapiony **ikona graficzna**:

- `ksessions_paint()` w trybie graficznym (`kgfx_desktop_active()`) **nie rysuje
  juz tekstu** — wola `kgfx_overlay_hide()` i wychodzi. Rozwiniete menu sesji
  pozostaje tekstowe, wiec przelaczanie dziala jak dotad.
- Kompozytor rysuje w pasku gornym **plakietke sesji**: po jednym malym kwadracie
  na sesje (docelowo 3). Biezaca sesja jest **wypelniona**, pozostale to puste
  ramki. Rysuje to `task_draw()` (obok ikony listy zadan), tylko w tym pasie.
- **Ikona listy zadan** ma teraz zawsze widoczna biala ramke (wczesniej byla
  czarna na czarnym pasku, gdy nic nie bylo zminimalizowane).
- Klik nie otwiera juz menu w `x>=576` w trybie graficznym, tylko w obszarze
  plakietki (`534..560, y<16`); w trybie tekstowym zostaje `x>=576`.
- Po przelaczeniu sesji `kernel_wm_refresh_bar()` odswieza **tylko** pasek
  (plakietke zadan i sesji) — bez pelnego przemalowania pulpitu.

Weryfikacja: zrzut z pamieci ekranu pokazuje trzy kwadraty (1 wypelniony, 2 puste),
ikone listy zadan i zegar; zrzut trybu tekstowego nadal pokazuje `[1]v`. Test
`check-sessions` dostal stuby `kgfx_desktop_active`/`kernel_wm_enabled`/
`kernel_wm_refresh_bar` oraz przypadek na ukrycie nakladki i odswiezenie paska.

### 5.14 Puste okno aplikacji — Z-order i damage powierzchni (naprawione)

Objaw: okno aplikacji (np. `WINDEMO.APP`) pokazywalo sama biala powierzchnie i
ramke WM, bez tresci, ktora narysowala aplikacja — i to nieregularnie („raz jest,
raz nie"). Zrzut z pamieci ekranu i odczyt symboli (`m68k-elf-nm`) pokazaly, ze
**powierzchnia aplikacji zawiera tresc** (6312 niezerowych bajtow, wypelnienie
`0xFF`), wiec blad byl w kompozycji, nie w rysowaniu. Dwie przyczyny:

1. **Brak zgloszenia damage po rysowaniu do powierzchni okna.** Aplikacja rysuje
   wsad (opkod 18) do wlasnego surface'u (`kernel_wm_app_surface`), ale kompozytor
   czyta surface tylko dla okien objetych damage. Rysowanie **nie dodawalo damage**,
   wiec tresc pojawiala sie dopiero, gdy przypadkowa inna zmiana wymusila
   rekompozycje (stad nieregularnosc). Dodano `kgfx_batch_bounds()` (zwraca obszar
   ostatniego wsadu) i `kernel_wm_app_dirty(surface,l,t,r,b)`, ktore zglaszaja
   kompozytorowi dokladnie ten fragment — bez pelnego przemalowania pulpitu.
2. **Zla kolejnosc Z.** `kernel_wm_app_surface` wolalo `console_raise()` dla
   **kazdej** nowej powierzchni, wiec duze, w wiekszosci puste okno konsoli
   przykrywalo nowe okno aplikacji — widac bylo biala powierzchnie konsoli, a
   ramka okna byla dorysowywana na wierzchu (dekoracje rysowane sa po zlozeniu).
   Teraz `console_raise()` dotyczy **tylko** pelnoekranowego canvasu pulpitu
   (flaga 4); okno dekorowane (opkod 20) zostaje na wierzchu.

Przy okazji: `wm_raise()` (klik = fokus) **gubil tytul okna i zapisany prostokat
minimalizacji**, bo `window_copy()` ich nie kopiowalo — okno tracilo nazwe na
liscie zadan po podniesieniu. Uzupelnione; `check-wm-core` sprawdza teraz, ze
podniesienie zachowuje `title` i `restore`.

Weryfikacja: zrzut z pamieci pokazuje pelna tresc okna (`APP WINDOW`,
`SEPARATE FROM DESKTOP`, wewnetrzna ramka, `KERNEL WM OWNS THE FRAME`) zarowno
gdy pulpit dziala, jak i gdy widoczna jest konsola.

### 5.15 Pomiar „żądania na klatkę" (ETAP 2 §6.1)

Budżet z ETAPU 1 („~6 żądań na klatkę na 16 MHz") byl **szacunkiem** z kosztu
dispatchu. Teraz jest **pomiar**:

- `kernel_graphics.c`: `kgfx_note_request()` — broker (`call==42`) zlicza każde
  obsłużone żądanie graficzne w `gfx_requests` (`kgfx_request_count()`).
- `kernel_wm.c`: przy każdej **złożonej** klatce kompozytor próbkuje licznik i
  liczy `wm_requests_last` (żądania w tej klatce), `wm_requests_max` (szczyt) i
  `wm_requests_total`; `kernel_wm_request_stats()` je wystawia.
- `system_report_t` v3: `wm_requests_total`, `wm_requests_last`,
  `wm_requests_max` — SYSINFO pokazuje je w wierszu kompozytora
  (`REQ/FRAME:n MAX:m`), obok `FRAMES`/`DAMAGE`.

Pomiar (`atos2-tools/scenario_req_per_frame.py`, adresy symboli z `m68k-elf-nm`):

| Punkt | gfx_requests | max/klatkę | klatki |
|---|---:|---:|---:|
| po `start desktop.app` | 2 | 2 | 14 |
| po `start windemo.app` | 5 | 3 | 33 |
| po 1 s bezczynności | 5 | 3 | 34 |

Wniosek: **API wsadowe działa** — cały pulpit to 2 żądania, a dołożenie okna
aplikacji to 3 (geometria + tytuł + wsad). Średnio **0,15 żądania na klatkę**,
czyli ~40× poniżej szacowanego budżetu; wąskie gardło dispatchu z ETAPU 1
przestało obowiązywać na ścieżce pulpitu. Kolejny cel to rasteryzacja
(`mono_copy_words`, `emutos_vdi_vertical`) oraz ET4000.

### 5.16 Zegar HH:MM — odswiezanie raz na minute

Zegar w pasku gornym pokazywal `HH:MM:SS`, a kompozytor rysowal applet **w
kazdej iteracji petli glownej** (dwa miejsca w `kernel_wm_frame_tick` plus
`kernel_wm_applet_bar`), wiec bramka „na sekunde" w `kernel_wm_applet_bar` byla
i tak obchodzona. Zmiany:

- `mod_clock.c`: `clock_format()` daje **`HH:MM`** (5 znakow); `clk_draw()`
  maluje teraz **takze tlo** kazdej komorki glifu (0/1), bo przy odswiezaniu raz
  na minute cyfra, ktora sie uproscila, musialaby zostawic stare piksele.
- `kernel_wm.c`: jeden wspolny `applet_draw()`; usunieto rysowanie appletu z
  petli per-klatka. Pasek gorny jest teraz odswiezany **tylko** gdy:
  1. sklad kompozycji go zakryl (`topstrip_damaged()` — canvas pulpitu obejmuje
     caly ekran), albo
  2. zmienila sie **minuta** (`kernel_wm_applet_bar()`, `now/3000`).
  `kernel_wm_applet_bar()` ma tez bramke trybu desktop (w trybie tekstowym nie
  rysuje zegara nad znacznikiem sesji).

Weryfikacja: tymczasowy licznik `applet_draws` przez 12 s bezczynnego pulpitu
pokazal **2 rysowania i zero zmian** (wczesniej tysiace, ~50/s). Zrzut paska:
`▣ ▢ ▢` (sesje), ikona listy zadan i `00:00`. Test `check-mod-clock`
sprawdza format `HH:MM` oraz brak „duchow" po przejsciu `00:00 -> 10:00`.

### 5.17 Zegar z RTC (z fallbackiem 12:00 AM)

Zegar byl licznikiem czasu od startu. Teraz jest sciagany z RTC raz, a potem
posuwany licznikiem 50 Hz:

- `system/kernel_rtc.c`: `kernel_rtc_read()` probuje kolejno
  1. **zegara IKBD** (ST/STe): komenda `$1C`, 6 bajtow BCD. Odczyt maskuje
     przerwania (`kernel_irq_mask()`/`kernel_irq_restore()` w
     `kernel_entry.S`) i **polluje ACIA** — inaczej poziom-6 handler ACIA
     skradlby bajty odpowiedzi do pierscienia wejscia, gdzie nie da sie ich
     odroznic od pakietow myszy.
  2. **RP5C15** (Mega ST/Mega STe) pod `$fffc21`.
- `mod_clock.c`: `clock_set_time()` ustawia godzine startowa; `clk_tick()`
  dolicza sekundy z licznika i re-anchoruje bazę, gdy licznik sie cofnie.
- `kernel_wm.c`: przy pierwszej inicjalizacji appletu odczyt RTC zasila zegar;
  **brak RTC = 12:00 AM** (`00:00`), wiec applet dziala na kazdej maszynie.

Uwaga sprzetowa: Hatari **zeruje zegar IKBD przy zimnym resecie** (i nie
aktualizuje go, gdy miesiac=0), wiec na profilu STe w emulatorze odczyt zwraca
00:00 i to jest wlasnie fallback. Na maszynie z podtrzymywanym RTC (Mega
ST/STe,TT/Falcon) albo na prawdziwym STe z ustawionym zegarem wskaze realna
godzine. Test `check-mod-clock` sprawdza seed i posuwanie; obsługa NVRAM
(MSTE/TT/Falcon) jest do dolozenia tak samo jak RP5C15.

### 5.18 Wyglad pulpitu w stylu EmuTOS (biale menu, paleta, ramki)

Ustalenie palety mono (potwierdzone zrzutem ekranu, nie zrzutem pamieci):
**kolor 1 = bialy, kolor 0 = czarny**. Wczesniej menu bylo czarne, a napisy
"FILE"/"OPTIONS" rysowaly sie jako biale bloki, bo `add()` ustawialo
`background=1` przy `color=1` (tlo = atrament). Poprawki:

- `desktop_wm.c`: pasek menu = **bialy** (kolor 1), napisy **czarne** (kolor 0,
  tlo biale) — jak w EmuTOS.
- `kernel_wm.c`: motyw `WM_INK=0` / `WM_PAPER=1`; plakietka sesji, ikona listy
  zadan, ramki okien i gadzety rysowane **czarnym atramentem na bialym tle**
  (wczesniej biale na czarnym). Aktywny pasek tytulu = bialy, nieaktywny =
  czarny (odpowiednik `TOPPED_COLOR`/`UNTOPPED_COLOR`; EmuTOS uzywa dithera —
  do dopracowania).
- `mod_clock.c`: `clock_set_colors(ink,paper)` — zegar jest czarny na bialym
  pasku; domyslnie nadal bialy na czarnym, wiec modul zostaje samodzielny.
- **Sprzatanie**: `kernel_wm_console_hide()` **nigdy nie bylo wolane**, wiec okno
  shella zostawalo na pulpicie. Teraz opkod 13 (wejscie w tryb desktop) je
  ukrywa i widac skomponowany pulpit (tlo EmuTOS, ikony DISK C/D, TRASH, okna).

Efekt (zrzut): bialy pasek menu z czarnym `FILE`/`OPTIONS`, ditherowe tlo,
ikony dyskow/kosza i okno z ditherowanym paskiem tytulu i gadzetami.

Do dokonczenia (kolejny krok): rozwijane **menu** w stylu EmuTOS, glify
gadzetow (closer/fuller/sizer) i rysowanie **tytulu okna aplikacji** na pasku
(w dzisiejszym WM tytul jest tylko na liscie zadan).

### 5.19 Jeden pulpit: auto-start, launcher i aplikacje w oknach

Zamiast zmuszac uzytkownika do wpisywania `desktop.app` i zmagac sie z drugim
pulpitem:

- **Auto-start**: `kernel_main.c` po `shell->start()` sam spawnuje
  `\SYSTEM\APP\DESKTOP.APP`. Pulpit zostaje wlascicielem GUI i wchodzi w tryb
  desktop; shell dalej zyje na tts1. Brak pliku = spawn zawodzi i zostaje tryb
  tekstowy (bezpieczny fallback).
- **Singleton**: drugi desktop nie powstaje. `gui_desktop_pids[sesja]` i
  `gui_keyboard_pids[sesja]` w kernelu odrzucaja rejestracje (opkod 13 /
  GUI_BEGIN_KEYS), a DESKTOP.APP wypisuje `DESKTOP ALREADY RUNNING` i konczy sie
  kodem 0 (zamiast mylacego `PROCESS START FAILED`).
- **Launcher w DESKTOP.APP**: biale menu `FILE` z rozwijanym menu
  (F10 lub klik):
  - `OPEN TERMINAL` — nowy opkod GFX **24** wolajacy `kernel_wm_console_show()`,
  - `RUN...` — pole tekstowe; `ATOS_CALL_SPAWN_ASYNC` (syscall 34) uruchamia
  `\SYSTEM\APP\<NAZWA>.APP` jako **okno na tym samym pulpicie**,
  - `CLOSE` — wyjscie z pulpitu (powrot do shella tekstowego).
  Broker GFX rozszerzony do opkodu 24.
- **Naprawiony blad**: aplikacja odrzucala komunikaty z `sender==0`, a tak kernel
  wstrzykuje `KKEY`/`KMOU` — dlatego na pulpicie **nie dzialal zaden klik ani
  klawisz**. Teraz sender 0 jest akceptowany. To bylo zrodlo "martwego pulpitu".

Weryfikacja (zrzuty): boot ladauje wprost na pulpicie (`desktop_is_active=1`);
F10 otwiera menu; `RUN...` + `WINDEMO` + Enter otwiera okno WINDEMO z pelna
trescia na jednym pulpicie; menu wypisuje `STARTED`.

Do dopracowania: tresc/fokus okna konsoli po `OPEN TERMINAL` (okno wstaje, ale
shell nie jest jeszcze wypelniony i nie ma fokusu klawiatury) oraz powrot do
tekstu przez `CLOSE`.

### 5.20 Terminal na pulpicie i wyjscie z pulpitu

Domkniecie launchrera:

- **`OPEN TERMINAL` dziala z trescia i fokusem.** Trzy bledy:
  1. `kconsole_set_grid(80,18)` **czyscilo** cala konsole przy zmianie geometrii.
     Komorki i tak maja staly stride `TERM_COLS` (80), wiec czyszczenie bylo
     zbedne — teraz tylko przycina kursor i zachowuje tresc.
  2. `kernel_wm_console_hide()` parkowalo okno na x=-2048, a `console_show`
     tylko zdejmowalo flage (mogl powstac duplikat). Teraz okno (dopasowane po
     `console_surface`) jest przesuwane z powrotem i podnoszone, a
     `kgfx_console_force_redraw()` wymusza pelny render siatki.
  3. Okno pokazywalo **gore** siatki; prompt jest na dole. `src_y` ustawione na
     ostatnie 18 wierszy (`(TERM_ROWS-18)*16`), jak terminal.
  `kernel_wm_console_focus()` kieruje klawiatury do shella
  (`console_focused=1`). Zweryfikowane: `OPEN TERMINAL` + `dir` + Enter
  wypisuje listing, prompt `tts1 C:\>` widoczny.
- **`CLOSE` wraca do trybu tekstowego.** Petla aplikacji nie sprawdzala `quit`
  po `continue`, wiec wybor CLOSE nic nie robil. Dodane `if(quit)break;` w
  sciezce klawisza i myszy. Zweryfikowane: pulpit konczy sie (`[PID 4 EXIT 0]`)
  i wraca text shell z `[1]v`.

Kosmetyka do dolozenia: podswietlenie wybranej pozycji menu (proba z inwersja
wiersza pogubila napisy — wycofana; trzeba dodac pole tla do `gui_draw_t` albo
osobny wariant textu).

### 5.21 Menu z podswietleniem i glify gadzetow okna

- **Podswietlenie pozycji menu**: inwersja wiersza (bialy tekst na czarnym tle)
  przez pole `background` textu nie rysowala napisow, wiec wybrana pozycja jest
  teraz oznaczona **czarna ramka** (`GUI_FRAME`) wokol wiersza — czytelne i
  niezawodne na monochromatycznym ekranie.
- **Glify gadzetow okna**: prawy gadzet (zamkniecie) dostal glif **EmuTOS
  closer** (`\005` z czcionki systemowej) rysowany `kgfx_text`; lewy
  (minimalizacja) zostaje belka. Kolory zgodne z motywem (`WM_INK` na
  `WM_PAPER`). Zweryfikowane na oknie WINDEMO otwartym z launchrera.

### 5.22 Tytul okna na pasku + optymalizacja dekoracji

- **Tytul okna aplikacji** jest rysowany na pasku tytulu (`kgfx_text`): czarny na
  bialym pasku aktywnego okna, bialy na czarnym pasku nieaktywnego; przycinany do
  szerokosci paska minus gadzety.
- **Naprawiony blad (3 warstwy)** — tytul pokazywal domyslne `APPLICATION`:
  1. `kernel_wm_set_title()` szukalo okna po `wm.win[i].pid==pid`, ale WM
     identyfikuje okno **wskaznikiem powierzchni**. Dodano `app_surface_for(pid)`
     i dopasowanie po powierzchni.
  2. `user_string()` w opkodzie 21 walidowalo wzgledem **biezacego procesu**
     (SYSTEM.GFX), a nie klienta, wiec wskaznik tytulu zawsze odpadal
     (`TITLE BADPTR`). Teraz szukamy właściciela po `argument0` i walidujemy
     kazdy bajt przez `user_process_range(owner,...)` — tak jak opkod 18.
  3. `kernel_wm_set_title()` dodaje do damage tylko pas tytulu (wczesniej
     odswiezal liste zadan).
- **Optymalizacja dekoracji**: próg `win_dirty[WM_MAX]` liczony **przed**
  `wm_recompose` (zanim damage zostanie wyczyszczony); dekoracje (ramka, tytul,
  gadzety) sa rysowane tylko dla okien, ktorych `frame` przecina te kompozycje.
  Okna nietkniete zostawiaja piksele z poprzedniej klatki. Wczesniej kazde okno
  bylo przerysowywane co klatke.

Weryfikacja: okno WINDEMO otwarte z launchrera ma pasek z **`WINDEMO`** i
gadzetami. Uwaga: `RUN...` sporadycznie zwraca `RUN FAILED` (spawn), mimo ze
plik istnieje — do zbadania (prawdopodobnie pamiec/zakres).

### 5.23 Mysz na pulpicie (delta vs pozycja absolutna)

Objaw: na pulpicie nie dalo sie ruszyc kursorem. Przyczyna: sesyjny
`kernel_user_mouse_event()` zwraca aplikacji GUI **delte ruchu**
(`forward_x/forward_y`), a `kernel_wm_frame_tick()` bral to jako pozycje
absolutna (`wm_mouse_x=mx`). Kazde zdarzenie ustawialo wiec kursor na kilku
pikselach od (0,0) i wygladal na zamrozony; hit-testing okien trafial w zle
wspolrzedne.

Poprawki:
- `kernel_wm.c`: w obsludze myszy WM bierze **absolutna** pozycje z sesji
  (`ksessions_mouse_x()/ksessions_mouse_y()`) i uzywa jej takze do
  `wm_hit/wm_hit_part` i `task_click`.
- `cursor_sync()` nie jest juz bramkowane `wm_mouse_valid`: zawsze ustawia
  wskaznik na pozycji sesji, wiec kompozycja, ktora schowala kursor, przywraca
  go we wlasciwym miejscu (wczesniej po pierwszym zlozeniu kursor znikal).

Weryfikacja: `wm_mouse_x/y == ksessions_mouse_x/y` (odczyt z pamieci, obie
wartosci rowne, np. 33/9). Wczesniej WM mial delte (~0).

Uwaga: `RUN...` z launchera dziala; pojedynczy `RUN FAILED` byl artefaktem
zbyt szybkiego wpisywania (zgubiony znak → zla nazwa pliku). 3/3 startow z
normalnym tempem: `STARTED` + okno aplikacji.

### 5.24 Stylizacja Win98: fazowana ramka (bevel)

Dodany dwukolorowy bevel 3D:
- `gui_draw.h`: nowy typ wsadu `GUI_BEVEL` (`color`=atrament, `background`=papier,
  `reserved`=0 wzniesiony / 1 wklesly).
- `kernel_graphics.c`: `bevel_rect()` — **czarny obrys zewnetrzny** + wewnetrzny
  **highlight** (papier) na gorze/lewej i **cien** (atrament) na dole/prawej.
  Obrys jest konieczny, bo na bialym tle sam bialy highlight jest niewidoczny.
- `wm_blit.c`: `wm_bevel_rect()` — to samo dla kompozytora (WM).
- Zastosowanie: ramka okna i gadzety (WM) oraz rozwijane menu i przyciski
  (DESKTOP.APP, `GUI_BEVEL`).

Efekt: okno ma gruba, fazowana ramke w stylu Windows 98; gadzety minimalizacji i
zamkniecia sa wzniesionymi przyciskami; menu ma obramowanie z cieniem.

### 5.25 RTC: NVRAM dla Mega STE / TT / Falcon

Zegar zna teraz trzy rodziny RTC, probowane w kolejnosci:
1. **IKBD** (ST/STe) — komenda `$1C`, 6 bajtow BCD.
2. **RP5C15** (Mega ST / Mega STE) — `$fffc21`.
3. **NVRAM MC146818** (Mega STE / TT / Falcon) — `$ffff8961` (select) /
   `$ffff8963` (data): sekundy 0, minuty 2, godziny 4, dzien 7, miesiac 8,
   rok 9, rejestr B 11 (bit 2 = tryb binarny, bit 7 = SET).

Kluczowe: strona NVRAM jest **niezmapowana na ST/Mega ST**, wiec odczyt jest
bramkowany typem maszyny (`kernel_machine_type()` — nowy getter w
`kernel_module.c`) i probowany tylko dla `MACHINE_MEGA_STE/TT/FALCON`.
Wczesniej odczyt NVRAM na ST mogl skonczyc sie bus errorem.

Brak RTC nadal daje fallback 12:00 AM, wiec zegar dziala na kazdej maszynie.
Uwaga: weryfikacja na MSTE/TT/Falcon wymaga odpowiedniego profilu Hatari
(domyslny harness uzywa `--machine ste`); kod jest wiernym portem
`hw/rtc.c::read_nvram`, ktory jest referencja projektu.

### 5.26 Przeciaganie okna zarysem + wcisniete przyciski (Win98)

- **Przeciaganie bez zawartosci**: zamiast `wm_drag_update()` (ktore przesuwalo
  okno na zywo) WM prowadzi wlasny stan `drag_*` — przy chwyceniu paska tytulu
  zapisuje offset i rozmiar, a podczas ruchu liczy tylko nowa pozycje i rysuje
  **przerywany zarys 1px** (`drag_outline`). Zawartosc okna stoi w miejscu.
  Dopiero na puszczenie `wm_move()` przenosi okno raz.
  - Zarys jest rysowany **po** kompozycji (nad oknami); poprzednia klatka jest
    czyszczona przez damage starego i nowego prostokata, co tez wymusza
    kompozycje (inaczej przy braku damage zarys by sie nie pojawil).
- **Wcisniete przyciski**: gadzet pod przyciskiem nie dziala juz na `press`,
  tylko pokazuje stan **sunken** (`wm_bevel_rect(...,sunken=1)`) i wykonuje
  akcje na **release**, o ile kursor wciaz jest nad tym samym gadzetem —
  zachowanie jak w Windows 98.

Uwaga: oba dzialania wymagaja ruchu myszy, ktorego harness nie potrafi
wstrzyknac; weryfikacja wizualna statyczna jest OK (okno/tytul/gadzety bez
regresji), a przeciaganie i wciskanie trzeba sprawdzic na zywo.

### 5.27 Menu: podswietlenie przez rewers kolorow

Zaznaczona pozycja menu jest teraz rysowana **inwersja** (czarny pasek + biale
glify), a nie ramka:
- `GUI_FILL` czarny na wiersz + `GUI_TEXT` z `color=1` (papier) i
  `background=0` (atrament).
- Znaleziony problem: etykiety z tablicy `static const char *menu_label[]` nie
  renderowaly sie przy `background=0`, mimo ze ten sam wzorzec z literałem
  dzialal, a `mono_glyph` przechodzi test hostowy dla fg=1/bg=0. Rozwiazanie:
  etykiety sa teraz **literalami** wybieranymi ternarnie. (Podejrzenie: sekcja
  danych tablicy wskaznikow w obrazie .APP; do zbadania, ale literały dzialaja.)

### 5.28 Przeciaganie zarysem: damage tylko obrysu (a nie calego okna)

W pierwszej wersji zarysu do listy damage trafial **caly prostokat okna** (zeby
wymusic kompozycje i zmazac poprzednia ramke). To znaczylo, ze kompozytor
re-blitowal obszar wielkosci okna (~W*H px) na klatke - czyli tyle samo, co przy
ruchu "z zawartoscia".

Teraz outline_damage() zglasza **cztery cienkie paski** (1px) obrysu, a nie
wypelniony prostokat. Koszt ruchu spada z ~W*H do ~2*(W+H) px na klatke
(dla okna 320x200: 64000 -> ~1040 px, ok. 60x mniej). WM_DAMAGE_MAX podniesione
z 8 do 16, bo stary i nowy obrys daja do 8 osobnych paskow.

### 5.29 Blit okna: szybka kopia mono z przesunieciem bitowym

Prawdziwe waskie gardlo kompozycji: gdy `source_x` i `destination_x` mialy
rozny offset w bajcie (typowe okno — klient zaczyna sie np. na x=296), stara
`wm_copy_rect()` schodzila do petli **piksel po pikselu**. Dla okna 320x168 to
~54 tys. iteracji na jeden blit.

Teraz `mono_span_copy()`:
- gdy oba offsety sa **wyrownane do 8**, kopiuje bajtami z **32-bitowym srodkiem**,
- w przeciwnym razie przesuwa bity: **8 pikseli na krok** (destynacja po bajcie,
  zrodlo przez 16-bitowe okno `so`), z ogonem <8 px na koncach.

Test hostowy `check-wm-blit` przechodzi teraz **wszystkie fazy (sx,dx)** wzgledem
referencji pikselowej. UBSan wychwycil przy tym realny blad: 32-bitowy odczyt ze
zrodla mogl byc niewyrownany — na 68000 to **address error** (fault na 030).
Warunek kopiowania bajtami wymusza teraz wyrownanie **obu** wskaznikow.

To jest "mocna optymalizacja na 030": mono blit okna idzie ~8x szybciej w
najgorszym przypadku (przesuniete) i ~32x gdy trafia sie wyrownane rzedy, bez
zadnego nowego bufora.

### 5.30 Blitter cross-buffer + skalowanie zarysem

**Blitter miedzy bitmapami** (`kblit_copy_planar_to`): dotychczasowy
`kblit_copy_planar` kopiowal w obrebie jednego bitmappu (uzywany przez
`kgfx_copy_rect`). Teraz istnieje wariant src->dst z osobnymi `line_words`;
stara funkcja to cienki wrapper. `glue_blit` uzywa go dla prostokatow
>= 960 px (mniejszym nie oplaca sie programowac rejestrow), a gdy Blittera
**nie ma lub nie przeszedl self-testu**, `kblit_copy_planar_to` zwraca 0 i
wchodzi szybka sciezka CPU (`wm_copy_rect`/`mono_span_copy`).

Weryfikacja: pulpit + okno WINDEMO renderuja sie identycznie **z Blitterem**
i **bez** niego (`--blitter off`).

**Skalowanie zarysem**: przeciaganie krawedzi okna (`WM_PART_FRAME`) prowadzi
stan `rsz_*` i rysuje przerywany zarys docelowego rozmiaru; dopiero na
puszczenie `wm_resize()` zmienia geometrie. Minimalny rozmiar 64x50, przyciete
do ekranu.

### 5.31 Przesuwanie i skalowanie okien — domknięcie

Trzy błędy blokowały drag/resize end-to-end:

1. **Zagłodzenie nadzorcy przez `pause_tick()`.** `kernel_user_sleep_context`
   zwracał 0, gdy nie było innego gotowego procesu, więc `pause_tick()` stawał
   się no-opem dla ostatniego runnable procesu: pętla `for(;;)pause_tick()`
   (np. WINDEMO) nie wracała do głównej pętli i zamrażała routing klawiatury
   oraz kompozytor. Teraz uśpienie **zawsze** parkuje proces i wraca do
   nadzorcy na jedną iterację (`return (uint32_t)-1` →
   `kernel_user_pause_recover`); dzięki temu dwa klienty wymieniające
   `pause_tick()` nie ping-pongują już poniżej schedulera (wcześniej
   kompozytor nie widział pośrednich pozycji skryptowanego przeciągania).

2. **Nieosiągalny `WM_PART_FRAME`.** Okno aplikacji rejestruje
   `client.w == frame.w`, `client.h == frame.h-16`, więc ramka miała zerową
   grubość i `wm_hit_part()` nigdy nie zwracało `WM_PART_FRAME` — resize był
   niemożliwy. `wm_hit_part()` rezerwuje teraz zewnętrzne 3 px okna na ramkę
   (styl Win98 / XaAES `XAW_BORDER`), a gadżety paska tytułu mają priorytet.

3. **Hit-test w pozycji wciśnięcia.** Kompozytor próbkuje mysz raz na klatkę;
   skryptowany burst wstrzyknięć trafiał w tytuł tylko przypadkiem, a szybkie
   kliknięcie mogło chybić. `kernel_sessions` pamięta teraz pozycję zbocza
   wciśnięcia (`mouse_press_x/y`, `ksessions_mouse_press_*`), a kompozytor
   używa jej do `wm_hit`/`wm_hit_part`/`drag_dx`/`rsz_gx`; przy realnej
   myszy zapisuje ją `handle_mouse`.

Dodatkowo aplikacje nie gubią wejścia: `call_wait()` w `desktop_wm.c`
kolejkuje komunikaty, których nadawcą nie jest usługa (`pending[]`), zamiast je
odrzucać. Wcześniej każdy klawisz/klik trafiony w trakcie malowania przepadał,
a odpowiedzi brokera zostawały w skrzynce, aż `SYSTEM.GFX` zaczynał zwracać
`-0x3c2` i pulpit zamarzał w połowie przeciągania. Ten sam wzorzec miały już
`commander.c`, `sysinfo.c` i `setup_app.c`.

Tryb testów skryptowych: opkod 25 włącza `mouse_scripted`, który (a) ignoruje
realne pakiety IKBD z Hatari i (b) nie kolejkuje KMOU do aplikacji; opkod 26
kończy tryb. Bez tego host wysyłał pakiety z `buttons=0`, które kasowały
wstrzyknięty stan.

**Weryfikacja** (`atos2-tools/probe_win.py`): po F1 `wm_dbg_press=1`,
`wm_dbg_phit=1` (trafienie w tytuł w 500,50) i `wm_dbg_drag_start=1`;
`wm_move` zatwierdza przesunięcie okna (y 44 → 59). Po F2 `rsz_start=1` w
(598,226) i startuje skalowanie krawędzią. **Referencja:** XaAES
(`xaaes/src.km/widgets.c`) używa dokładnie tego modelu — osobny widget
`XAW_BORDER` do skalowania ramką, `XAW_RESIZE` (SIZER) w prawym dolnym rogu,
`XAW_MOVER` oraz metody click/drag/release; klasyczny AES rysuje przerywany
zarys (`graf_dragbox`) i dopiero potem zmienia geometrię.

### 5.32 Wejście myszy, koalescencja i optymalizacja kompozycji

**Zamrożenie myszy i „wieczny" zarys.** `ksessions_pump()` wołał
`handle_mouse()` tylko `if(!forward_ready&&...)`. Gdy aplikacja nie mogła
odebrać zaległego pakietu (pełna skrzynka), flaga `forward_ready` zostawała
ustawiona i **każdy kolejny pakiet IKBD był pomijany** — także zwolnienie
przycisku. Skutek: kursor stawał, `drag_active` nigdy się nie kończył i
przerywany zarys rysował się w kółko, a okno nie dawało się przesunąć.
Teraz pakiety zawsze aktualizują stan sesji; `forward_ready` tylko odracza
dostarczenie do aplikacji.

**Zamienione przyciski IKBD.** Bajt statusu myszy IKBD ma `bit0 = PRAWY`,
`bit1 = LEWY`, a cały ATOS (kompozytor, aplikacje) i hook injection używają
`bit0 = LEWY`. Bez zamiany **lewy przycisk był ignorowany**, a prawy działał
jak lewy — stąd „klik w FILE nic nie robi”. `kernel_raw_mouse_event()`
zamienia teraz bity 0 i 1.

**Pulpit przestaje się kończyć.** `DESKTOP.APP` bywał kończony po starcie
aplikacji (status `-258`, `kdbg_end_name` = `"DESK"`), przez co znikał
właściciel klawiatury (`kdbg_kbpid=0`) i **nic nie reagowało na mysz ani
klawisze**. Pętla pulpitu traktuje teraz błąd `receive` jako chwilowy
(ponawia), a wychodzi wyłącznie przez CLOSE/Escape.

**Koalescencja KMOU.** Ścieżka wejścia dopisywała jeden komunikat na każdy
pakiet myszy, więc ruch kursora zapełniał kliencką połowę skrzynki i klawisze
były odrzucane (backpressure) — F-klawisze „ginęły". Teraz nowa pozycja
zastępuje oczekujący KMOU na końcu kolejki (znaczenie ma tylko ostatnia
pozycja, jak w GEM/AES).

**Koniec pełnego recompose co tick.** Kompozytor wymuszał
`kernel_wm_damage_all()` w każdej klatce w trybie pulpitu. To (a) kasowało
XOR-owy kursor na czas recompose przy 50 Hz — widoczne mruganie wskaznika —
oraz (b) przerysowywało niepotrzebnie całą klatkę. Teraz komponowane są
wyłącznie realne uszkodzenia: zarys przeciągania to 4 cienkie krawędzie
(`outline_damage`), dekoracje okna wracają tylko gdy jego ramka była
uszkodzona (`win_dirty[]`), a tło pulpitu maluje się raz. Geometria okien
żyje w `wm.win[].frame`, więc wymiary są pamiętane, a nie liczone od nowa.

**Indeks okna po `wm_raise`.** Gałąź resize brała `rsz_win=hit` **po**
`wm_raise()`, które przenosi okno na wierzch i przesuwa listę; teraz używa
`wm.count-1`, czyli faktycznego slotu podniesionego okna.

### 5.33 Dlaczego przesuwanie okna było wolne (i jak to robią referencje)

Porównanie modeli:
- **GEM AES / EmuTOS** — nie ma menedżera okien: gumkę rysuje aplikacja przez
  VDI (`vro_cpyfm`, `graf_dragbox`); appka nie przerysowuje ekranu na każdy
  pakiet myszy.
- **XaAES** — ruch okna wykonuje **serwer** (widżety `XAW_TITLE`/`XAW_BORDER`,
  metody click/drag/release); aplikacja nie maluje ekranu przy każdym ruchu.
- **MagiC** — ruch „na żywo" szybkim blitem zawartości okna.

U nas były **dwie** ścieżki i obie drogie:
1. **Aplikacja pulpitu** przesuwała swoje okno przez `paint()`, czyli pełny
   batch 640×400 na **każdy** pakiet myszy. Zastąpione przez `paint_rect()`,
   które maluje wyłącznie unię starego i nowego prostokąta okna.
2. **`gui_damage()` raportował pełny ekran dla każdego `GUI_WINDOW`**
   (`x=y=0;r=width;b=height`). Przez to nawet `paint_rect()` dawał damage
   640×400 — pomiar pokazał ~288 000 px/klatkę, czyli cały ekran. Teraz
   `GUI_WINDOW` zawęża się do własnego prostokąta (+4 px cienia), więc ruch
   okna uszkadza tylko region, a kompozytor przenosi resztę przez blit.

Efekt: ruch okna WM idzie przez kompozytor (blit z powierzchni okna, na MSTE
dodatkowo Blitter), a okno pulpitu przez ograniczony repaint zamiast pełnego
ekranu.

### 5.34 Okna pulpitu w WM — skalowanie do N okien

Okno pulpitu (`ATOS DESKTOP`) jest teraz **prawdziwym oknem WM** (uchwyt 1),
a nie rysunkiem w kanwie. Kanwa (uchwyt 0) trzyma tło + górne menu; okno ma
własną powierzchnię i jest przesuwane/skalowane przez kompozytor
(`wm_move`/`wm_resize` + blit z powierzchni). Aplikacja **nie maluje nic** przy
ruchu myszy — zgodnie z modelem GEM/XaAES, gdzie ramkę okna posiada serwer.

**Zarys ruchu jako XOR (bez recompose).** Rysowanie zarysu przez damage i
recompose kosztowało ~4 ms na zarys + ~10,6 ms recompose + ~5,9 ms ozdób na
każdy krok. EmuTOS robi to inaczej: `graf_dragbox` przełącza **XOR-owy**
prostokąt (`gr_wait`), więc wymazanie jest darmowe i nic pod spodem się nie
zmienia. Nasz `drag_xor()` robi to samo: XOR 1-px prostokąta w żywy framebuffer,
zero damage, zero recompose. Pomiar autodrag (200 kroków): **24,2 s → 3,58 s
(6,7×)**, liczba kompozycji **168 → 2**. Zawartość okna wstawiana jest
`wm_move` na puszczenie (wersja C).

**Scalanie damage prostokątów — główne wąskie gardło.** `wm_damage_add()`
łączył przecinające się prostokąty w **bounding box**. Cztery 1-px krawędzie
zarysu stykają się w rogach, więc scalenie dawało **cały prostokąt okna**
(~65 000 px/klatkę zamiast ~1 000) i dusiło kompozytor (pomiar: ~13 klatek/s).
Scalanie jest teraz warunkowe — tylko gdy bounding box nie marnuje dużo
powierzchni (`c <= a+b+(a+b)/2`). Po zmianie pomiar dał **~1 100 px/klatkę**,
czyli 60× mniej pracy na krok zarysu.

**Ruch „na żywo" z zawartością** (`wm_move_live`, w `wm_core.c`): geometria
jest aktualizowana w każdej klatce, a uszkadzana jest tylko **symetryczna
różnica** starego i nowego prostokąta (do 4 cienkich pasków na stronę). Dzięki
temu zawartość okna jedzie za kursorem, a kompozytor kopiuje ją z powierzchni
okna — nie ma repaintu aplikacji ani pełnego przerysowania okna. Skalowanie
(`rsz_active`) nadal używa przerywanego zarysu i zatwierdza rozmiar na
puszczenie.

**ABI:** uchwyt okna jedzie w polu `color` opkodów 18 (batch) i 20 (geometria);
`0` = okno główne/kanwa (zgodność wstecz — istniejące aplikacje nic nie zmieniają).
Po zatwierdzeniu ruchu/resize kompozytor wysyła do właściciela **WMOV**
(`0x574d4f56`: `value1=(x<<16)|y`, `value2=(w<<16)|h`) — odpowiednik
`WM_MOVED` z XaAES, żeby klient znał nową geometrię.

**Limity i koszty przy N oknach:**

| N okien | Powierzchnie | RAM (32 KB/okno) | Koszt ruchu jednego okna | Z-order |
|--------:|-------------:|-----------------:|--------------------------|---------|
| 1 | 2 (kanwa+okno) | 64 KB | blit prostokąta okna | bez zmian |
| 3 | 4 | 128 KB | **taki sam** | bez zmian |
| 5 | 6 | 192 KB | **taki sam** | bez zmian |
| 8 | 8 | 256 KB | **taki sam** | `WM_MAX=8` |

Stałe: `WM_MAX=8`, `WM_APP_WINDOWS=6` (+konsola+kanwa=8),
`WM_CLIP_MAX=16`, `WM_DAMAGE_MAX=24`, `WM_SURFACE_MAX=8`. Ruch nie zależy od
liczby okien — blitowany jest tylko prostokąt ruchomego okna z jego powierzchni
(na MSTE z Blitterem). Koszt rośnie dopiero przy kompozycji nakładających się
uszkodzeń. Jedyny realny koszt skalowania to RAM: 32 KB na pełnoekranową
powierzchnię okna; na 1 MB maszynach następnym krokiem byłyby powierzchnie
rozmiaru okna (w×h bitów), co daje ~4× mniej.

## 6. Nastepne kroki ETAPU 2

1. **API wsadowe** dla kompozytora/konsoli + pomiar „żądania na klatkę" —
   **zrobione** (§5.15): wsad opkod 18 niesie całe odświeżenie pulpitu, licznik
   pokazuje 0,15 żądania/klatkę.
2. **WM domyślnie zapalony (`WM=1`)** — zrobione. Desktop (`DESKTOP.APP` na
   batchu) komponuje się przez `kernel_wm`; profil tekstowy jest bez zmian
   (`kernel_wm_frame_tick` pozostaje zbramkowany na tryb desktop). Przyrostowy
   flush z §5.1 i okna WM są teraz żywe. Na pulpicie jest też **używalne okno
   konsoli** (§5.6).
3. Pomiar w kernelu dla `mono_copy_words` i `emutos_vdi_vertical`.
4. Blitterowa kopia takze dla trybow 2/4bpp (dzis tylko mono) po rozszerzeniu
   self-testu.

### 5.35 Dlaczego DESKTOP.APP umieral: libgcc bez -mpcrel (BUG KRYTYCZNY)

Objaw: po kazdym starcie DESKTOP.APP (pid 4) konczyl sie statusem -258
(bus error) na 68030 / -259 (address error) na 68000, wm.count spadalo do 1
i zostawalo tylko okno konsoli. Trace Hatari na 68000 pokazywal, ze proces
wykonywal sie pod adresem 0x000011BE i czytal spod 0x7A33B.

Przyczyna: aplikacje ATOS linkuja sie na adres 0 (linker/app-sys.ld), a loader
umieszcza je pod adresem z heapu (np. 0x76030). Kod aplikacji jest pozycyjnie
niezalezny dzieki -mpcrel (GUI_APP_CFLAGS), ALE libgcc dolaczany przez -lgcc
nie jest budowany z -mpcrel. Jego pomocnicze funkcje wywoluja sie wzajemnie
BEZWZGLEDNYM jsr:

    __divsi3:   jsr 0x00000DD0      ; powinno byc base+0xDD0
    __umodsi3:  jsr 0x00000DD0 / 0x00000EBC
    __modsi3:   jsr 0x00000E2C / 0x00000EBC

Pokil aplikacja nie uzywala dzielenia/modulo, blad byl utajony. Nowy pasek
przewijania rysuje wiersze ROW nn, czyli q/10 i q%10 - pierwsze wywolanie
q/10 wchodzi przez PC-relative lea+jsr do __divsi3, a to skacze juz do
absolutnego 0xDD0, czyli do smieci w niskim RAM-ie.

Naprawa: system/app_math.c dostarcza POZYCYJNIE NIEZALEZNE __udivsi3,
__umodsi3, __mulsi3, __divsi3, __modsi3 (shift-and-subtract, bez / i %, bez
64 bitow, zeby nie sciagnac libgcc). Obiekt build/system/app_math.o (budowany
z GUI_APP_CFLAGS) jest dolaczany PRZED -lgcc do 16 aplikacji ladowanych z
obrazu, wiec linker nigdy nie siega po wersje z libgcc. Regula-prerekwizyt dla
*.APP wymusza relink po zmianie tego obiektu.

Po naprawie: kdbg_uf_count=0, kdbg_rel_end=0, wm.count=2, win1 flags=0x1,
content_h=400, tytul 'ATOS DESKTOP', okno zyje po 26 s i po pelnym autodragu.

### 5.36 Pasek przewijania w kliencie i wysokosc dokumentu (opcode 29)

Pasek ma byc WEWNATRZ klienta i pojawiac sie TYLKO gdy tresc nie miesci sie w
oknie:

* kolumna paska to client.x + client.w - WM_SB_W (WM_SB_W=16), rysowana tylko
  gdy sb_visible(i), czyli content_h > client.h;
* klient zglasza wysokosc dokumentu opcodem 29, ZANIM powstanie powierzchnia
  okna (pierwszy GFX_BATCH). Poprzednia wersja kernel_wm_set_content()
  wymagala istniejacej powierzchni i po cichu gubila wysokosc - pasek nigdy
  sie nie pokazywal. Teraz wartosc trafia do wm_window_req[]
  (content_h/content_set) i jest przenoszona do wm_window_t w
  kernel_wm_app_surface();
* opkod 29 jest na liscie operacji, ktore NIE uniewazniaja cache'u konsoli
  (nie dotyka powierzchni konsoli).

Zmierzony stan: win1 client=(96,64,320,184) content_h=400, wiec pasek jest
widoczny w oknie (zrzut /tmp/atos-desktop.png), razem z gadzetami zamknij /
zwijaj i sizerem w prawym dolnym narozniku.

### 5.37 Dekoracje przycinane do uszkodzenia (glowna optymalizacja ruchu)

Pomiar wyjsciowy: pass dekoracji rysowal CALY chrom (pasek tytulu, ramke,
gadzety, tor paska przewijania) przy kazdej klatce, niezaleznie od tego, jak
maly fragment ekranu sie zmienil - 18,2 ms na klatke. To jest koszt stalego
rozmiaru okna, nie ruchu.

Naprawa: `deco_dmg[]`/`deco_nd` to zrzut listy uszkodzen zrobiony PRZED
`wm_recompose()` (ktory czysci liste). Kazda primitywa chromu idzie przez
`deco_fill()` / `deco_bevel()` / `deco_hit()`, ktore przecinaja ja z ta lista.
Koszt dekoracji jest teraz proporcjonalny do odmiecionej powierzchni, a nie do
rozmiaru okna - dokladnie tak, jak GEM robi `w_move()` + `w_update()`
(`aes/gemwmlib.c`: najpierw blit okna, potem odrysowanie tylko odslonietych
paskow).

| Metryka (autodrag 200 krokow, 68030) | przed | po |
|--------------------------------------|------:|----:|
| Czas calego autodragu | 19,1 s | **9,8 s** |
| Koszt kompozycji | 36,7 ms | **25,6 ms** |
| - recompose | 12,8 ms | 12,7 ms |
| - **dekoracje** | 18,2 ms | **7,6 ms** |
| - kursor | 1,3 ms | 1,2 ms |

### 5.38 Czego NIE robic: blit ekranowy okna przed kompozytorem

Zgodnie z modelem GEM sprawdzilem tez wariant `w_move`: skopiowac prostokat
okna na ekranie w nowe miejsce (`row_copy_bits` z buforem wiersza, zeby
zachodzenie na siebie bylo bezpieczne), odjac nowa ramke od listy uszkodzen i
zostawic kompozytorowi tylko naprawe opuszczonych paskow. Chrom nie jest wtedy
rysowany wcale (`kdbg_deco_cycles` spadlo z 7,6 ms do **0,19 ms**).

**Wynik: 22,9 s - ponad 2x GORZEJ.** Powod: ekran w ATOS to VOFA/Shifter,
wiec kazdy zmieniony piksel musi trafic do video przez `kvideo_damage()` +
`kvideo_flush()`. Przesuniecie okna zmienia CALY jego prostokat (rowniez czesc
wspolna starego i nowego polozenia), wiec flush obejmuje ~2x 320x200 px na
krok i to on zjada zysk z nieskomponowanego chromu. Wniosek: w architekturze
z kompozytorem i odroczonym flushem oplaca sie przemalowac tylko odmiecione
paski (stan obecny), a nie przenosic piksele okna. Wariant zostal wycofany.

Waskim gardlem pozostaje `wm_recompose` (12,7 ms na klatke przy ~2,8 tys.
zmienionych pikselach, czyli ~55 cykli/piksel) - to glownie bitowy zapis mono
do framebuffera. Nastepne kroki: szybszy span-copy (blitter dla szerokich
paskow), scalanie paskow uszkodzen przed kompozycja oraz cache klienta
okna (brak blitu gdy tylko okno sie przesuwa, a tresc jest bez zmian).

Diagnostyka dodana przy okazji pomiaru: `kblit_calls`, `kblit_hw`, `kblit_cpu`
oraz prog `WM_BLIT_MIN_PIXELS` (kiedy uzyc Blittera, a kiedy kopii CPU).

### 5.39 Dlaczego ruch z trescia sie "rozjezdzal" i model docelowy

Obiaw zgloszony przez uzytkownika: zawartosc okna rozjezdzala sie przy
przenoszeniu. Przyczyna byla w modelu ruchu, nie w rysowaniu: `wm_move_live()`
uszkadzal tylko SYMETRYCZNA ROZNICE starej i nowej ramki (dwa cienkie paski).
Kompozytor przemalowuje wylacznie uszkodzone prostokaty, wiec CZESC WSPOLNA
starego i nowego polozenia zostawala z nieaktualnymi pikselami - okno
wygladalo, jakby sie rozmazywalo, a po puszczeniu (`wm_move` + pelne
uszkodzenie) wracalo na miejsce. W kompozytorze nie da sie tego naprawic
tanio: przesuniecie zmienia CALY prostokat okna, nie tylko paski.

Model docelowy = to samo, co robi pulpit EmuTOS-a przy przeciaganiu okna:
`graf_dragbox` (XOR-owany kontur) w trakcie, a `wind_set(WF_CXYWH)`/`wm_move`
dopiero na puszczeniu. Kontur jest rysowany i zmazywany przez **XOR prosto do
framebuffera**, wiec nie uruchamia ani kompozytora, ani passu dekoracji:

* `xor_rect()` / `xor_outline()` - ten sam wzor kreski co przy resize, ale XOR,
* `outline_video_damage()` - do karty trafia tylko 1px obramowanie konturu,
* kursor chowany na czas XOR i odtwarzany (`cursor_sync`) przed flush,
* na puszczeniu: XOR-zmazanie konturu, `wm_move()`, `notify_geometry()`,
  `kernel_wm_damage_all()` - okno pojawia sie w nowym miejscu z pelnym chromem.

| autodrag 200 krokow (68030) | czas | uwagi |
|-----------------------------|-----:|-------|
| ruch z trescia (paski sym. roznicy) | 9,8 s | **rozmazywal zawartosc** |
| kontur rysowany w kompozytorze | 20,7 s | poprawny, ale wolny |
| **kontur XOR (`graf_dragbox`)** | **4,8 s** | poprawny, 2 kompozycje na caly drag |

Podczas calego 200-krokowego przeciagania wypadly tylko **2 kompozycje**
(`kdbg_wm_composes`) - cala reszta to XOR + flush samego obramowania. Zrzuty:
`docs/screenshots/desktop-drag-outline.png` (w trakcie) i
`docs/screenshots/desktop-after-drag.png` (po puszczeniu).

### 5.40 Co zostaje

`wm_move_live()` zostaje w `wm_core.c` (uzywany przez testy hosta), ale sciezka
interaktywna go nie uzywa: w kompozytorze z odroczonym flushem poprawny ruch
"z zawartoscia" wymaga albo pelnego uszkodzenia ramki (drogie), albo blitu
ekranowego z flushem calego okna (zmierzone 22,9 s, patrz 5.38). Kontur XOR
jest zarowno poprawny, jak i ~4x szybszy od poprzedniej wersji.

### 5.41 Ikony dyskow: przegladanie zawartosci (pierwszy wycinek)

Klik na **DISK C** / **DISK D** otwiera okno plikow (WM handle 2) z listingiem
katalogu. Wykorzystany jest istniejacy ABI: `ATOS_CALL_DIR_PAGE = 52`
(`d1=skip, d2=count, a0=path, a1=entries`) - zero nowych wywolan w kernelu.

* Okno ma wlasny pasek tytulu, gadzet zamknij i sizer; jego zawartosc jest
  client-relative, wiec przesuniecie okna nie wymaga przerysowania
  (`WMOV` bez handle rozpoznajemy po rozmiarze - okna maja rozne wymiary).
* `content_h == client.h` -> pasek przewijania jest ukryty; listing jest
  **stronicowany przez aplikacje** (`MORE >`). To celowe: kompozytor wlada
  `src_y`, wiec aplikacja nie wie, ktory wiersz jest pod kursorem - przy
  wlasnych stronach kazdy wiersz ma staly offset i klik dziala.
* Wiersz `..` wchodzi do katalogu nadrzednego, klik na `<DIR>` wchodzi glebiej,
  klik na plik zaznacza go (rewers).
* Kernel wysyla `WCLS` (0x57434c53, value1 = handle) gdy kompozytor zamknie okno
  klienta (`window_close`), wiec aplikacja kasuje stan zamiast rysowac do okna,
  ktore juz nie istnieje; ponowne otwarcie dziala, bo `fb_open_at` zawsze
  deklaruje geometrie (`gfx(20,...)`) przed pierwszym rysowaniem.
* Trash: na razie tylko komunikat w statusie (kernel ma `ATOS_CALL_TRASH`,
  ale nie ma listingu kosza).

Haczyki testowe (klawiatura): **F7** = otworz okno plikow (jak DISK C),
**F8** = wejdz w pierwszy wiersz. Przy okazji poprawiony skan: `GFX_CYCLE`
wisial na 0x41 (F7) i byl martwy - przeniesiony na 0x43 (F9).

Zrzut: `docs/screenshots/desktop-file-window.png` (\SYSTEM: BOOT.SYS 786,
KERNEL.SYS, ACSI.SYS 564, ... SHELL.SYS 11477 - zgodne z listingiem instalatora).

### 5.42 Chrom okna a jego WIDOCZNY obszar (naprawa nakladania sie okien)

Objaw zgloszony przez uzytkownika: przy dwoch-trzech oknach "rozwalalo okna i
zawartosc". Przyczyna: pass dekoracji rysowal chrom KAZDEGO okna przyciety
wylacznie do listy uszkodzen. Kompozytor najpierw blituje klienty wszystkich
okien (od dolu do gory), a dekoracje rysuje po nich, wiec chrom DOLNEGO okna
(pasek tytulu, ramka, gadzety) malowal sie po kliencie okna GORNEgo - dokladnie
tam, gdzie sie nakladaja.

Naprawa: `deco_begin(w)` liczy dla okna `w` wlasny obszar widoczny:
`(uszkodzenie INTERSECT ramka okna)` minus ramki wszystkich okien wyzej
(`deco_sub`, ta sama dekompozycja na paski co `wm_core`). Wszystkie primitywy
id:a przez `deco_vis[]`:

* `deco_fill()` tnie wypelnienia i fazowania do `deco_vis` (bez zmian wygladu),
* `deco_hit()` odsiewa gadzety/sizer, ktore w ogole nie sa widoczne,
* `deco_all()` wymaga pelnej widocznosci dla rzeczy, ktorych nie da sie przyciac
  w polowie: tekst tytulu rysowany jest **znak po znaku** (komorka 8x8),
  strzalki paska przewijania i przekatne X.

Uwaga: nie mozna tu uzyc `wm.clip[]` z kompozytora - ono startuje od KLIENTA
(bo blituje piksele klienta), a pasek tytulu, ramka i gadzety leza POZA
klientem. Pierwsza wersja poprawki uzywala `wm.clip[]` i przez to tytuly okien
w ogole nie byly rysowane - dlatego obszar widoczny chromu liczy sie od ramki.

Weryfikacja (zrzut `docs/screenshots/desktop-two-windows.png`): okno plikow
`DISK C:` (120,56,380,210) pod aktywnym `ATOS DESKTOP` (273,137,320,200);
tytuly, gadzety i paski obu okien poprawne, zaden chrom nie przebija okna wyzej,
`kdbg_uf_count=0`, `kdbg_rel_end=0`.

### 5.43 KRYTYCZNE: bledny algorytm w moich pomocnikach dzielenia

Naprawiajac libgcc bez `-mpcrel` (5.35) napisalem `__udivsi3`/`__umodsi3`
z bledem: skalowalem w dol tylko dzielnik, a bit ilorazu szedl osobno. Wynik:
`7218 % 10` zwracalo 7200 zamiast 8. Objawialo sie jako **smiśnie wygladajace
cyfry** w interfejsie (rozmiary plikow `0000á`, a w oknie pulpitu `ROW 10`
zamiast `ROW 01`) - czyli nie font, nie JPEG, tylko zle dane.

Poprawny algorytm (restoring division) skaluje dzielnik i bit ilorazu RAZEM:

    bit = 1;
    while (!(den & 0x80000000u) && (den << 1) <= num) { den <<= 1; bit <<= 1; }
    while (bit) { if (num >= den) { num -= den; quot |= bit; } den >>= 1; bit >>= 1; }

Sprawdzone wyczerpujaco na hoscie (iloczyn kartezjanski 11 wartosci, w tym
0xFFFFFFFF) wzgledem natywnych `/` i `%` - `all ok`. Po naprawie rozmiary w
oknie plikow sa poprawne (`AUTOEXEC.PRG 382`, `BIOS.CFG 128`, `EVIDESK.INF 35`).
Uwaga na przyszlosc: `number()` z `gui_app.h` nie uzywa dzielenia (odejmowanie
na tablicy `places[]`), dlatego bylo odporne na ten blad.

### 5.44 Wyglad okna plikow: sortowanie, kolumny, font 8x16

* `kgfx_text()` rysuje glify **8x16**, a nie 8x8 - przy odstepie wierszy 14 px
  teksty nachodzily na siebie i naglowek czytal sie jak `NAMF`/`CT7F`.
  Wiersze maja teraz 18 px, a uklad jest policzony pod 16-px glify.
* Pasek sciezki: **wciete** pole (`GUI_BEVEL` z `reserved=1`), tekst 8 px od
  krawedzi - wyglada jak pole, a nie jak zabladzony bialy pasek.
* Pod belka tytulu jest pasek sortowania: `[NAME] [DATE] [TYPE]`. Aktywny
  klawisz jest wciśniety i w negatywie. Sortowana jest CALA zawartosc katalogu
  (do 64 wpisow, w dwoch wywolaniach po 32), nie tylko widoczna strona;
  katalogi zawsze na poczatku, dalej wg nazwy / daty (najnowsze pierwsze) /
  rozszerzenia, a przy rownych kluczach wg nazwy.
* Kolumny staly sie **wyrownane**: nazwa 0..11, znacznik `<DIR>` 13..17,
  rozmiar prawostronnie 20..29, data 32..47. `gui_draw_t.text` ma tylko 32
  bajty, wiec data jest osobnym elementem tekstowym.
* Data z FAT (`datetime`) pokazywana jako `YYYY-MM-DD HH:MM`.
* Menu pulpitu: bialy pasek 16 px z czarna krawedzia na dole (`y=16`) i tekstem
  wypelniajacym pasek - wczesniej pod FILE/OPTIONS zostawal bialy pasek.
* `list[]` zwiekszone do 56 elementow (kazdy wiersz to 2-3 primitywy).

Zrzut: `docs/screenshots/desktop-file-window.png`.

### 5.45 Kolor okna: aktywne czarne, nieaktywne szare

Na ekranie 1bpp "szary" to 50% szachownica (jak EmuTOS `IP_4PATT`).
`deco_dither_rect()` maluje ja przycieta do widocznego obszaru chromu
(`deco_dither`), a `deco_fill`/`deco_bevel` ida jak dotad. Teraz:

* **aktywne**: pasek tytulu **czarny**, tekst bialy (negatyw),
* **nieaktywne**: pasek **szary (dither)**, tekst czarny na bialych polach
  glifow - czytelny na szarym tle.

Wczesniej aktywne bylo biale, a nieaktywne czarne, co czytalo sie odwrotnie.

### 5.46 Dlaczego nie dalo sie kliknac w ikone (i jak to sprawdzilem)

Dwie realne przyczyny gubionych klikniec:

1. **Koalescencja KMOU nadpisywala klik.** Gdy w skrzynce lezal juz KMOU,
   nowy pakiet zastępował go w miejscu - jesli klient byl zajety, nacisniecie
   zostalo zamienione na nowsza pozycje (albo na samo zwolnienie). Teraz
   scalane jest **tylko czyste przesuniecie**: `KMOU` o zmienionym stanie
   przyciskow dostaje wlasny wpis, wiec klik nigdy nie zniknie.
2. **Przestaraly `drag_active` polykal wszystko.** Forwarding pomijal KMOU
   gdy kompozytor uznawal, ze trwa przeciaganie; przy zgubionym zwolnieniu
   flaga zostawala wlaczona i **zadne** pozniejsze klikniecie nie docieralo do
   aplikacji. Teraz pomijanie dziala tylko, gdy przycisk jest NAPRAWDE
   wcisniety (`kernel_wm_dragging()&&(buttons&1u)`).

Weryfikacja end-to-end (probe): hook **F9** ustawia wskaznik nad ikona
DISK C (przez `ksessions_mouse_inject`, bez przyciskow), a Host wysyla
**prawdziwe** zdarzenie IKBD (`hatari-event doubleclick`):

* okno plikow sie otwiera (`wm.count=3`, tytul `DISK C:`),
* hook **F8** ustawia wskaznik nad przyciskiem `DATE`, kolejny prawdziwy
  klik **sortuje** i podswietla DATE - czyli sciezka IKBD->KMOU->aplikacja
  dziala w calosci.

Test `test_gui_input_backpressure` zaktualizowany do nowej reguly: te same
przyciski = scalanie w miejscu, zmiana przyciskow = nowy wpis.

### 5.47 Menu OPTIONS (styl EmuTOS) i przelaczanie rozdzielczosci

Menu w pasku ma teraz dwa rozwijane zestawy jak w EmuDesk: **FILE**
(OPEN TERMINAL / RUN... / CLOSE) i **OPTIONS** (SCREEN 640x400 / SET 800x600 /
SET 1024x768). Rozwijane listy maja wlasna geometrie (`OPT_X/OPT_W`) i obsluge
myszy oraz strzalek, a `MENU_ROW` to 18 px (font 8x16). F10 otwiera OPTIONS.

Dodany opkod **GFX 30** na tryb video:

* `a=0` -> zapytanie, zwraca `(width<<16)|height` - OPTIONS pokazuje aktualny
  tryb bez zgadywania,
* `a=(w<<16)|h` -> zadanie zmiany; kernel przyjmuje **tylko** tryb, ktory
  kompozytor i powierzchnie okien potrafia obsluzyc, inaczej zwraca blad,
  a aplikacja pokazuje `N/A: NEEDS ET4000/VDI`.

**Dlaczego 800x600 i 1024x768 nie dzialaja na tej maszynie:** emulowany STE ma
Shifter z dokladnie trzema trybami (320x200 low, 640x200 medium, 640x400 mono
przy `--monitor mono`). Te dwie rozdzielczosci wymagaja albo

1. karty **ET4000/Nova** - kernel ma gotowy backend (`video/et4000.c`, domyslnie
   1024x768x8) i sciezke VOFA, ale **Hatari tej karty nie emuluje**; albo
2. trybu **`--vdi`** w Hatari (`--vdi --vdi-width 1024 --vdi-height 768
   --vdi-planes 1`) - wtedy jednak ATOS-owy BIOS/kernel musi przejac geometrie
   z VDI zamiast programowac sztywny tryb Shiftera, a kompozytor, powierzchnie
   okien (dzis 640x400 = 32000 B) i uklad pulpitu musza isc za rozmiarem ekranu.

Dlatego najpierw menu + zapytanie + uczciwa odmowa (opkod 30), a dopiero potem
- jesli chcesz - adoptowanie trybu VDI i przebudowa kompozytora na dowolny
rozmiar. Zrzut: `docs/screenshots/options-menu.png`. (Na docelowej karcie ET4000
te tryby sa juz podlaczone i dzialaja na zywo - patrz 5.49.)

### 5.48 Docelowa maszyna: Mega ST + TF536 + 64 MB + ET4000 (zweryfikowane)

`atari-030-port/run-bios-vga.sh` odpala **ten sam** `build/bios_mste.bin` i
`work/atos-v2.img` na docelowej konfiguracji:

```
--machine megast --cpulevel 3 --cpuclock 32 --memsize 4 --ttram 64 \
--tf536 on --graphics-card vofa --gfx-chip et4000 --mono --ide-master atos-v2.img
```

(patchowany `atari-030-port/hatari-tf536` obsluguje `--tf536`, `--graphics-card
`vofa`, `--gfx-chip et4000`). Sprawdzone: pulpit ATOS wstaje tam poprawnie -
status `32MHz/030 (CED)/- 4/64MB MegaST, TOS 2.06, MONO 71 Hz`, okno z paskiem
przewijania, `ROW 01..ROW 04`. Zrzut:
`docs/screenshots/target-megast-tf536-et4000.png`.

**800x600 i 1024x768 na tej maszynie sa realne** (karta je ma, Hatari je
emuluje, 64 MB TT-RAM starcza: mono 1024x768 = 96 KB, 8bpp = 768 KB), ale
dzis ich nie ma, bo:

1. `system/vofa_et4000.c` programuje **jeden sztywny zestaw rejestrow**
   (CRTC/SEQ/ATT/GDC) - czyli jeden tryb (640x480, widok 640x400). Potrzebna
   tablica trybow dla 800x600 i 1024x768.
2. `kvideo_boot()`/`kvideo_activate_vofa()` ustawiaja powierzchnie na 640x400 -
   musza przyjac `width/height` z wybranego trybu.
3. Kompozytor jest sztywno 640x400: `kernel_wm_init()` -> `wm_init(&wm,640,400)`,
   powierzchnie okien 32000 B (=640x400), geometria okna konsoli (640x304),
   `SCRW/SCRH` pulpitu. Wszystko musi liczyc sie z rozmiaru ekranu.
4. Opkod **GFX 30** (zapytanie + zadanie zmiany) juz jest - trzeba go podlaczyc
   do programowania CRTC i re-inicjalizacji kompozytora.

Ferryt: calosc jest testowalna w Hatari bez okna (`SDL_VIDEODRIVER=dummy
./run-bios-vga.sh --cmd-fifo /tmp/vofa.fifo` + `hatari-debug screenshot`).

### 5.49 Tryby 800x600 / 1024x768 dzialaja na zywo (ETAP 2, krok 3+2+4)

Punkty 1-4 z 5.48 sa zrobione. Przelaczanie rozdzielczosci z OPTIONS dziala
**na zywo**, bez restartu, na docelowej konfiguracji (Mega ST + TF536 + ET4000).

**Tablice trybow** (`system/vofa_et4000.c`). Doszly `crtc_800` i `crtc_1024` obok
referencyjnego `crtc` (640x480, nadal zgodnego z EmuTOS/Nova - pilnuje tego
`test_vofa_reference`):

| tryb | CRTC 0x13 (offset) | CRTC 0x12 | CRTC 0x07 | stride | bajty |
|------|--------------------|-----------|-----------|--------|-------|
| 640x480 (widok 640x400) | 0x28 | 0xDF | 0x3E | 80 | 38400 |
| 800x600 | 0x32 | 0x57 | 0xF0 | 100 | 60000 |
| 1024x768 | 0x40 | 0xFF | 0xFD | 128 | 98304 |

Wartosci CRTC policzone formulami VESA (800x600@60: 1024x628, 40 MHz;
1024x768@60: 1344x806, 65 MHz) i sprawdzone w `tests/test_vofa.c` na atrapie
magistrali. Hatari wypisuje je w logu jako
`VOFA-MODE: w=800 h=600 stride=100 bpp=1 crtc13=32` (i `w=1024 h=768 stride=128
crtc13=40`), wiec tryb zmienia sie naprawde na karcie.

**Kompozytor niezalezny od rozdzielczosci**:
`kvideo_set_mode(w,h)` liczy `pitch=(w+7)/8` i (re)alokuje cien karty oraz
mape uszkodzen per wiersz; `kvideo_damage`/`kvideo_flush` ida za trybem.
`kernel_wm_init/enable` bierze geometrie z `kgfx_get_info()`, powierzchnie
aplikacji i konsoli sa alokowane raz na **maksymalny** tryb (98304 B), a
`kernel_wm_reconfigure()` po zmianie trybu ustawia nowy pitch, rozciaga
plutno-pelnoekranowe (flag 7) i konsoli oraz zleca pelny repaint.
`kgfx_video_reconfigure()` czysci bufor pulpitu i wola to wszystko z opkodu
**GFX 30**; aplikacja biurowa po sukcesie czyta nowy `SCRW/SCRH` i maluje od nowa.

**Trzy bledy, ktore to blokowaly (wszystkie znalezione pomiarem):**

1. **Bus error w `kvideo_flush`** (pc=0x3022C). `video_render_log` wola
   `kvideo_flush` zanim `kvideo_set_mode` powstanie mape wierszy (`row_left`
   = NULL); przy `full=1` petla nie czytala mapy, ale na koncu pisala
   `row_left[y]=...` -> zapis do zera. Teraz mape alokuje takze
   `kvideo_activate_vofa`, a `kvideo_flush` ja sprawdza.
2. **VOFA.SYS bez relokacji nie moze trzymac wskaznikow.** Tablica trybow miala
   pole `const uint8_t *crtc`; modul jest linkowany pod 0 i ladowany pod
   przypadkowy adres, wiec wskaznik zostawal link-time (BIOS/niski RAM)
   i CRTC programowalo sie smieciem (`crtc13=00`, ekran gasl). Tablica trzyma
   teraz tylko skalary, a tablice CRTC wybiera `crtc_for()` (PC-relative lea).
   Regresji pilnuje nowy test `test_extra_modes_are_selected_at_runtime`.
3. **Sciezka ROM pomijala `kvideo_boot`.** Gdy `nova_initialized` (BIOS juz
   zaprogramowal karte), kernel nie ladowal VOFA.SYS, wiec `vofa_service` byl
   NULL i opkod 30 zwracal `NO VIDEO SERVICE`. Dodane `kvideo_module_attach()`
   publikuje serwis bez przeprogramowywania karty.

**Szybka zmiana trybu**: opkod 30 wola `vofa_et4000_set_mode()`, ktory przepisuje
**tylko CRTC** (bez sondy obecnosci, czyszczenia VRAM i 260 ms sprawdzania
zegara). Pelne `init()` zostaje dla zimnego startu.

Menu ma teraz trzy pozycje **SET 640x400 / SET 800x600 / SET 1024x768**
(wczesniej pozycja 0 tylko pokazywala tryb, wiec nie dalo sie wrocic do 640).

**Zrzuty** (docelowa konfiguracja, okno Hatari w rozmiarze trybu karty):
`docs/screenshots/target-mode-menu.png`, `target-mode-800x600.png`,
`target-mode-1024x768.png` oraz `target-mode-640x400.png` (640x400 bez czarnego pasa).

### Renderowanie natywne w Hatari (koniec skalowania do 640x400)

Mostek VOFA nie transkrybuje juz obrazu karty do pamieci Shiftera. To byl
ostatni limit: nawet poprawnie zaprogramowany CRTC trafial do sztywnego rastra
ST-high 640x400 (1 bajt/8 px), wiec 800x600 i 1024x768 byly obcinane albo
skalowane. Teraz `ConvST_Refresh()` (`src/conv_st.c`) rozpoznaje tryb karty
(`VOFA_GetMode`) i:

* ustawia rozmiar okna z trybu karty (`ConvGen_SetSize(cw,ch)`),
* dla `cbpp == 1` (nasz tryb mono) konwertuje VRAM karty natywnym konwerterem
  ogolnym Hatari (`ConvGen_DrawPointer`, paleta 0=czarny, 1=bialy),
* dla `cbpp == 8` zostaje wierna kopia chunky z paleta DAC karty.

W `src/video.c` usuniete zostaly: transkrypcja `VOFA_NextLine` do rastra ST
w `Video_DrawScreen` i wymuszanie trybu Shiftera w VBL (przestawialo okno
z powrotem na 640x400). W `vofa.c` wrocil oryginalny `VOFA_NextLine`
(awaryjne skalowanie nie jest juz potrzebne). Dzieki temu okno Hatari ma
800x636 i 1024x804 (tryb + pasek statusu), a pulpit jest 1:1.

**Tryb 640 to teraz prawdziwe 640x400 (bez czarnego pasa).** Wczesniej karta
skanowala 480 linii (standard VGA/EmuTOS-Nova), a ATOS malowal 400 - dolne
80 linii bylo czarne, a po powrocie z wiekszego trybu zostawaly w nim smieci
(uszkodzone wiersze 401..480). Teraz `crtc` dla 640 ma pionowe timingi
640x400@70 (VTotal 449, VDisplayEnd 400 -> `crtc[0x12]=0x8F`), a poziome
regulatory i offset pozostaja te same co w referencji EmuTOS. Dzieki temu
konsola tekstowa 80x25 wypelnia caly ekran, a pulpit nie ma czarnego pasa.
Okno Hatari dla 640 ma teraz 640x436 (640x400 + pasek statusu).
`vofa_et4000_set_mode()` nadal czysci `[height*pitch, card_bytes)`, ale dla
obecnych trybow to pasmo jest puste (widok = wysokosc karty) - zostaje jako
zabezpieczenie na przyszlosc. Test `tests/test_vofa.c` sprawdza 400 linii,
`test_vofa_reference` pilnuje zgodnosci poziomych regulatorow z upstream.

**Okna aplikacji po zmianie trybu.** Powierzchnie okien sa rysowane starym
pitch-em, a kompozytor czyta je juz nowym - otwarte okno plikow (DISK C)
wygladalo jak uszkodzone (poziome smugi) i sprawialo wrazenie zawieszenia,
choc CPU zyl (`kdbg_uf_count=0`, brak wyjatku; PC w `kernel_wm_frame_tick`).
`video_try()` po udanym przelaczeniu wola teraz `fb_paint()` obok
`paint()`/`paint_win()`, wiec okno plikow jest rysowane od nowa w nowym
trybie. Sprawdzone: F7 -> SET 1024x768 daje poprawne okno z lista plikow,
a powrot do 640 tez jest czysty.

Na prawdziwej karcie tryby tez wychodza natywnie; zegar pikseli
zostaje na domyslnym wyborze karty (MISC=0xE3, clock 0) - 800x600/1024x768
moga wymagac zaprogramowania PLL ET4000, co jest poza zakresem tego kroku.

### 5.50 Menu nad oknami + OPTIONS -> SETTINGS -> okno SETUP

**Menu na wierzchu.** Rozwijana lista FILE/OPTIONS rysowana jest w plotnie
pulpitu - a plotno to NAJNIZSZE okno w kolejnosci Z. Okno postawione na y=16
(pozwala na to zarowno opkod 20, jak i przeciaganie) zaslanialo liste. Sam
pasek 16 px byl bezpieczny od poczatku (clamp y>=16 w opkodzie 20 i w ruchu),
ale lista juz nie.

Rozwiazanie: nowy opkod **GFX 31**. Aplikacja deklaruje prostokat rozwinięcia
(`a=(x<<16)|y`, `b=(w<<16)|h`, `c=1`; `c=0` gasi), a kernel
(`kernel_wm_overlay()` w `system/kernel_wm.c`) po kompozycji i po dekoracjach
kopiuje ten prostokat z powierzchni plotna na wierzch bufora, tuz przed
kursorem. Kopia jest bajtowa (1 bpp, x wyrownany do 8 px), region jest maly
(np. 152x54 B), wiec koszt jest pomijalny. Broker GFX rozszerzony do opkodu 31
(`gfx_service_app.S`).

**OPTIONS -> SETTINGS -> SETUP.** Menu OPTIONS ma teraz jedna pozycje
**SETTINGS**. Wybranie otwiera okno **SETUP** (handle 5, zwykle okno WM,
tytul rysuje kompozytor):
- "RESOLUTION" i trzy wiersze `640x400`, `800x600`, `1024x768`,
- aktualny tryb oznaczony gwiazdka, zaznaczenie podswietlone,
- wybor mysza albo strzalkami + Enter; przelaczenie idzie przez ten sam
  opkod 30, wiec okno Hatari zmienia rozmiar natywnie,
- X w pasku tytulu zamyka okno (WCLS, tak samo jak okno plikow).

Po zmianie trybu aplikacja przerysowuje plotno, okno pulpitu, okno plikow
i okno SETUP w nowym pitchu. Wiersz statusu nie jest juz rysowany, gdy menu
jest otwarte - wczesniej nachodzil na rozwinięta liste.

Zrzuty (docelowa maszyna): `docs/screenshots/menu-above-windows.png`
(lista FILE nad oknem plikow), `setup-window.png`,
`setup-800x600.png`, `setup-1024x768.png`.

**SETUP nie blokuje juz klawiatury (naprawiony "zwis").** Gdy okno SETUP bylo
otwarte, aplikacja przechwytywala KAZDY klawisz: galaz obslugiwala tylko
gora/dol/Enter, ale i tak konsumowala reszte, wiec F10, Escape ani menu nie
reagowaly i caly pulpit wygladal na zawieszony (a CPU zyl: kdbg_uf_count=0,
proces w normalnym stanie oczekiwania na komunikat). Teraz SETUP obsluguje
tylko Up/Down/Enter/Escape, a pozostale klawisze leca dalej normalna sciezka.
Escape chowa okno SETUP - nowy wariant opkodu 20 z w=0,h=0 chowa okno
(bez czekania na X), a ponowne otwarcie SETTINGS pokazuje je z powrotem
(kernel_wm_set_window() dodaje hide/show). Sprawdzone: SETTINGS otwarte ->
F7 -> wejscie do katalogu AUTO -> F10 nadal otwiera OPTIONS, Escape je zamyka.
### 5.51 Sprzatanie pulpitu: bez okna testowego, bez przypadkowego wyjscia, jedna mysz

**Okno testowe usunięte.** DESKTOP.APP nie tworzy juz przy starcie swojego okna
ATOS DESKTOP (OFF / KERNEL WM COMPOSITOR / DRAG THE TITLE BAR). Pulpit startuje
jako: pasek menu (FILE/OPTIONS), ikony (DISK C/D, TRASH), zegar i ikona zadan.
paint_win() i powiazane wx/wy/ww/wh/button_on zniknely.

**Escape nie zamyka juz pulpitu.** Wczesniej pojedynczy Escape (mysz i klawiatura
dziela multiplekser IKBD, wiec trafialy sie przypadkowe kody) konczyl DESKTOP.APP
i wracal do shella. Teraz jedyna droga wyjscia jest FILE -> CLOSE; Escape zamyka
tylko menu/dialog.

**Jedna mysz w Hatari.** run-bios-vga.sh dodaje --grab --mousewarp true: hostowy
kursor Maca jest przechwytywany i ukrywany, a ruch relatywny napedza kursor ATOS
rysowany w VRAM karty - zamiast dwoch rozjezdzajacych sie wskaznikow jest jeden.
AltGr+M przelacza grab, AltGr+Q wychodzi z Hatari.
### 5.52 Eksplorator: dwuklik otwiera

W oknie plikow pojedynczy klik tylko zaznacza wiersz (podswietlenie), a
**dwuklik** wykonuje akcje:
- `..` - wyjscie o katalog wyzej,
- katalog - wejscie,
- plik `.PRG/.APP/.TOS/.TTP/.GTP` - uruchomienie przez `SYS_SPAWN_ASYNC` z pelna
  sciezka (np. `\AUTO\SYSINFO.PRG`); status `STARTED` albo `RUN FAILED`,
- inny plik - tylko zaznaczenie.

Dwuklik wykrywany jest lokalnie w aplikacji: `fb_double(row)` porownuje numer
wiersza i czas z `hires()` (12288 jednostek/s), prog `DBL_TICKS` = 5000 (~0,4 s).
Klikniecie w naglowek NAME/DATE/TYPE nadal sortuje pojedynczym klikiem.
Sprawdzone: F7 -> dwuklik `APP` wchodzi do `\APP`, dwuklik `DESKTOP.APP`
startuje program (status `STARTED`).

### 5.53 Kosz: podglad, przywracanie i oproznianie

**Kosz dziala.** Kernel mial juz `kfat_trash_path` (call 53), `kfat_move_path`
(call 54) i `kfat_purge_path` (call 55), ale nie mial zadnego UI. Teraz ikona
**TRASH** na pulpicie otwiera okno kosza (handle 3, zwykle okno WM - kompozytor
rysuje ramke i pasek tytulu), tak samo jednym klikiem jak DISK C / DISK D.

Uklad zawartosci:

- kazdy skasowany element lezy w swoim kubełku `\TRASH\Txxxxxxx\<nazwa>`,
  wiec okno listuje `\TRASH`, a potem czyta pojedynczy wpis wewnatrz kazdego
  kubełka i pokazuje **nazwa | kubełek | rozmiar** (katalogi jako `<DIR>`),
- wpisy nie-katalogowe w `\TRASH` (np. zostawiony tam plik) sa pomijane,
- kubełki puste (RESTORE przenosi element, ale sam katalog kubełka zostaje na
  dysku) sa pomijane - bez tego pusty kubełek pokazywal sie jako widmo o nazwie
  `Txxxxxxx`.

Operacje (przyciski lub klawiatura):

- **RESTORE** / dwuklik wiersza / **Enter** - `ATOS_CALL_MOVE` (54) na
  `\<nazwa>` (przywraca do katalogu glownego). Po udanym przeniesieniu kubełek
  jest oprozniany przez `ATOS_CALL_PURGE` (55), zeby nie zbieraly sie puste
  katalogi. Gdy nazwa juz istnieje w korzeniu, call 54 zwraca blad i status
  mowi `RESTORE FAILED (NAME IN USE?)`,
- **DELETE** / **Delete** - `ATOS_CALL_PURGE` (55) na zaznaczonym kubełku,
- **EMPTY ALL** - `ATOS_CALL_PURGE` po wszystkich kubełkach,
- **Up/Down** wybiera, **Escape** chowa okno (opkod 20 z w=0,h=0).

`\TRASH` jako taki pozostaje READ_ONLY - oprozniane sa tylko kubełki, zgodnie
z`test_trash.c`. Po zmianie trybu wideo okno kosza jest przycinane do nowego
ekranu i przerysowywane w nowym pitchu (tak jak okno plikow).

Sprawdzone na docelowej maszynie (STE/Mega, mono 640x400): okno listuje `HELLO.TXT T0000001 23` i
`OLDDATA.DAT T0000002 120`; strzalka zaznacza wiersz; Enter przywraca
`HELLO.TXT` do `\` i usuwa kubełek; Delete oprozniania drugi kubełek
(`DELETED` + `EMPTY - NOTHING TO RESTORE`). Kontrola obrazu po probie:
`\HELLO.TXT` w korzeniu, `\TRASH` zawiera tylko pomijany `DESKTOP.APP`;
`kdbg_uf_count=0`. Bramka: 41/41 OK. Zrzuty:
`docs/screenshots/trash-window.png`, `trash-restored.png`,
`trash-deleted.png`.

### 5.54 Focus manager + Atari Commander (AC) jako okno w pulpicie

**Problem.** Sesja miala JEDNEGO wlasciciela klawiatury (`gui 6` zwracal blad,
gdy ktos juz go mial), a cala mysz szla do niego. Drugie okno aplikacji nie moglo
dostac ani klawisza, ani klikniecia - wiec okno AC w pulpicie bylo niemozliwe.

**Rozwiazanie: routing wejscia per zdarzenie.**

- `gui_focus_pids[SESSION_COUNT]` (kernel_exception.c) - klient okna, ktore
  uzytkownik ostatnio nacisnal; 0 = brak okna aplikacji.
- Kompozytor ustawia focus w sciezce klikniecia (`kernel_wm.c`): okno pod
  wcisnieciem przejmuje klawiature, puste plotno / pasek zadan go czysci.
  Zamkniecie okna i wyjscie procesu tez czyszcza focus (`window_close()`,
  `user_gui_release_session()`).
- **KKEY** idzie do `gui_focus_pid`, a gdy go nie ma - do wlasciciela sesji
  (pulpit/shell). **KMOU** idzie do wlasciciela okna pod kursorem
  (`kernel_wm_pid_at`), a gdy punkt nie trafia w okno - do wlasciciela sesji.
- **Click capture**: wcisniecie zatrzaskuje cel (`mouse_capture_pid`), wiec ruch
  i zwolnienie ida do tego samego klienta. Bez tego przeciagniecie kursora z
  pulpitu nad okno aplikacji zostawialo `prev_left` pulpitu na 1 i kolejne
  klikniecia przestawaly dzialac.
- Wyjscie procesu zamyka jego okna: `kernel_wm_close_pid(pid)` - bez tego
  aplikacja konczaca prace bez wyrejestrowania zostawiala okno-widmo i martwy
  focus.

**Autoryzacja GFX.** Okno drugiej aplikacji moze teraz uzywac takze opkodu 29
(wlasna wysokosc dokumentu) i 30 z a=0 (**zapytanie** o tryb, read-only) - bez
tego nie mogloby sie dopasowac do rozdzielczosci.Zmiana trybu (30 z a!=0)
nadal nalezy do wlasciciela sesji, podobnie jak wszystkie globalne opkody.

**AC.APP - etap A** (`system/ac.c`, ~12 KB zrodla). Graficzny, dwupanelowy
menedzer plikow jako zwykle okno WM: GFX 20 (geometria), 21 (tytul), 18 (partia
`gui_draw_t`), 29 (wysokosc tresci). **Nigdy** GFX 13, wiec pulpit zostaje na
ekranie, a AC dostaje klawisze i mysz tylko wtedy, gdy jego okno jest aktywne.
Lewy panel `\`, prawy `\SYSTEM\APP`, nazwa + rozmiar (`<DIR>` dla katalogow),
wiersz `..`; TAB przelacza panel, strzalki zaznaczaja, Enter/dwuklik wchodzi do
katalogu (albo uruchamia `.PRG/.APP/.TOS/.TTP/.GTP`), Backspace wychodzi wyzej,
Escape konczy. Uklad liczy sie z rozmiaru z GFX 30, wiec 640x400, 800x600 i
1024x768 daja po prostu wiecej wierszy i szersze kolumny.

**Dowody (STE mono 640x400).** Deskryptory przez `memdump`: po kliknieciu w okno
AC `gui_focus_pids[0]=5` (pid AC) przy `gui_keyboard_pids[0]=4` (pulpit); kazde
Nacisniecie strzalki zwieksza `kdbg_wake_keys` o 2 (make+break), a zaznaczenie
przechodzi `..` -> `APP` -> `AUTO` -> `BIOS` -> `MCM`; Escape ustawia stan
procesu AC na TERMINATED, `gui_focus_pids[0]=0`, okno znika, a F10 otwiera menu
OPTIONS/SETTINGS pulpitu (czyli klawiatura wraca do pulpitu). `kdbg_uf_count=0`.
Bramka: **42/42 OK** (nowy `tests/test_gui_focus_routing.py` pinuje routing:
klawisze za focusem, mysz za oknem pod kursorem, click capture, martwy proces
nie polyka klawisza). Zrzuty: `docs/screenshots/ac-window.png`,
`ac-navigated.png`, `ac-closed.png`; probe: `atos2-tools/probe_ac.py`.

**5.55 AC etap B: pasek narzedzi i operacje na plikach**

AC ma teraz pasek narzedzi u gory (7 przyciskow z prostymi ikonami rysowanymi
z GUI_FILL/FRAME i podpisem): **COPY, MOVE, MKDIR, RENAME, TRASH, VIEW, QUIT**.
Klikniecie dziala, a te same akcje sa pod klasycznymi klawiszami Nortona/MC:
F2 rename, F3 view, F5 copy, F6 move, F7 mkdir, F8 trash, F10 quit. Kazda akcja
trafia w zaznaczony wpis aktywnego panelu, a druga panel jest celem dla
COPY/MOVE.

Operacje ida przez ABI kernela: 49 copy (a0=zrodlo a1=cel), 54 move, 50 mkdir,
53 trash, 2/3/4 open/read/close, 16 alloc. MKDIR/RENAME otwieraja modalny prompt
z kursorem (znaki z KKEY value[2], Backspace, Enter, Escape), TRASH ma
potwierdzenie ENTER/ESC. Wynik operacji pokazuje sie w dolnym pasku, a blad -
z kodem (COPY -2 = -FAT_WRITE_INVALID), co od razu zdradzilo, ze probe zaznaczyl
katalog zamiast pliku. VIEW czyta do 2048 B i wyswietla tekst z przewijaniem
Up/Down.

**Dwa bledy rysowania znalezione i naprawione przy okazji:**

- Podpisy przyciskow byly pusta tablica wskaznikow (const char *tb[7]).
  Aplikacje sa ladowane pod adresem runtime i **bezwzgledne wskazniki w danych
  nie sa relokowane**, wiec tablica czytala smieci. Podpisy ida teraz z funkcji
  switch (literaly sa PC-relative). To ten sam blad, co kiedys z tablica
  trybow VOFA.SYS.
- Dolny pasek byl czarny, ale tekst mial background=1 (papier), wiec zamalowywal
  pasek bialym, a legenda byla ucinana do 31 znakow (text[32]). Tekst jest teraz
  inverse (background=0) i krotszy.

**Sprawdzone** (STE mono 640x400): pasek z ikonami i podpisami; prompt
NEW FOLDER NAME:; mkdir tworzy katalog (\PACK potwierdzony na obrazie); copy
pliku do drugiego panelu (\SYSTEM\APP\AUTOEXEC.PRG na obrazie); dolny pasek
inverse; kdbg_uf_count=0; bramka 42/42 OK. Zrzuty:
docs/screenshots/ac-toolbar.png, ac-prompt.png, ac-status.png;
probe: atos2-tools/probe_acd.py.

**Uwaga o probe:** harness atos2_auto.Hatari nie uzywa --grab, wiec mysz hosta
dostarcza delty IKBD i pozycja kursora dryfuje. Klikniecia w konkretny przycisk
paska sa przez to niestabilne - testy akcji robic klawiszami, a klik weryfikowac
osobno (jak w 5.54).

**Co dalej.** Etap C: skalowanie i resize okna AC pod 800x600/1024x768 (uklad
juz liczy sie z GFX 30, zostaje weryfikacja na docelowej ET4000 i reakcja na
zmiane trybu w locie), etap D (osobno): paleta kolorow systemowych + tryb
pelnoekranowy AC w osobnej sesji (architektura focusu jest per sesja, wiec ten
sam kod zadziala jako wlasciciel calej sesji bez okna).

### 5.56 AC etap C: skalowanie 640x400 / 800x600 / 1024x768

Uklad AC byl juz liczony z rozmiaru okna, ale byly dwie dziury: okno nie
reagowalo na zmiane rozmiaru (WMOV niósl tylko x,y) i kernel nie ruszal okien
aplikacji przy zmianie trybu wideo.

**Kernel.**

- `kernel_wm_reconfigure()` oprocz konsoli i plotna pulpitu przycina teraz
  zwykle okna aplikacji (flags==1) do nowego ekranu i, gdy ramka sie zmienila,
  powiadamia wlasciciela przez `notify_geometry()` (czyli WMOV).
- Opcode 20 jest **autorytatywny** takze dla istniejacego okna: klient, ktory
  re-deklaruje geometrie (okno przeliczone po zmianie trybu, okno dialogowe
  wysrodkowane na nowo), naprawde sie przesuwa/zmienia rozmiar. Wczesniej
  opcode 20 zapisywal tylko `wm_window_req`, a kompozytor rysowal ze starej
  ramki w `wm.win[]`, wiec re-deklaracja nic nie zmieniala. Tlo pulpitu
  (flaga bit 2) jest wykluczone.

**AC.**

- `apply_size()` liczy `client_h`, szerokosc panelu, `px[]` i liczbe wierszy
  z rozmiaru okna; `layout_full()` ustawia okno na `(8,24)` i rozmiar
  `ekran-16 x ekran-40`. Uklad idzie wiec za trybem i za recznym resize.
- Obsluga WMOV: jesli zapytanie `GFX 30` pokaze inny tryb niz zapamietany,
  AC przelicza sie na caly nowy ekran (`layout_full` + opcode 20 + repaint);
  w przeciwnym razie przyjmuje nowe x,y,w,h (przeciaganie/rozciaganie okna)
  i przelicza uklad.

**Sprawdzone na docelowej ET4000** (`SDL_VIDEODRIVER=dummy`, tryb przelaczony
na zywo z OPTIONS -> SETTINGS wg logu `VOFA-MODE`):

- 800x600: `VOFA-MODE w=800 h=600 stride=100`; AC jako okno 784x560 z paskiem
  narzedzi, dwiema panelami i paskiem statusu (`docs/screenshots/ac-800.png`).
- 1024x768: `VOFA-MODE w=1024 h=768 stride=128`; AC 1008x728, panele 498 px
  (`docs/screenshots/ac-1024.png`).
- 640x400 (harness STE): bez zmian, jak w 5.54/5.55.
- Regresja pulpitu po zmianach w kernelu: okno DISK C i okno SETUP otwieraja
  sie na swoim miejscu, `kdbg_uf_count=0`; bramka **42/42 OK**.

Probe: `atos2-tools/probe_ac_scale.py` (cel) i `atos2-tools/probe_scw.py`
(regresja pulpitu).

**Uwaga.** `SYSTEM/VIDEO.CFG` ("VOFA 800"/"VOFA 1024") jest czytany tylko na
sciezce `kvideo_boot`, nie na sciezce ROM, ktorej uzywa docelowy BIOS - dlatego
test szedl przez przelaczenie trybu w SETTINGS, a nie przez plik startowy.

**Co dalej.** Etap D (osobno): paleta kolorow systemowych (dzis caly system to
1 bpp) + tryb pelnoekranowy AC w osobnej sesji/desktopie w tle. Architektura
focusu i ukladu jest per sesja i niezalezna od okna, wiec ten sam kod zadziala
jako wlasciciel calej sesji.

### 5.57 Etap D: kolory systemowe (paleta + motywy)

**Problem.** Caly system byl 1 bpp, a natywny konwerter VOFA rysowal tryb 1 bpp
twardymi kolorami czarny/bialy (`ConvGen_SetPaletteColor(0,0,0,0)` i
`(1,255,255,255)`), wiec paleta nie miala zadnego wplywu na obraz.

**Rozwiazanie - paleta systemowa sterujaca dwoma kolorami trybu 1 bpp:**

- `conv_st.c` (Hatari): sciezka `cbpp==1` czyta teraz wpisy 0 i 1 palety ST
  (`IoMem_ReadWord(0xff8240/0xff8242)`, 4 bity na dzialke) zamiast stalych
  czern/biel - to, co ustawi kernel, jest tym, co widac.
- `kernel_graphics.c`: `kgfx_set_palette()` dziala w kazdym trybie (wczesniej
  odmawial dla mono) i zachowuje 4 bity na dzialke (`0x0fff`).
- `kgfx_init()` programuje domyslna palete systemowa (16 wpisow), zanim
  cokolwiek sie narysuje - bez tego startowy ekran bylby czarny, bo ST paleta
  jest wyzerowana.
- `include/atos_theme.h`: nazwane kolory systemowe (`ATOS_COLOR_BACKDROP`=0,
  `ATOS_COLOR_PAPER`=1, ramka, zaznaczenie, tytul, status, ostrzezenie).
- DESKTOP.APP: **OPTIONS -> THEME** cyklicznie ustawia motyw (MONO / BLUE /
  GREEN) przez GFX opcode 8 dla wszystkich 16 wpisow. AC nie zmienil ani linii
  kodu - rysuje indeksami 0/1, wiec idzie za motywem razem z pulpitem.

**Sprawdzone na docelowej ET4000** (probkowanie pikseli zrzutow): MONO tlo
`(136,136,136)` (dither czern/biel) - `theme-mono.png`; BLUE tlo `(0,0,34)`,
status `THEME BLUE` - `theme-blue.png`; GREEN tlo `(0,34,0)`, a otwarte okno AC
w `(0,102,0)/(0,34,0)` - `theme-green-ac.png`. Bramka **42/42 OK**, regresja
pulpitu bez zmian. Probe: `atos2-tools/probe_theme.py`.

**Zakres i dalszy krok.** W trybie 1 bpp widoczne sa dokladnie 2 kolory, wiec
motyw zmienia wyglad calego systemu, ale nie daje 16 kolorow naraz. Pelna
paleta wymaga trybu kolorowego: sciezki `cbpp==4` (planarnej, tak jak istnieje
juz prywatna sciezka 1 bpp), pozycji trybu w `vofa_et4000.c` i ustawienia
`graphics.planes=4`/`line_bytes=width/2` dla VOFA. Renderer planarny kernela jest
ogolny (`emutos_vdi_planar_fill`, `surface_pixel_set` obsluguja dowolna liczbe
planow), wiec to jest wykonalne jako osobny krok; paleta i nazwy kolorow sa juz
gotowe.

### 5.60 Launcher na dole pulpitu (konfigurowalny)

**Cel.** Zeby nie klikac za kazdym razem FILE -> RUN... i nie wpisywac nazwy
programu, pulpit ma na dole pasek z przyciskami aplikacji.

**Konfiguracja** - `\SYSTEM\LAUNCH.CFG`, jedna linia na pozycje:
`LABEL=sciezka`, komentarz zaczyna `;` albo `#`. Wartosci specjalne:
`*TERMINAL` (przywoluje okno konsoli) i `*TRASH` (otwiera okno kosza).
Brak pliku albo pusty plik = wbudowana lista domyslna. Plik jest instalowany
z `--if-missing`, wiec edycje uzytkownika przezywaja reinstalacje. Domyslna
zawartosc ma Atari Commander na pierwszym miejscu:

```
COMMANDER=\SYSTEM\APP\AC.APP
TERMINAL=*TERMINAL
SYSINFO=\SYSTEM\APP\SYSINFO.APP
TRASH=*TRASH
```

**Rysowanie i z-kolejnosc.** Pasek (`LBAR_H` 26 px) rysuje DESKTOP.APP na
swoim plotnie, ale plotno jest najnizsza warstwa - dlatego pasek jest
deklarowany jako **overlay** (GFX opcode 31), tak samo jak rozwijane menu.
Gdy menu jest otwarte, overlay wskazuje liste menu; gdy zamkniete - pasek
launcher-a. Dzieki temu pasek zostaje nad oknami aplikacji.

**Klikniecie** sprawdza pasek przed ikonami pulpitu i oknami i wywoluje
`SYS_SPAWN_ASYNC` z pelna sciezka (albo `*TERMINAL` / `*TRASH`). Miesci sie
tylko tyle pozycji, ile szerokosc ekranu pozwala (`(SCRW-8)/LBAR_W`), wiec na
640 px widac 6 z 8 mozliwych.

**Sprawdzone** (STE mono 640x400): pasek widoczny z przyciskami
`COMMANDER | TERMINAL | SYSINFO | TRASH` (`docs/screenshots/desktop-launcher.png`);
klik w COMMANDER uruchamia AC jako okno, a pasek pozostaje nad nim -
`docs/screenshots/launcher-start-ac.png`. Klik weryfikowalem przez tymczasowe
przepiecie klawisza testowego F5 na `inject(...)` w pozycji przycisku (potem
przywrocone). Bramka **42/42 OK**. Probe: `atos2-tools/probe_lbar.py`
i `probe_lbt.py`.

### 5.61 Terminal jako okno 1/3 + budzenie aplikacji z launcher-a

**Terminal.** Okno konsoli nie zabiera juz calego ekranu: `CONSOLE_WH` to
`screen_h/3` (na 640x400 ok. 133 px), a siatka powloki zostaje 80x25 - okno
pokazuje wiersze wokol kursora przez `src_y`. Okno ma belke z tytulem
**TERMINAL** i gadzet zamykania (wczesniej tytul brzmial SHELL, a okno bylo
rozciagniete na caly ekran).

**Budzenie zamiast drugiej kopii.** To byla przyczyna "zwisu": launcher
odpalal drugi egzemplarz AC, gdy pierwszy byl zwinienty - dwa GUI walczyly o
klawiature i ekran. Teraz:

- `launch_t` pamieta `pid` zwrocony przez `ATOS_CALL_SPAWN_ASYNC`
  (syscall 34 wypelnia `request->pid`),
- klik sprawdza `ATOS_CALL_CHILD_QUERY` (32): jesli proces zyje
  (`state != 5`), wola nowy **GFX opcode 32** `kernel_wm_activate_pid(pid)`,
- kernel odnajduje okno tego wlasciciela, **przywraca je z minimalizacji**
  (`window_restore`), podnosi (`wm_raise`) i ustawia fokus klawiatury.

**Minimalizacja.** `window_minimize()` zwalnia fokus, gdy zwiniete okno bylo
sfokusowane - wczesniej klawisze szly do ukrytego okna.

**Sprawdzone** (STE mono 640x400, zrzut + stan procesow): po pierwszym kliknie
COMMANDER w tabeli procesow jest **1x AC.APP** (pid 5), po drugim kliknieciu
**nadal 1x AC.APP** - czyli drugie klikniecie weszlo sciezka RESTORE, nie
SPAWN. Wczesniej kazde klikniecie tworzylo nowy proces. Terminal:
`docs/screenshots/terminal-third.png`. Bramka **42/42 OK**. Probe:
`atos2-tools/probe_wk2.py`, `probe_wk3.py`, `probe_termscr.py`.


### 5.63 Kursor wg trybu, znikajacy dialog SETUP, stary pasek menu

**Kursor przyciety do 640x400.** `kernel_sessions.c` przycinal pozycje myszy na
sztywno (`mouse_x>639 -> 639`, `mouse_y>399 -> 399`) niezaleznie od aktywnego
trybu, wiec w 800x600 i 1024x768 kursor zatrzymywal sie w 3/4 ekranu. Teraz
zakres pochodzi z `kgfx_get_info()` (`pointer_max_x/y`), a test w bramce
sprawdza, ze w trybie 800x600 wstrzykniecie (900,700) daje (799,599).

**Znikajacy dialog SETUP.** `video_try()` po zmianie rozdzielczosci odnawial
tylko okna plikow i kosza. Okno SETUP jest osobna powierzchnia WM rysowana przy
starej szerokosci linii, wiec kompozytor czytal je zlym krokiem i dialog
wygladal jak znikniety. Dodane `setup_after_mode()` (clamp do nowego ekranu +
`GFX_WINDOW` + `setup_paint()`), wolane z `video_try()`.

**Stary pasek menu nad dialogiem.** `menu_activate()` dla OPTIONS robil
`setup_open_it()` i `return` bez `paint()` (inaczej niz dla FILE). Skutek:
plotno pulpitu dalej pokazywalo rozwiniete menu, a overlay (GFX 31) zostawal na
prostokacie menu - menu bylo rysowane na wierzchu nowo otwartego okna SETUP.
Teraz po wybraniu pozycji OPTIONS pulpit jest przemalowany.

**Sprawdzone na docelowej ET4000:** SETTINGS -> Down -> Enter przelacza tryb
(`VOFA-MODE w=800 h=600`), a dialog SETUP pozostaje widoczny i poprawnie
narysowany po zmianie. Bramka **42/42 OK**. Probe: `atos2-tools/probe_fx.py`,
`probe_sm.py`.

**Kolory** - nie udalo sie odtworzyc zgloszenia: pomiar pikseli w 640x400,
800x600 i 1024x768 (headless i w oknie) daje czysta czern i biel, zgodnie z
`bpp=1`; DAC karty trzyma wpisy 0/1. Do wyjasnienia, czy chodzi o motyw
(BLUE/GREEN zmieniaja dwa widoczne kolory).

### 5.62 Stany przyciskow launcher-a i jedna instancja aplikacji

**Kolory jak w pasku zadan:** nieuruchomiona = **biala** (wypukly przycisk),
uruchomiona = **czarna** (biale napisy), zwinięta = **szara** (w 1 bpp szarosc
to dither `GUI_BG`, bo tryb mono ma tylko dwa kolory).

Stan pochodzi z kernela przez nowy **GFX opcode 33**
(`kernel_wm_window_state(pid)`: 0 = brak okna, 1 = widoczne, 2 = zwiniete;
dopasowanie po `window_owner_pid`, bo `wm.win[].pid` to wskaznik powierzchni,
nie pid procesu). Jesli opcode nie odpowie, jest fallback na zywotnosc
procesu (`ATOS_CALL_CHILD_QUERY`), zeby uruchomiona aplikacja nigdy nie
pokazala sie jako bialа.

**Jedna instancja na desktop.** Klik w przycisk uruchomionej aplikacji nie
tworzy drugiej kopii: `launch_t.pid` pamieta pid z `SPAWN_ASYNC`, a klik wola
`GFX opcode 32` -> `kernel_wm_activate_pid(pid)` (przywrocenie z minimalizacji,
podniesienie, fokus). To naprawia "zwis" z dwoch walczacych kopii AC.

**Krytyczny szczegol: broker.** `SYSTEM/GFX.APP` (`system/gfx_service_app.S`)
odrzucal kazdy opcode **> 31** (`cmpi.l #31/%d2; bhi command_invalid`), wiec
opkody 32/33 nigdy nie docieraly do kernela. Limit podniesiony do 33.

**Pomiar** (`atos2-tools/probe_states2.py`, probkowanie pikseli przycisku):

- nieuruchomiona: **white 89% / black 10%** (bialy przycisk),
- uruchomiona: **white 5% / black 94%** (czarny).

Stan **szary (zwinięta)** jest zaimplementowany (stan 2), ale w automacie nie
udalo sie go potwierdzic zrzutem: wstrzykniete klikniecie w gadzet minimalizacji
zamiast stanu 2 dawalo stan 1 (okno znikalo z ekranu, proces zyl). Do
potwierdzenia prawdziwa mysza: zwij okno AC i sprawdz, czy przycisk COMMANDER
jest szary. Bramka **42/42 OK**.

**Uwaga na przyszlosc.** Gdyby zamkniecie okna X-em nie konczylo procesu
(AC obsluguje `WCLS` przez `quit=1`, wiec konczy), launcher moglby zostac bez
sposobu na okno; dlatego stan 0 jest traktowany jako "nieuruchomiona".

**Uwaga.** Pierwszy wiersz tekstu w terminalu lezy tuz pod belka tytulu;
jesli bedzie nachodzil, do poprawienia jest offset `client.y` dla okna konsoli
w kompozytorze.
**5.57a Sprostowanie: paleta przez DAC karty, nie przez rejestry Shiftera.**

Pierwsza wersja 5.57 kazala konwerterowi `cbpp==1` czytac rejestry palety ST
(`IoMem_ReadWord(0xff8240/0xff8242)`). To bylo bledne z dwoch powodow: na tej
karcie liczy sie **RAMDAC** (rejestry Shiftera sa niezwiazane ze scanoutem),
a w emulacji rejestry ST sa nadpisywane przez sciezke video ST - na zrzucie
pojawily sie teal `(0,136,136)` i bladozolty `(255,255,119)` zamiast czerni i
bieli. Poprawka:

- `vofa_et4000_set_palette()` (`system/vofa_et4000.c`) programuje wpis RAMDAC
  (0x3C8/0x3C9, 6 bitow na dzialke); vtable uslugi video podniesiony do **v3**
  (`KERNEL_VIDEO_VERSION 3`) i uzupelniony w `vofa_module.c`.
- `kvideo_set_palette()` (`kernel_video_boot.c`) zamienia `$0RGB` (4 bity)
  na 6 bitow; `kgfx_set_palette()` wola je, gdy VOFA jest aktywne.
- `conv_st.c` `cbpp==1` czyta `VOFA_GetDAC()` i rozszerza 6->8 bitow
  (`(v<<2)|(v>>4)`), tak samo jak istniejąca sciezka `cbpp==8`.

Wynik (docelowa ET4000, probkowanie pikseli): MONO menu `(255,255,255)` na
tle `(0,0,0)`; po przelaczeniu motywu BLUE tlo `(0,0,170)` - czyli dokladna
wartosc z palety systemowej. Nadal jestesmy w trybie **1 bpp mono**
(`VOFA-MODE ... bpp=1`); motyw zmienia dwa widoczne kolory, 16 kolorow
wymaga trybu `cbpp==4` (patrz koniec 5.57). Bramka 42/42.
### 5.59 Katalog sterownikow kart + wybor drivera przy starcie

**Problem.** VOFA.SYS lezal plasko w \\SYSTEM i byl jedynym, zaszytym sterownikiem;
dodanie W32i (akceleracja) albo Mach32/64 znaczyloby zmiany w kernelu.

**Rozwiazanie - katalog + manifest + wybor po wykryciu z BIOS:**

- **`\SYSTEM\DRV\`** - nowy katalog na sterowniki kart, po jednym module na
  rodzine. Na razie `ET4000.SYS`; w kolejce `W32I.SYS` (akceleracja),
  `MACH32.SYS`, `MACH64.SYS`. `\SYSTEM\VOFA.SYS` zostaje jako fallback dla
  obrazow bez katalogu.
- **FAT**: `kfat_find_driver_file()` / `kfat_load_driver_file()`
  (`system/kernel_fat.c`) odnajduja plik w `\SYSTEM\DRV` (normalny loader
  modulow czyta `\SYSTEM` plasko, wiec trzeba zejsc o poziom glebiej).
- **Loader**: `kernel_load_driver_module()` (`system/kernel_module.c`) - to samo
  co loader systemowy, ale z katalogu DRV.
- **BIOS juz wie, co to za karta**: `hw/nova.c` wykrywa `MACH64`, `MACH32`,
  `ET4000/W32 FAMILY` (+ adapter VME / CrazyDots / VOFA) i przekazuje to
  w handoffie (`reserved0` bit7=valid, bity0..6=adapter; `reserved1`=chipset).
  Kernel zapamietuje to przez `kvideo_set_detected()`
  (`system/kernel_main.c`).
- **Manifest** (`system/kernel_video_boot.c`): `chip -> lista kandydatow`, od
  najbardziej konkretnego. Dla rodziny ET4000 najpierw `W32I.SYS`, potem
  `ET4000.SYS`; dla Mach32/Mach64 odpowiednio. Pierwszy, ktory sie zaladuje
  i opublikuje usluge video, wygrywa; potem fallback na `\SYSTEM\VOFA.SYS`.
- **Kontrakt**: `include/graphics_device.h` opisuje docelowy `gfx_device_t`
  (caps/formats/mode_info/set_palette/damage/flush/cursor/blit) i zasade, ze
  **zaden kod poza sterownikiem nie testuje nazwy chipu** - wybiera sciezke po
  `caps`. To jest interfejs, do ktorego przejdą VDI i kompozytor (etap K1/K2
  z 5.58); dzis dziala juz warstwa wyboru i ladowania.

**Sprawdzone na docelowej ET4000** - log konsoli startowej pokazuje caly
przebieg (zrzut `docs/screenshots/boot-driver-select.png`):

```
VIDEO: VOFA (ROM INIT) / 640x400 MONO / VIEW 640x400
VIDEO DRV TRY: W32I    SYS
VIDEO DRV TRY: ET4000  SYS
VIDEO DRV: ET4000  SYS
```

czyli: BIOS zaraportowal rodzine ET4000, manifest sprobowal sterownika
z akceleracja (nieobecny), potem podstawowego - i ten sie zaladowal. Bramka
**42/42 OK**, pulpit i AC bez zmian. Probe: `atos2-tools/probe_drv.py`.

**Dodanie nowej karty to teraz**: nowy `\SYSTEM\DRV\<NAZWA>.SYS` + jedna
linia w manifeście. Bez zmian w `kernel_graphics.c`, `kernel_wm.c`,
`desktop_wm.c`, `tos_vdi.c`.
### 5.58 Etap D2: AC pelnoekranowy + COMMAND.APP = AC

**Wykrywanie srodowiska.** AC probuje najpierw zostac wlascicielem klawiatury
przez brokera GUI (`gui 6`, czyli kernel call 47).

- Udalo sie (brak innego GUI, np. uruchomienie z konsoli albo we wlasnej
  sesji): AC wchodzi w tryb pulpitu (`gfx 13` = `kgfx_enter_desktop`) i zajmuje
  **caly ekran** - bez okna, bez paska tytulu, `client_h = scrh`. Klawiatura i
  mysz ida do niego sciezka wlasciciela sesji (focus=0), wiec routing
  z 5.54 dziala bez zmian.
- Nie udalo sie (pulpit juz trzyma klawiature): AC zostaje **oknem** i dostaje
  wejscie przez focus manager (klik-to-focus).

Wyjscie: `gui 7` (zwolnienie klawiatury) + `gfx 6` (`kgfx_restore_console`),
wiec konsola wraca do shella.

**COMMAND.APP = AC.** Shell nie uruchamia COMMAND.APP sam - tylko gdy
uzytkownik wpisze `command` (shell dokleja `.APP`). Instalator kladzie wiec:

- `SYSTEM/APP/COMMAND.APP` = AC (ten sam plik co AC.APP),
- `SYSTEM/APP/AC.APP` = AC (`ac`),
- `SYSTEM/APP/TEXTCMD.APP` = dotychczasowy tekstowy commander (nic nie ginie).

**Sprawdzone** (STE mono 640x400): FILE -> CLOSE zamyka pulpit i wraca shell
(`[PID 4 EXIT 0]`); wpisanie `command` daje **pelnoekranowe AC** (status
`FULL SCREEN`, `kdbg_kbpid` = pid AC, `kdbg_uf_count=0`) -
`docs/screenshots/ac-fullscreen.png`; w pulpicie AC nadal jest oknem z
paskiem narzedzi. Bramka **42/42 OK**. Probe: `atos2-tools/probe_acfs.py`
i `atos2-tools/probe_cmd.py`.

**Tryb "inny desktop w tle".** Architektura jest gotowa: wlasciciel klawiatury
i focus sa per sesja (`gui_keyboard_pids[SESSION_COUNT]`, `gui_focus_pids[]`),
a uklad AC nie zalezy od okna. Uruchomienie AC w drugiej sesji (`Shift+Alt+F10`)
da drugi, niezalezny pelnoekranowy desktop; brakuje tylko zautomatyzowanego
startu AC jako "pulpitu" tej sesji.

### 5.65 Zabezpieczenie swiezo otwartego okna

Nie udalo sie odtworzyc znikania dialogu SETUP zastrzykami myszy (czysty ruch,
ruch z wcisnietym przyciskiem, przemiatanie; 640x400 i 800x600, STE i ET4000).
Najbardziej prawdopodobna przyczyna: na prawdziwej myszy (Mac + Hatari
z opcjami --grab/--mousewarp) przy ruchu przychodzi pojedynczy przypadkowy
impuls przycisku; trafia w gadzet X nowo otwartego okna i ono znika.

Zabezpieczenie: wm_open_frame (kernel_wm.c) pamieta klatke, w ktorej okno
zostalo zadeklarowane (kernel_wm_set_window), a akcja CLOSE/MINIMIZE jest
ignorowana, gdy od otwarcia minelo mniej niz 20 klatek (~0,4 s). Gest, ktory
otworzyl okno, nie moze go od razu zamknac. Diagnostyka: gdy okno zamknie
kompozytor, pulpit pokazuje 'WM CLOSED SETUP' w linii statusu. Bramka 42/42.

### 5.66 Szybka zmiana rozmiaru okna (1024x768)

**Objaw.** Przy zmianie rozmiaru okna AC w 1024x768 wszystko bylo bardzo wolne,
nie dalo sie przerwac, a okno 'samo' ruszalo sie powoli - kolejka myszy
zapychala sie za wieloma pakietami.

**Przyczyna.** Przenoszenie okna rysowalo kontur tanio (XOR prosto do bufora
klatki + `outline_video_damage`), ale **zmiana rozmiaru** szla przez kompozytor:
`outline_damage()` dodawalo uszkodzenie 4 krawedzi o rozmiarze okna, a petla
klatek robila pelny `wm_recompose` (zawartosc okna + dekoracje) dla kazdego
pakietu myszy. Przy 1024x768 to pasy ~1000 px szerokosci i ~700 px wysokosci na
kazdy ruch.

**Poprawka.** Resize uzywa teraz tej samej taniej metody co move:
`xor_outline()` + `outline_video_damage()` prosto w bufor, z `kvideo_flush()`
tylko dla obramowania, i tylko gdy prostokat faktycznie sie zmienil. Usuniete
`outline_damage`/`drag_outline` i rysowanie konturu w przebiegu dekoracji
(kompozytor nie moze zamalowac konturu XOR). Koszt kroku zmiany rozmiaru to
~2*(w+h) pikseli zamiast przemalowania okna.

**Sprawdzone** (STE 640x400): skryptowany resize okna DISK C dziala, okno
zmienia rozmiar, brak pozostawionego konturu (`docs/screenshots/resize-cheap.png`).
Bramka **42/42 OK**. Probe: `atos2-tools/probe_rs.py`.

### 5.67 Powiększanie okna działa (kontrakt AES work area)

**Objaw.** Zmniejszyć okno się dało, ale powiększyć nie: kontur i ramka rosły,
a obraz (klient) zostawał mały.

**Przyczyna.** `wm_resize()` w `system/wm_core.c` wyprowadzał rozmiar klienta ze
**starego** rozmiaru i tylko go obcinał:

```c
uint16_t cw=s->win[i].client.w;      /* stary rozmiar */
if(cx+cw>w)cw=w-cx;                  /* tylko w dół */
```

Przy powiększaniu `cx+cw<=w`, więc klient zostawał stary - stąd "rośnie cień, a
obraz nie".

**Poprawka (zgodna z AES).** Dodane `wm_resize_client_to_frame()`: klient =
ramka minus marginesy, czyli `wind_calc(WC_WORK)` / `delta` w XaAES. Kernel woła
je po `wm_resize()` tylko dla okien aplikacji (`window_owner_pid!=0`); okno
powłoki ma stały bitmap 80x25, więc zostaje bez zmian.

**Jak to robi AES (sprawdzone w źródłach):**

- **EmuTOS** `aes/gemwmlib.c`, `wind_set`: dla `WF_CURRXYWH` rozróżnia *size* od
  *move* po prostokącie (`pt->g_x==c.g_x && pt->g_y==c.g_y`), przebudowuje listy
  prostokątów (`newrect`, `everyobj`), ustawia aktywny/ctrl prostokąt
  (`w_setactive`) i odświeża; obszar roboczy jest funkcją ramki.
- **XaAES** `xaaes/src.km/c_window.c`: trzyma `delta` ramka<->roboczy (`f2w`),
  przy zmianie rozmiaru obcina do `WIND_HMIN`, aktualizuje prostokąt okna,
  `update_windows_below()`, i wysyła klientowi `WM_SIZED` (nowe x,y,w,h) oraz
  redraw. My robimy to samo: uszkodzenie starej i nowej ramki + `WMOV` do
  aplikacji, która sie przerysowuje.

**Test w bramce** (`tests/test_wm_core.c`): po `wm_resize` na 340x230 klient ma
`cw+140` / `ch+110` - czyli rośnie. Bramka **42/42 OK**.

### 5.68 Powiększanie okna: WMOV musi nieść handle (wzorzec AES WM_SIZED)

Po dodaniu `wm_resize_client_to_frame()` kernel poprawnie zwiększał klienta
(odczyt z pamięci: frame 420x300 -> 521x360, client 420x284 -> 521x344), ale
obraz nadal sie nie powiekszal. Przyczyna byla po stronie klienta: **WMOV nie
niesie handle okna**, wiec pulpit rozpoznawal je po rozmiarze:

```c
if(fb_open&&nw==fb_ww&&nh==fb_wh){fb_wx=nx;fb_wy=ny;}   /* resize => brak trafienia */
```

Przy zmianie rozmiaru rozmiar nigdy nie pasuje do zapamietanego, wiec wiadomosc
byla **odrzucana** - okno nie aktualizowalo geometrii ani nie przerysowywalo
nowego obszaru.

**Poprawka.** `WMOV` niesie teraz `value[3]` = handle okna (tak jak XaAES w
`WM_SIZED`/`WM_MOVED`), a pulpit routuje po handle i **przyjmuje nowy rozmiar**
(`fb_ww/fb_wh`, przeliczenie stron, `fb_paint()`).

**Sprawdzone:** skryptowany test powiekszania okna DISK C - okno rosnie, a
tresc (pasek sciezki, naglowki kolumn, wiersze) jest przerysowana w nowym
obszarze. Bramka **42/42 OK**.

### 5.69 Wydajnosc: co zrobione, co dalej

Zrobione: kompozycja tylko z damage; kontur move/resize XOR prosto do bufora;
odrysowanie 2 wierszy przy strzalkach i kliknieciach w AC; oproznianie calej
kolejki myszy w jednej klatce; `kvideo_flush` zapisuje tylko uszkodzone spany
z porownaniem z cieniem (mniej ruchu na magistrali).

Do zrobienia (kolejnosc wg zysku): (1) nawigacja w menu bez pelnego repaintu
plotna (teraz kazda strzalka w menu rysuje pelnoekranowy dither); (2) flush
zapisami 16-bitowymi tam, gdzie adapter na to pozwala (polowa transakcji);
(3) pominięcie porownania z cieniem dla damage znanego jako zmieniony.
Na 68000 kluczowe jest utrzymanie malego damage (bez pelnoekranowych
repaintow) - inaczej 8 MHz nie nadazy nawet przy 640x400.

### 5.70 Optymalizacje: menu bez pelnego repaintu + flush tylko uszkodzonych wierszy

**1. Nawigacja w menu.** paint_menu() rysuje wylacznie prostokat rozwijanej
listy (tlo listy + ramka + pozycje) i deklaruje overlay. Wczesniej kazda
strzalka w menu wolala paint(), ktore rysowalo CALY ekran: pelnoekranowy dither
pulpitu (98 KB przy 1024x768), ikony, launcher i pasek statusu. Teraz to jeden
maly prostokat (~90x32 px). Pelny paint() zostal tam, gdzie trzeba usunac liste
(zamkniecie menu / Escape) albo ja zatwierdzic.

**2. Flush tylko uszkodzonych wierszy.** kvideo_flush() iterowal
for(y=0;y<mode_h;y++), czyli 768 wierszy przy 1024x768, nawet gdy uszkodzone
bylo kilka. Dodane row_min/row_max (aktualizowane w kvideo_damage, zerowane po
flushu) zwezaja petle do faktycznie zmienionego zakresu; tylko ten zakres jest
tez zerowany. Przy odrysowaniu dwoch wierszy AC to 2 obroty petli zamiast 768.

**Sprawdzone** (STE 640x400): menu OPTIONS otwiera sie i podswietla pozycje
strzalkami (bez artefaktow, tlo i launcher nietkniete), a Escape usuwa liste i
przywraca dither (probka 50/50 czern/biel). Bramka 42/42 OK.

**Niezrobione jeszcze:** (a) zapisy 16-bitowe w flushu - VOFA jest adresowany
bajtowo i kolejnosc bajtow w slowie byla powodem uzycia dwoch zapisow
bajtowych, wiec trzeba by to najpierw zweryfikowac; (b) pominiecie porownania z
cieniem dla damage znanego jako zmieniony.

### 5.71 Flush zapisami 16-bitowymi + ocena cache fontow/ikon i NVDI

**Zapis slowem.** `kvideo_flush()` pisal dwa bajty na slowo:
`card[o*2]=value>>8; card[o*2+1]=value;`. Karta siedzi na 16-bitowej
magistrali, a jej zapis slowem robi dokladnie to samo (w Hatari
`VOFA_Mem_wput` = `bput(addr,val>>8); bput(addr+1,val)`). Teraz jest jeden
zapis 16-bitowy - **polowa transakcji** przy tym samym obrazie VRAM.

**Weryfikacja kolejnosci bajtow:** powrot do konsoli startowej - tekst
(`FAT LAYER: FAT16 MOUNTED`, `VIDEO DRV: ET4000 SYS`, znacznik `[1] v`)
renderuje sie bez garbienia, czyli starszy bajt trafia pod adres parzysty.
Bramka **42/42 OK**.

**Cache fontow/ikon - ocena (z kodu):**

- `mono_glyph()` (`emutos_vdi_mono.c:151`) rysuje glif **slowami**: na kazdy
  z 16 wierszy 32-bitowy shift/maska i 1-2 read-modify-write. Cache
  przesunietych glifow (klucz: znak + `x&15`) dalby ~20-30% na samym tekscie,
  ale tekst nie jest waskim gardlem - dominantą jest ruch pamieci.
- tlo/dither (`emutos_vdi_planar_fill`) to juz petla `move.w` z `dbra`;
  wzorzec jest liczony raz na wiersz.
- ikony to statyczne bitmapy blitowane raz na przemalowanie tla - cache
  nie ma tu czego przyspieszyc.
- blit kompozytora uzywa **blittera sprzetowego, gdy jest**
  (`kblit_copy_planar_to`), inaczej szybkiej petli CPU.

**Kluczowe ustalenie:** docelowa maszyna to **Mega ST + ET4000, czyli bez
blittera ST** - dlatego wszystko idzie na CPU. W tym ukladzie prawdziwa
akceleracja (i to wlasnie droga NVDI) to **blitter karty**: ET4000/W32i ma
2D blitter. Miejsce na to jest juz przygotowane: sterownik `W32I.SYS`,
`GFX_CAP_HW_BLIT` w `graphics_device.h` i manifest sterownikow w
`kernel_video_boot.c`. To nastepny duzy krok, a nie cache glifow.

**NVDI robi trzy rzeczy, ktore i my robimy lub mozemy zrobic:** (1) minimalny
redraw/damage - mamy; (2) wstepnie renderowane glify w formacie ekranu -
czesciowo; (3) sterowniki ekranu uzywajace sprzetu karty - to jest ta
 brakujaca czesc (blitter W32i).

### 5.72 TF536 ST Edition a blitter - analiza dokumentacji i polityka systemu

**Co mowi dokumentacja (zrodla na koncu):**

1. Lista kompatybilnosci TF536 (exxos): *"Recommended to disable caches by
   jumpering the CDIS jumper on the TF536"* oraz *"Standard TOS build disables
   the blitter when an 030 is present"*. TOS 1.04 jest niekompatybilny z TF536
   (osobne cache I/D na 030 + self-modifying code w handlerze F-Line).
2. Watek "TF536 in Mega ST / Error 33": uzytkownicy **wyjmuja blitter z plyty**;
   *"According to the PAK manual the blitter is slower than <33MHz 68030
   anyway. So it doesn't make much sense to keep it."* Patch blittera na Mega ST
   to modyfikacja plyty zwiazana z gniazdem 68000, wiec przy TF536 bez
   przedmiotu.
3. EmuTOS-devel: obecnosc blittera zmienia tylko wyniki wydajnosci (nie lamie
   zgodnosci).

**Kiedy blitter moze sie zepsuc na TF536:**

- jest **bus masterem** na ST-RAM z stalym cyklem 16-bit; 030 na 32-50 MHz ma
  wlasny timing szyny - stad naruszenia arbitrazu (korupcja/zawieszenie),
- widzi **tylko ST-RAM** (16-bit, 24-bit adres) - TT-RAM (64 MB na TF536) jest
  dla niego nieosiagalny, wiec blit "przez" TT-RAM jest po prostu zly,
- **koherencja z cache 030**: DMA do ST-RAM plus cache danych = nieaktualne
  odczyty, jesli cache nie jest wylaczony/flushowany,
- sprzetowy patch blittera na Mega ST traci sens, gdy gniazdo 68000 zajmuje
  akcelerator.

**Co robi nasz system (i co naprawilismy):**

1. **Nowa polityka (jak TOS).** `kernel_main` wylacza blitter, gdy
   `cpu_type>=2` (68020/030/040). Zmierzone na 030 z obecnym blitterem:
   `BLITTER_PRESENT=1`, `BLITTER_ENABLED=0` (caps=0x0035) - czyli fallback CPU.
2. **Self-test funkcjonalny (byl).** `kblit_self_test()` sprawdza 12 przypadkow
   fill/copy (wyrownane, skos, nakladanie w obie strony, skrajne slowa); przy
   bledzie blitter jest wylaczany automatycznie.
3. **Bezpieczenstwo adresowe.** Powierzchnie kompozytora i framebuffer pulpitu
   sa alokowane w **ST-RAM** (`kmem_alloc(...,prefer_tt=0)`), wiec nawet gdy
   blitter dziala, nie dotyka pamieci dla niego nieosiagalnej.
4. **Cache.** `kernel_entry.S` czysci cache I 020/030 i nie wlacza cache D, wiec
   zapisy DMA blittera nie trafiaja na nieaktualne odczyty z cache.

**Wniosek:** naprawa jest mozliwa i jest zrobiona dwuwarstwowo - polityka
(030 => blitter OFF, jak TOS) plus runtime'owy self-test i alokacje ST-RAM.
Jesli kiedys bedziemy chcieli akceleracji sprzetowej na 030, wlasciwa droga jest
**blitter karty** (W32i), a nie blitter ST.

Zrodla:
- https://www.exxosforum.co.uk/forum/viewtopic.php?f=94&t=3486 (lista kompatybilnosci TF536)
- https://www.exxosforum.co.uk/forum/viewtopic.php?t=8287 (TF536 in Mega ST / Error 33)
- https://sourceforge.net/p/emutos/mailman/emutos-devel/thread/5D052597.6787.39BEBD%40anodyne.xplornet.com/ (EmuTOS + TF534/TF536)

### 5.73 Desktop: powiekszanie okna wypelnia sie, justowanie z wielokropkiem

**1. Powiekszanie nie wypelnialo okna (naprawione).** `wm_resize()` buduje liste
przyciec (`wm_rebuild_clip`) **przed** tym, jak `wm_resize_client_to_frame()`
powietksza klienta. Kompozytor wycinal wiec okno po staremu i w nowym obszarze
malowal tlo pulpitu - okno 'wypelnialo sie' dopiero, gdy cos innego (przesuniecie)
przebudowalo przyciecia. Teraz `wm_resize_client_to_frame()` sam wola
`wm_rebuild_clip()`, a kernel dodaje uszkodzenie nowej ramki i wymusza kompozycje.
Pomiar na docelowej (dummy): nowy obszar od razu ma tresc okna.

**2. Justowanie i wielokropek.** `row_text()` w AC liczy szerokosc panelu w
znakach (`cols=(pw-8)/8`) i: (a) nazwa dluzsza niz pole jest skracana do
`XXX...`, (b) kolumna rozmiaru jest wyrownana do prawej krawedzi panelu,
(c) tag `<DIR>` idzie tuz za nazwa. Dzieki temu przy zwezaniu okna lista jest
czytelna zamiast ucinana na krawedzi.

**3. Skalowanie czcionek - potwierdzone, ze go NIE ma.** Siatka jest stala
8x16 (`mono_glyph`), a `kgfx_text` wybiera tylko tablice glifow; nigdzie nie ma
skalowania. Zmiana polega wylacznie na tym, ile znakow sie miesci.

**4. Miganie kursora terminala - zaprojektowane, do dokonczenia.** Atrybuty
komorki terminala: 0x100 = inverse, 0x200 = podkreslenie (`text_terminal.c:117`,
`term_render_mono_polled`). Kursor jest komorka z tym atrybutem wkładana przez
shell. Plan: `kernel_wm_console_present()` robi lokalna kopie siatki i gdy okno
terminala jest sfokusowane (`console_focused`), w fazie 'off' czysci atrybut
kursora; gdy okno nie jest na wierzchu - kursor staly. Faza z licznika klatek
(~0.5 s).

**5. Smieci na pulpicie/menu - do reprodukcji.** Nie udalo sie ich zlowic w
testach; potrzeba dokladnego przypadku (co bylo robione: ruch? zamkniecie okna?
zmiana rozmiaru?), bo od tego zalezy, czy brakuje uszkodzenia tla, czy paska
górnego (`kernel_wm_refresh_bar`).

Bramka **42/42 OK**.

### 5.74 Pasek zadan: zamkniecie listy sciera ikonki (reprodukcja + czesciowa naprawa)

**Reprodukcja (harness, 640x400).** Otwarcie listy zadan (klik w ikonke na pasku)
dziala; po zamknieciu listy **caly pasek** (badge sesji, ikona zadan, zegar)
jest starty do tla pulpitu. Zrzuty: `docs/screenshots/taskbar-open.png`
(pasek obecny) i `docs/screenshots/taskbar-erased.png` (pasek starty).

**Znalezione i naprawione:**

1. `task_draw()` przy zamknietej liscie robil wczesny `return` i **nie czyscil**
   jej prostokata - lista zostawala na framebufferze. Teraz zamkniecie dodaje
   uszkodzenie WM tego prostokata (+ video damage) i wymusza kompozycje.
2. `topstrip_damaged()` sprawdzal tylko pas `y<16`, a prostokat **otwartej**
   listy lezy nizej (`y>=16`) - kompozycja zamalowywala liste i nie odrysowywala
   jej. Teraz sprawdzany jest takze prostokat otwartej listy.
3. Pasek jest teraz odrysowywany **po kazdej** kompozycji (`task_draw()`
   + `applet_draw()`), a nie tylko gdy sam pas byl uszkodzony - koszt ~212 B
   skanlinii na klatke.

**Status: NIE POTWIERDZONE.** Po tych zmianach harness nadal pokazuje pasek
starty po zamknieciu listy, a zrzut jest **bajt w bajt identyczny** w trzech
kolejnych buildach - czyli nowa sciezka w petli klatek nie jest w tym scenariuszu
w ogole osiagana (najprawdopodobniej po zamknieciu listy **nie ma zadnej
kompozycji**, a pasek jest stapiany czyms innym - pozostaje kandydat: save-under
kursora i/lub sciezka `kvideo_flush`). Nastepny krok: licznik wywolan
`task_draw()` + licznik kompozycji odczytane po zamknieciu listy, zeby ustalic
kto startuje pasek bez kompozycji.

**Do sprawdzenia na sprzetowej docelowej:** czy objaw jest taki sam jak w
harnessie (tam emulacja jest wolniejsza i sciezka zdarzen moze sie roznic).
Bramka **42/42 OK**.

### 5.75 Pasek zadan - POMIAR (liczniki) i wniosek

Dodane liczniki: `kdbg_task_draws`, `kdbg_task_opens`, `kdbg_task_closes`, plus
odczyt `task_list_open/drawn`, `kdbg_wm_composes`, `kdbg_frames` i wskaznika
`wm_framebuffer` (sonda `atos2-tools/probe_dg.py`).

**Pomiar (harness 640x400, klawisz testowy klika ikonke na pasku):**

```
after F7    draws=3 opens=0 closes=0 list_open=0 drawn=0 composes=2
list open   draws=4 opens=1 closes=0 list_open=1 drawn=1 composes=2
            framebuffer strip white bits=1556  screen strip white px=1466
list closed draws=6 opens=1 closes=2 list_open=0 drawn=0 composes=3
            framebuffer strip white bits=1732  screen strip white px=1636
```

**Wniosek:** `task_draw()` **jest** wywolywany po zamknieciu (draws 4->6) i
kompozycja **tez** sie odbywa (composes 2->3), a jednak ikony znikaja. Bialych
pikseli paska **przybywa**, bo pasek ma biale tlo - starte czarne glify zwiekszaja
licznik. Czyli pasek jest rysowany i **po** tym stapiany. Jedyny kandydat, ktory
dziala po rysowaniu: **save-under kursora** - wskaznik stoi dokladnie na ikonie
zadan, a `kgfx_cursor_hide()`/`cursor_sync()` przywracaja piksele zapamietane
**przed** narysowaniem paska.

Proba przeniesienia rysowania paska na koniec klatki (po `cursor_sync`) dala
niespojny pomiar (`draws=0` przy `composes=1`), czyli trafila w niewlasciwa
sciezke - **cofnieta**. Stan obecny: pasek odrysowywany po kazdej kompozycji
(poprzednia wersja).

**Nastepny krok:** przeniesc `task_draw()` za `cursor_sync()` w tej samej
funkcji, ktora realnie rysuje pasek (znalezc ja po `kdbg_task_draws`), albo
wykluczyc obszar paska z save-under kursora (`kgfx_cursor_hide` nie powinien
przywracac pikseli w pasie y<16).

### 5.76 Pasek zadan - NAPRAWIONE (potwierdzone pomiarem i zrzutem)

**Przyczyna (zmierzona):** `task_draw()` byl wywolywany po zamknieciu listy
(`kdbg_task_draws` 5->8) i kompozycja sie odbywala (`kdbg_wm_composes` 2->3), a
mimo to ikony znikaly. Pasek jest rysowany i **po** rysowaniu stapiany przez
**save-under kursora**: wskaznik stoi dokladnie na ikonie zadan, a
`kgfx_cursor_hide()`/`cursor_sync()` przywracaja piksele zapamietane przed
narysowaniem paska.

**Poprawka (celowana, tania):** po `cursor_sync()` w petli klatek, **jesli
kursor rzeczywiscie nachodzi na pas paska** (`y<16` i `x>=screen_w-106`), pasek
jest odrysowywany, a wskaznik stawiany ponownie. W typowym przypadku (kursor
poza pasem) koszt = **zero**; przy kursorze na pasku to ~106x16 px.

**Pomiar (harness 640x400):**

```
               framebuffer strip white bits   screen strip white px
przed: open   1556                          1466
       closed 1732  (+176 = ikony starte)    1636  (+170)
po:    open   1556                          1466
       closed 1582  (+26)                   1486  (+20)  <- ikony zostaja
```

Bialych pikseli **przybywa** przy stracie (pasek ma biale tlo, czarne glify
znikaja), wiec brak przyrostu = ikony sa. Zrzut:
`docs/screenshots/taskbar-fixed.png` - badge sesji, ikona zadan i zegar `00:00`
wszystkie obecne po zamknieciu listy. Bramka **42/42 OK**.

**Uwaga metodyczna:** adresy symboli kernela przesuwaja sie przy kazdej
przebudowie, wiec pomiar trzeba robic po `m68k-elf-nm` z **aktualnego** builda -
stary pomysl 'niespojny pomiar' byl skutkiem czytania nieaktualnych adresow.

### 5.77 Kursor terminala - ustalenia

Miganie kursora **juz istnieje** i robi je shell: w `system/shell_module.S` sa
`shell_cursor_hide`, `shell_cursor_show`, `shell_cursor_update` oraz
`shell_cursor_tick` (podzialka czasu), a komorka kursora to atrybut `0x200`
(podkreslenie, rysowane w wierszu 14 - `text_terminal.c:118`). Zadanie
'miga tylko gdy okno na wierzchu' sprowadza sie wiec do zahaczenia tego ticku
o `kernel_wm_console_focused()` - bez pisania migania od zera i bez dodatkowej
pracy na klatke (gdy okno nie jest sfokusowane, kursor stoi ukryty).

### 5.78 Tryb 800x600x256 - krok 1: sterownik karty (zrobiony)

**Sterownik (`system/vofa_et4000.c`, `include/vofa_et4000.h`):**

- `VOFA_MODE_800_8` (indeks 3) i `VOFA_MODE_COUNT=4`.
- `crtc_800_8`: te same timingi co 800x600, ale rejestr CRTC 0x13 = **0x64**, bo
  w Chain-4 offset liczy sie w jednostkach 8-bajtowych (800/8 = 100).
- `seq8 = {1,1,0,0x0e}` (Chain-4 = liniowy bufor 8-bit), `gdc8` z 0x05 = 0x40
  (256-colour shift) i 0x06 = 0x05, `attr8` z 0x10 = 0x41 i 0x12 = 0x0F.
- `mode_table` += `{800,600,600,480000}` (480 000 B VRAM).
- `vofa_et4000_init()` i `vofa_et4000_set_mode()` programuja wlasciwy zestaw
  rejestrow w zaleznosci od trybu (mono <-> 8 bit), wiec przelaczanie na zywo
  zmienia **format piksela**, nie tylko timing; clear pasa ogonowego liczy pitch
  jako `width` (nie `width/8`) dla 8 bpp.

**Hatari nie wymaga zmian**: `VOFA_GetMode()` juz dekoduje Chain-4 jako 8 bpp
liniowo (unit 8 B, `bits=8`), a `conv_st.c` ma sciezke `cbpp==8` czytajaca DAC
karty (`VOFA_GetDAC`).

Bramka **42/42 OK**; tryb nie jest jeszcze uzywany, wiec mono desktop dziala
bez zmian.

### 5.79 Co jeszcze trzeba dla 8 bpp w kernelu (kolejne kroki)

1. `kvideo_set_mode()` musi znac format: dla 8 bpp pitch = `width` bajtow/linie
   (dzis `(width+7)/8`).
2. `KGFX_MAX_FRAME_BYTES` i `WM_SURFACE_MAX_BYTES`: 98 304 -> **480 000** B.
3. Powierzchnie do **TT-RAM** (`kmem_alloc(...,prefer_tt=1)`): 8 powierzchni po
   480 KB to ~3,8 MB, a ST-RAM ma 4 MB. Blitter i tak jest na 030 wylaczony.
4. `kvideo_flush()`: dla 8 bpp bufor jest chunky (bajty), ale kopiowanie
   slowami 16-bit daje ten sam strumien (pitch 800 jest parzysty), wiec
   wystarczy powiekszyc cien do 480 000 B i ustawic `mode_pitch=800`.
5. `kgfx_set_palette()`: limit 16 -> **256** wpisow DAC (8-bit).
6. **Rysowanie 8 bpp** (najwiekszy kawalek): `kgfx_fill_rect` uzywa dzis
   planarnego `emutos_vdi_planar_fill`, a `kgfx_text` planarnego `mono_glyph`;
   dla chunky trzeba sciezek bajtowych (fill, glif, blit kompozytora).
7. Dopiero **po** 6. mozna przelaczyc tryb domyslny na 800x600x256 - inaczej
   desktop nie narysuje sie przy starcie. Dlatego domyslny tryb zostaje mono.

**Uwaga o budzecie:** 1 MB VRAM karty pomiesci 480 KB, wiec przy 800x600x8
zmiesci sie **podwojne buforowanie** - to usunie miganie kursora, o ktorym byla
mowa wczesniej.

### 5.80 Paleta 256 wpisow + zakres prac dla 8 bpp

`kgfx_set_palette()` przyjmuje teraz indeksy **0..255**: wpisy 0..15 ida
dodatkowo do rejestrow Shiftera (jak dotad), a **kazdy** wpis 0..255 do RAMDAC
karty. W mono widoczne sa nadal tylko 0/1, wiec zachowanie pulpitu sie nie
zmienia; to przygotowanie pod tryb 256-kolorowy.

**Zakres prac dla 8 bpp (kolejnosc):**

1. `surface_t` / prymitywy `kgfx_*` potrzebuja sciezek **bajtowych**: `surface_fill`,
   `surface_pixel_set`, `kgfx_text` (zamiast planarnego `mono_glyph`), `kgfx_line`,
   `kgfx_copy_rect` oraz `emutos_vdi_planar_fill` (dither tla) i `theme_icon`.
2. `kernel_wm.c`: `glue_blit`/`wm_copy_rect` (blit kompozytora) + dither tla w
   `wm_recompose`.
3. `kvideo_set_mode()` z formatem (pitch = `width` dla 8 bpp, nie `(width+7)/8`).
4. Rozmiary: `KGFX_MAX_FRAME_BYTES` i powierzchnie -> 480 000 B, alokowane w
   **TT-RAM** i realokowane przy zmianie trybu (teraz sa raz, na najszerszy tryb
   mono - przy 8 bpp ten invariant nie wystarcza).
5. `kgfx_cursor_*` (kursor compositora) - zapis/odtworzenie bajtowe.
6. `ksurface_*` (uchwyty powierzchni aplikacji, GFX 9/10/11) - bajtowo.
7. **Na koniec**: pozycja `800x600x256` w SETTINGS + tryb domyslny przy starcie
   (mono 640x400/800x600/1024x768 zostaja na liscie i dzialaja jak dotad).

Bramka **42/42 OK**; mono desktop bez zmian.

### 5.81 8 bpp (800x600x256) - warstwa kernela: zaimplementowana, przelaczenie niezweryfikowane

**Zaimplementowane (bramka 42/42 OK):**

1. `surface_t` ma flage `chunky`; `surface_pixel_set/get` i `surface_fill` maja
   sciezki **bajtowe** (piksel = `y*pitch + x`).
2. Kursor: osobny bufor `cursor_chunky_background[16*16]`, zapis/odtworzenie
   bajtowe i XOR ksztaltu.
3. `kgfx_text` dla chunky idzie sciezka per-piksel (planarny `mono_glyph` jest
   wylaczony), `kgfx_fill_rect` pomija planarny blitter.
4. Tlo pulpitu: szachownica 50% w dwoch kolorach systemowych (ikony bitmapowe
   na razie pomijane - sa zasobami planarnymi).
5. Glebia w calym lancuchu: `kvideo_set_mode_format` (pitch = `width` dla 8 bpp),
   `kvideo_program_mode_format` (wybor wpisu `VOFA_MODE_800_8`),
   `kgfx_video_reconfigure_format` (ustawia `graphics.mode=8`, `line_bytes=800`).
6. `KGFX_MAX_FRAME_BYTES` 98304 -> **480000**; powierzchnie aplikacji licza
   rozmiar z aktywnego trybu i sa **realokowane** przy zmianie glebi (z
   przepisaniem wskaznikow w `wm.win[]`), duze bufory ida do **TT-RAM**.
7. Kernel: opcode **34** = 800x600x256 (broker: limit podniesiony na 34).
8. SETTINGS: czwarta pozycja `800x600x256` (i poprawiony limit strzalki w dol).
9. `tests/test_cursor_words.py` zaktualizowany o nowe pole `surface_t`.

**Czego NIE udalo sie potwierdzic:** przelaczenie trybu nie zadzialalo w
automatycznych probach - licznik `kdbg_mode_result` pozostal na 777, czyli
`kgfx_video_reconfigure_format()` **w ogole nie zostalo wywolane** ani z klawisza
testowego F3, ani z jednorazowego haczyka przy starcie desktopu. To wskazuje na
**sciezke wejscia/fokus** w automatyzacji (desktop nie dostaje klawisza), a nie
na kod trybu. Ostatnia proba dala dodatkowo czarny ekran, wiec haczyki testowe
cofniete - zostawiony czysty build z dzialajacym mono.

**Nastepny krok:** przelaczenie sprawdzic realna droga uzytkownika (menu ->
OPTIONS -> SETTINGS -> 800x600x256) albo naprawic fokus w automatyzacji, a potem
obejrzec ekran w kolorze. Mono 640x400/800x600/1024x768 zostaje na liscie i
dziala jak dotad.

**Uwaga:** diagnostyka `kdbg_mode_result`, `kdbg_task_draws/opens/closes` zostaje
w kodzie jako instrumentacja (znikomy koszt).

### 5.82 Sterowniki zewnetrzne: Nova (Idek/Joebar) dla VoFa/ET4000 - ustalenia

Sprawdzone zrodlo: **Silicon Heaven** (`silicon-heaven.com/atari/nova/`), dzial
Novy dla MegaST: `ST - MegaST / VoFa` - czyli dokladnie nasza karta (VoFa/ET4000).

**Kluczowa obserwacja: driver jest rozgaleziany po ukladzie DAC, nie po karcie.**

```
DAC                glebia    wariant
TR9C1710           8-bit     ET4000_T0
ADV476             8-bit     ET4000_T1
KDA0476            8-bit     ET4000_T3
CIC10487           15-bit    ET4000_T4
SC11486CN-66       15-bit    ET4000_T6
ATT20C491/W82C490/SC15025 24-bit  ET4000_T8
```

Pliki do pobrania dla naszej karty: `VoFa_ET4000T0_V301.zip` (8-bit),
`VoFa_ET4000T3_V301.zip` (8-bit), `VoFa_ET4000T8_V301.zip` (24-bit). Uwaga autora:
*"15/16/24 bit modes not fully supported yet"* - czyli nawet referencyjny driver
najlepiej obsluguje **8 bitow = 256 kolorow**.

**Co to znaczy dla nas:**

1. Hatari modeluje RAMDAC jako `vofa_dac[256*3]` po **6 bitow na dzial**
   (`VOFA_GetDAC`), czyli **8-bitowy DAC z 256 wpisami** - to klasa T0/T1/T3.
   Nasza sciezka palety (`0x3C8/0x3C9`, wartosci 6-bit z rozszerzeniem na 8-bit)
   jest wiec **wlasciwa** dla naszego celu.
2. Nasz kernel **nie wykrywa DAC** - zaklada sciezke VGA 8-bit. Dla karty z DAC
   24-bit (T8) programowanie palety jest inne, wiec warto dodac probe DAC i
   wybor sciezki (tak jak robi to Nova), zanim zadeklarujemy obsluge 15/24 bit.
3. Kolejnosc prac jest zgodna z referencja: **8 bpp (256 kolorow) pierwsze**,
   glebsze tryby pozniej.

**Inwentarz naszych sterownikow na obrazie IDE (`work/atos-v2.img`):**

- system: `BOOT.SYS KERNEL.SYS ACSI.SYS CORE.SYS VFS.SYS PROC.SYS TIMER.SYS
  EVENT.SYS INPUT.SYS SHELL.SYS INIT.APP GFX.APP GUI.APP`,
- sterownik grafiki: `SYSTEM\DRV\ET4000.SYS` (jedyny; kopia VOFA.SYS, wybierany
  przez BIOS po chipsecie),
- aplikacje: `SYSTEM\APP\*.APP` (DESKTOP, COMMAND, SYSINFO, SETUP, HW, ...).

Referencje:
- https://silicon-heaven.com/atari/nova/ST%20-%20MegaST/VoFa
- https://silicon-heaven.com/atari/nova/MegaSTE/ET4000/
- https://silicon-heaven.com/atari/nova/Tools (MENU.PRG, NOVA_COL, VMG per-DAC)

### 5.83 Sciezka RAMDAC: detekcja rodzin + domyslny CIC10487 (15-bit)

Zgodnie z ustaleniem z Nova/Idek (jeden driver na rodzine DAC) nasz sterownik
rozpoznaje rodzine RAMDAC i dobiera programowanie palety:

- `enum {VOFA_DAC_UNKNOWN, VOFA_DAC_8BIT, VOFA_DAC_15BIT, VOFA_DAC_24BIT}`,
- **`VOFA_DAC_DEFAULT = VOFA_DAC_15BIT`** (CIC10487 - taki uklad nosza karty
  VoFa w praktyce),
- `vofa_et4000_dac_detect(io, override)`: sonduje rejestr **maski piksela
  `0x3C6`** (zapis 0x00 / 0xFF / 0xA5 i odczyt). Prawdziwy 8-bitowy DAC
  przechowuje wzorce (00->00, FF->FF) => `8-BIT`; gdy czesc ignoruje lub
  aliasuje zapis, wynik jest nierozstrzygajacy => **domyslnie 15-bit
  (CIC10487)**; `override != UNKNOWN` zawsze wygrywa,
- `dac_scale()` skaluje palete z 6 bitow na dzial (tak juz podaje kernel) do
  szerokosci czesci: **15-bit -> 5 bitow**, **24-bit -> 8 bitow**, 8-bit -> bez
  zmian,
- `vofa_et4000_set_palette()` uzywa `dac_scale()`, a `init()` ustawia maske
  (`0x0F` dla rodziny 15-bit, `0xFF` dla pozostalych),
- usluga video ma wersje **4** z `dac_type()` i `dac_name()`, wiec kernel
  raportuje w logu startowym linie `DAC: 15-BIT (CIC10487/SC11486)`.

**Test hostowy `tests/test_vofa_dac.py`** kompiluje prawdziwy sterownik z atrapa
rejestrow VGA (0x3C6 z trzema zachowaniami, 0x3C9 jako port danych R/G/B) i
sprawdza: detekcje 8-bit, domyslny 15-bit przy zapisie ignorowanym i przy
zachowanym tylko gornym nibblu, priorytet `override` oraz skalowanie palety dla
kazdej rodziny (0x3F -> 0x3F / 0x1F / 0xFF, 0x20 -> 0x20 / 0x10 / 0x82).

Bramka: **43 testy OK**.

### 5.84 Trwaly wybor trybu video (pierwszy start + SETTINGS) - ustalenia i stan

**Co juz istnieje w systemie:**

1. **`\BIOS\BIOS.CFG`** (`hw/config.c`, struktura `bios_config_t`) z polami
   `video_driver` (0 auto / 1 shifter / **2 et4000**), `video_mode`
   (0 low / 1 medium / 2 high / **3 = vga8** - rezerwa na 800x600x256) oraz
   `video_pending` (niepotwierdzony boot karty). Zapis przez `cfg_save()`, ktory
   jest juz wolany z menu ROM-u (`ui/setup.c`, `ui/boot.c`) i z `main.c`.
2. Menu setup ROM-u ma juz pole **VIDEO DEVICE** (AUTO / SHIFTER / NOVA VGA),
   ale **nie ma wyboru trybu**.
3. **`\SYSTEM\VIDEO.CFG`** - plik tekstowy czytany przez kernel w `kvideo_boot`
   (`VOFA [800|600|1024|768]`); jesli go nie ma, kernel nie programuje karty
   (robi to ROM).

**Co dodalem:**

- `requested_mode()` przyjmuje token **`256`** => `VOFA_MODE_800_8`,
- `kvideo_boot()` ustawia glebie (`kvideo_set_mode_format(...,8)`) dla tego
  trybu, a nowy **`kgfx_set_depth(bpp)`** adoptuje ja w warstwie grafiki przed
  pierwszym malowaniem (wolany z `kernel_main` po starcie karty),
- sciezka **ROM INIT** honoruje `video_mode==3` =>
  `kvideo_program_mode_format(800,600,8)`,
- `tools/make_bios_cfg.py` ma `--video-driver` i `--video-mode` (przydatne do
  testow i do ustawiania z zewnatrz).

**Test (BIOS.CFG z `video_mode=3`, harness + TT-RAM):** system wstal ze
**czarnym ekranem** i grafika zostala w 640x400 mono (`graphics w=640 h=400
mode=2`). Czyli **ROM nie rozumie wartosci 3** - handoff zatrzymuje sie przed
kernelem. Sam kernel ma juz obsluge (build + bramka **43 testy OK**).

**Wniosek dla zadania uzytkownika (pierwszy start + SETTINGS):**

1. **Pierwszy start**: miejsce na wybor to menu setup ROM-u (`ui/setup.c`) -
   trzeba dodac pole **VIDEO MODE** (w tym 800x600x256 gdy karta jest obecna) i
   zapisywac je przez `cfg_save()`; brak poprawnego configu = pokazac menu.
2. **SETTINGS (desktop)**: potrzebny **syscall zapisu** configu (albo pliku
   `VIDEO.CFG`) - aplikacje maja dzis tylko open/read/close/stat/move;
   dodac `SYS_VIDEO_SAVE` i wywolywac `cfg_save()` z jadra.
3. **Kernel**: gotowy - rozumie `video_mode==3` oraz token `256`.

Obraz przywrocony do normalnego BIOS.CFG (bez `video_mode=3`), zeby nie
zostawiac niesprawnego startu.

### 5.85 Pole VIDEO MODE w setupie ROM-u + co trzeba dla trybu tekstowego

**Zrobione:**

- `ui/setup.c`: nowe pole **VIDEO MODE** (cykl: `640X400 MONO` -> `800X600 MONO`
  -> `1024X768 MONO` -> `800X600X256`), widoczne tylko sensownie gdy NOVA jest
  obecna (`NOVA REQUIRED` inaczej); wiersze BLITTER/AUTO TOS/BOOT DEVICE
  przesuniete nizej, podpowiedz na wierszu 14.
- `include/config.h`: nazwane wartosci `CFG_MODE_640=2`, `CFG_MODE_800_8=3`
  (dawna rezerwa), `CFG_MODE_800=4`, `CFG_MODE_1024=5`.
- Zapis przez `cfg_save()` do `\BIOS\BIOS.CFG` (`video_mode`); gdy urzadzenie
  to SHIFTER, wymuszamy `CFG_MODE_640`.
- Kernel juz rozumie `video_mode==3` (sciezka ROM INIT) oraz token `256` w
  `\SYSTEM\VIDEO.CFG`.

**Znalezione twarde zalozenia (to jest robota dla trybu tekstowego w kazdej
rozdzielczosci):**

1. `hw/vofa_rom.c:54` - `vofa_et4000_init(&io,VOFA_MODE_640)` na sztywno; ROM
   musi programowac tryb z `g_config.video_mode`.
2. `include/hardware.h:57` - `SCREEN_BYTES = 32000` (640x400 mono) jako **stala**;
   `ui/console.c` uzywa jej do czyszczenia ekranu i przewijania
   (`console_clear`, `console_next_row`), wiec w 800x600 (60000 B), 1024x768
   (98304 B) i 800x600x256 (480000 B) tekst przewijalby sie zle. Potrzebny
   mode-aware rozmiar ramki.
3. `ui/console.c` `putc_core()` rysuje glif **planarnie mono**
   (`(cursor_x>>1)*shifter_group_bytes()`, dwa bajty na znak) - dla 8 bpp
   potrzebna sciezka **chunky** (8 bajtow na wiersz glifu, 1 bajt = 1 piksel).
4. Kernel: `term_render_mono_polled()` (`system/text_terminal.c`) tez pisze
   planarnie - dla sesji tekstowych w 8 bpp potrzebna wersja chunky.

**Kolejnosc:** (a) `vofa_rom_init` z trybem z configu, (b) mode-aware rozmiar
ramki w konsoli ROM-u, (c) chunky glif w konsoli ROM-u, (d) chunky renderer
sesji tekstowych w kernelu. Bramka **43 testy OK**.

### 5.86 Tryb z BIOS-u w trybie tekstowym - stan i pulapka pomiarowa

**Zrobione:**

1. `ui/setup.c`: pole **VIDEO MODE**, zapisywane do `\BIOS\BIOS.CFG`
   (`g_config.video_mode`) przez `cfg_save()`.
2. `hw/shifter.c`: geometria karty dla konsoli - `shifter_set_card_mode()`
   (line_bytes, frame_bytes, group_bytes, columns, cell_height) oraz
   `shifter_frame_bytes()`; `shifter_line_bytes/text_columns/group_bytes`
   zwracaja geometrie karty, gdy jest aktywna.
3. `ui/console.c`: czyszczenie ekranu i przewijanie uzywaja
   `shifter_frame_bytes()` zamiast stalej `SCREEN_BYTES = 32000` (640x400),
   wiec tekst nie lamie sie w 800x600 (60000 B) ani 1024x768 (98304 B).
4. `hw/vofa_rom.c`: `vofa_rom_init()` programuje tryb z `g_config.video_mode`
   (2 -> 640x400, 4 -> 800x600, 5 -> 1024x768) i ustawia geometrie konsoli.
   **8 bpp (3) celowo zostaje na 640x400** dopoki konsola nie umie rysowac
   chunky - inaczej tryb tekstowy bylby zepsuty, a tego wlasnie nie chcemy.

**Pulapka pomiarowa (wazna):** zrzut z Hatari ma **zawsze 640x436**, nawet po
przelaczeniu trybu karty - okno/ekran emulatora **nie podaza za trybem VOFA**
(w `conv_st.c` konwerter czyta `VOFA_GetMode`, ale rozmiar zrzutu sie nie
zmienia). Dlatego **rozmiar zrzutu nie jest dowodem** na tryb karty i wszystkie
wczesniejsze 'sprawdzenia' 800x600 nim nie byly. Weryfikacja musi isc przez
zmienne po stronie goscia (geometria ROM-u / kernela).

**Do zrobienia (reszta wymagania 'tekst w kazdej rozdzielczosci'):**

- chunky glif w `ui/console.c:putc_core()` (8 bpp: 8 bajtow na wiersz glifu),
- chunky renderer sesji tekstowych w kernelu (`term_render_mono_polled`),
- po tym mozna wlaczyc `CFG_MODE_800_8` w ROM-ie.

Bramka **43 testy OK**; obraz przywrocony do normalnego configu.

### 5.87 Tryb tekstowy w 8 bpp (konsola ROM-u + sesje) - zrobione, oraz
          KRYTYCZNE ustalenie o automatyzacji

**Zrobione:**

1. `hw/shifter.c`: geometria karty ma flage **chunky**
   (`shifter_set_card_mode(...,chunky)`, `shifter_chunky()`), a `frame_bytes`
   jest 32-bitowe (480000 B nie miesci sie w 16 bitach).
2. `ui/console.c`: **chunky glif** w `putc_core()` - w 8 bpp kazdy bit glifu to
   jeden **bajt** (1 = bialy, 0 = czarny), z obsluga inverse i podkreslenia
   (kursor).
3. `hw/vofa_rom.c`: ROM honoruje **wszystkie cztery** tryby z `BIOS.CFG`
   (640x400, 800x600, 1024x768, 800x600x256) i ustawia geometrie konsoli.
4. `system/text_terminal.c`: `term_render_chunky_polled()` - renderer sesji
   tekstowych w 8 bpp (1 bajt/piksel, dowolny pitch, te same atrybuty komorek).
5. `system/kernel_graphics.c`: `kgfx_console_render()` wybiera renderer po
   glebi; `system/kernel_wm.c`: powierzchnia konsoli w 8 bpp ma pitch i rozmiar
   framebuffera (w mono zostaje staly blok 640x400/80 B).

Bramka **43 testy OK**.

**KRYTYCZNE: automatyzacja nie testowala karty.** Harness (`atos2_auto.py`)
uruchamia **systemowe `hatari`** (v2.6.1) z `--machine ste --monitor mono` i
**bez** `--graphics-card vofa`. Dlatego:

- `kvideo_is_vofa()` bylo falszywe => opcode 30 zwracal `-0x3f3` **zanim**
  ustawil `kdbg_mode_result` (stad 'nigdy nie wolane' 777),
- desktop pokazywal `MONO 71 Hz`, a zrzuty zawsze 640x436,
- **wszystkie** wczesniejsze 'testy trybow' byly na SHIFTERZE, nie na karcie.

Po podaniu naszego emulatora (`--binary atari-030-port/.../hatari`) i argumentow
karty ROM nadal raportuje `GRAPHICS BACKEND: SHIFTER` oraz `TT-RAM: 0 KB`,
`CPU: 68000` - czyli konfiguracja docelowa (`--tf536 on --ttram 64 --ide-master`)
nie jest odtwarzana (harness uzywa `--acsi`). To jest do naprawy w sondach.

**Weryfikacja musi wiec isc z docelowej** (`run-bios-vga.sh`, karta + TF536):
w setupie ROM-u ustawic VIDEO MODE = `800X600X256` i sprawdzic, czy caly boot
(tekst) oraz desktop wstaja w kolorze.

### 5.88 800x600x256 na DOCELOWEJ: tekst dziala, kernel wisi (dowody + co dalej)

**Przelom - potwierdzone na docelowej (patched Hatari + karta + TF536):**

- `BIOS.CFG` z `video_mode=3` => zrzut ma **800x636** (czyli 800x600 + pasek
  Hatari), a **caly boot-log jest czytelny w tej rozdzielczosci** - konsola ROM-u
  rysuje chunky glify poprawnie. To domyka wymaganie 'tekst w wybranej
  rozdzielczosci'. Zrzut: `docs/screenshots/boot-800x600x256.png`.
- W setupie ROM-u pole **VIDEO MODE** jest widoczne i dziala (zrzut z menu:
  `VIDEO MODE  640X400 MONO`, bez dopisku NOVA REQUIRED => karta wykryta).

**Blocker:** kernel **wisi** po `MEMORY SELF-TEST: PASS` (log konczy sie w tym
miejscu), wiec desktop nie wstaje. Linia `GRAPHICS BACKEND: SHIFTER 640X400`
pochodzi z wczesnej inicjalizacji (przed podniesieniem video), wiec sama w sobie
nie jest bledem - ale kernel nie dochodzi do adoptowania trybu karty.

**Co podejrzewam (do sprawdzenia):** wczesna sciezka video jadra zaklada
640x400 mono (`kvideo_reset/kvideo_select` + cien 32000 B), a przy 8 bpp
`shadow_ensure()` potrzebuje 480000 B; jesli alokacja/geometria sie nie zgadza,
`kvideo_activate_vofa()` zwraca 0 i init sie zatrzymuje. Nastepny krok: znaczniki
logu wokol wczesnej inicjalizacji video (kvideo_boot / shadow_ensure / kgfx_init)
i odczyt, w ktorym miejscu sie zatrzymuje.

**Wazna lekcja o narzedziach:** harness (`atos2_auto.py`) chodzi na systemowym
Hatari BEZ karty (`--machine ste`, brak `--graphics-card`), wiec wszystkie
wczesniejsze 'testy trybow' byly na Shifterze. Testy trybow karty trzeba robic
na docelowej ( `run-bios-vga.sh` ), najlepiej zapisujac `video_mode` wprost do
`BIOS/BIOS.CFG` obrazu.

### 5.89 BACKLOG (uzytkownik): szybkosc przewijania tekstu

Zgloszone po sprawdzeniu na docelowej: *"prędkość przewijania tekstu jest
dramatycznie niska"*. Do poprawy **po** zakonczeniu pracy nad trybem
kolorowym. Prawdopodobne zrodla (do zmierzenia, nie zgadywac):

- `console_next_row()` w `ui/console.c` przesuwa cala ramke (`frame-row_bytes`
  bajtow) petla 32-bitowa - w 8 bpp to ~480 000 B na jedno przewiniecie,
- to samo w sesjach tekstowych: `term_scroll()` + pelny re-render,
- brak sprzetowego wspomagania (blitter na 030 wylaczony, a i tak nie siega
  VRAM karty).

Mozliwe kierunki: przewijanie przez **bufor cykliczny** (kursor wiersza w VRAM,
zawijanie adresu) zamiast przesuwania pamieci, albo przeniesienie konsoli do
cienia w ST/TT-RAM i przepisywanie tylko zmienionych wierszy na karte.

### 5.90 800x600x256: gdzie dokladnie stoi kernel (znaczniki) + wspolna przyczyna

**Znaczniki w logu (docelowa, `video_mode=3`):**

```
MEMORY SELF-TEST: PASS
CONSOLE SEED OK
VIDEO SHADOW: ON
HANDOFF COPIED
CONSOLE BOUND
EXCEPTIONS BOUND
ACSI LOADED        <-- i dalej nic
```

Czyli: ROM-owa konsola, inicjalizacja video, bindowanie przerwan i **zaladowanie
modulu ACSI** przechodza; zatrzymuje sie **pierwsze wyjscie konsoli jadra**
(`runtime_handoff.console_puts("ACSI DMA DRIVER...")` po podmianie na
`kconsole_puts`).

**Poprawka wprowadzona (sluszna, ale nie wystarcza):** kernel adoptuje teraz
geometrie karty **od razu** po `kvideo_activate_vofa()`
(`kvideo_set_mode_format(800,600,8)` + `kgfx_set_depth(800,600,8)`), zanim
konsola cokolwiek narysuje - wczesniej pracowal jako 640x400 mono nad
800x600x8. Sygnatura `kgfx_set_depth()` przyjmuje teraz pelna geometrie.

**Wspolna przyczyna z zgloszonym 'wolnym przewijaniem':** sciezka zapisu
konsoli jadra wykonuje zbyt duzo pracy na jeden znak (pelnoekranowy
scroll/flush; w 8 bpp ramka to 480 000 B). To samo dotyczy konsoli ROM-u
(`console_next_row()` przesuwa `frame-row_bytes`). Efekt: w 8 bpp boot
praktycznie staje, a w mono przewijanie jest 'dramatycznie wolne'.

**Nastepny krok (jeden temat, dwa objawy):**

1. zmierzyc, ile bajtow/cykli idzie na jeden znak w `kconsole_puts` ->
   `kconsole_flush` -> `kernel_wm_console_present` -> `kvideo_flush`
   (liczniki `kdbg_total_cycles` / `kdbg_recompose_cycles` juz sa),
2. przewijanie zrobic **bez przesuwania pamieci** (bufor cykliczny wierszy /
   offset), zamiast kopiowania `frame-row_bytes`,
3. dopiero potem wracamy do kolorow (geometria jest juz adoptowana poprawnie).

Uwaga: w kodzie zostaja **znaczniki startowe** (`CONSOLE SEED OK`, `VIDEO SHADOW`,
`HANDOFF COPIED`, `CONSOLE BOUND`, `EXCEPTIONS BOUND`, `ACSI LOADED`) - sluza do
tej diagnozy i usune je po naprawie.

### 5.91 Probne naprawy zwisu 8 bpp - cofnięte (nie dzialaly) + stan wiedzy

**Probowalem dwoch dobrze uzasadnionych rzeczy i obie cofnąłem**, bo zwis
zostal, a zmienialy sciezke startowa:

1. kernel **adoptuje geometrie karty od razu** po `kvideo_activate_vofa()`
   (`kvideo_set_mode_format(800,600,8)` + `kgfx_set_depth(800,600,8)`) - zwis
   bez zmian,
2. `kgfx_set_target(VOFA_MEM)` - konsola jadra rysuje do **VRAM karty**, nie do
   (niewidocznego, 32 KB) bufora Shiftera. To bylo o tyle ryzykowne, ze po
   adopcji geometrii kernel rysowal **chunky** glify do bufora 32 KB w ST-RAM
   (czyli ~320 000 B poza zakres) - potencjalna korupcja pamieci. Zwis bez zmian,
   wiec cofniete.

**Stan wiedzy (dowody ze znacznikow):** boot przechodzi ROM, inicjalizacje
konsoli jadra, `kvideo_activate_vofa`, kopie handoffu, bindowanie przerwan i
**zaladowanie modulu ACSI**; zatrzymuje sie na **pierwszym zapisie konsoli
jadra** (`kconsole_puts`). Nie wiem jeszcze dlaczego - dlatego nastepny krok to
**pomiar w tej sciezce**, a nie kolejne hipotezy:

- znacznik/licznik cykli miedzy etapami `kconsole_flush` ->
  `kernel_wm_console_present` / `kgfx_console_present` -> `kvideo_flush`,
- podejrzenia: przy pierwszym zapisie `kvideo_flush()` kopiuje **cala ramke**
  (480 000 B) i/lub `kgfx_console_present` renderuje pelna siatke 80x25
  (256 000 B) do VRAM karty - w obu przypadkach to nie 'zwis' a bardzo dlugi
  zapis, ktory przy 40 s wyglada jak stop.

**Backlog (Twoj):** szybkosc przewijania. Mechanizm potwierdzony w kodzie:
`console_next_row()` kopiuje `frame-row_bytes` (do ~467 200 B na wiersz w 8 bpp),
a konsola jadra robi to samo na swojej powierzchni. Plan: przewijanie **bez
przesuwania pamieci** - bufor cykliczny wierszy dla okna terminala, a dla
pelnoekranowej konsoli ROM-u **sprzetowy start adresu CRTC** karty (jedna
rejestracja zamiast kopiowania).

Bramka **43 testy OK**; obraz wrocil do dzialajacego trybu mono.

### 5.92 8 bpp boot: ustalony MECHANIZM (nie zwis) + uczciwy stan

**Pomiar rozstrzygajacy:** wstawilem pomiar czasu wokol pierwszego zapisu
konsoli jadra (`kconsole_puts`) i wypis przez dzialajaca konsola ROM-u:

```
MEMORY SELF-TEST: PASS
KCONSOLE CYCLES: 0        <-- zapis zajal ZERO cykli
```

Czyli konsola jadra **nie wisi** - ona **batchuje** (`kconsole_puts` zbiera do
320 znakow, dopiero wtedy `kconsole_flush()`), a w trybie karty jej wyjscie nie
trafia na widoczny ekran. **Log startowy nie 'staje' - on przestaje byc
wypisywany** (maszyna prawdopodobnie leci dalej).

**Znaleziony konkretny blad (naprawiony, ale bez potwierdzonego efektu):**
`kgfx_console_present()` - to jest **bootowy** renderer konsoli (sciezka WM
uzywa `kgfx_console_render`) - wolal **na sztywno** `term_render_mono_polled()`.
W trybie 256 kolorow rysowal wiec mono glify do **chunky VRAM karty**. Dodana
galez `term_render_chunky_polled(..., graphics.line_bytes, ...)`.

**Probne zmiany cofniete** (nie usunely objawu, a zmienialy dzialajaca sciezke):
adopcja geometrii karty w ROM-init i przekierowanie `kgfx_set_target(VOFA_MEM)`.

**Nastepny krok (jeden, konkretny):** zrzucic przez konsolę ROM-u stan, ktory
widzi `kconsole_flush()` w momencie pierwszego flushu: `active`, `dirty`,
`pending`, `graphics.planes`, `graphics.mode`, `kgfx_desktop_active()`,
`kvideo_scanout()`. To powie, ktory z wczesnych `return`-ow zjada wyjscie - i to
jest ostatnia rzecz miedzy nami a bootem w kolorze.

### 5.93 8 bpp boot: DIAGNOZA DOMKNIETA LICZBAMI (ostatni krok: 1 bajt)

Zrzut stanu, jaki widzi `kconsole_flush()` w trybie karty (docelowa,
`video_mode=3`):

```
KCONSOLE: active=1 dirty=0 pending=0 target=0 session=0 mode=2 planes=1
KCONSOLE: desktop_active=0 wm_enabled=0 wm_session=0 framebuffer=12582912 scanout=12582912
```

Rozbior:

- `scanout=12582912` = **0xC00000** -> karta skanuje swoj VRAM, tryb 800x600x256
  jest ustawiony przez ROM (potwierdzone wczesniej zrzutem 800x636),
- `framebuffer=12582912` -> po dodaniu `kgfx_set_target(VOFA_MEM)` konsola
  jadra **celuje w VRAM karty** (wczesniej `65536` = ST-RAM, czyli rysowala
  niewidocznie) - **to dziala**, zmierzone,
- `mode=2 planes=1` -> kernel **nadal mysli, ze jest w mono** - bo `h->video_mode`
  w handoffie niesie tryb **Shiftera** (2), a nie tryb **karty z BIOS.CFG** (3).

Dodatkowo naprawione i w kodzie: `kgfx_console_present()` (bootowy renderer
konsoli) ma teraz galez chunky (`term_render_chunky_polled` z pitch'em
framebuffera) - wczesniej rysowal mono glify w chunky VRAM karty.

**Ostatni krok:** przekazac tryb karty z ROM-u do kernela. W 128-bajtowym ABI
handoffu jest **wolny bajt `disk_reserved`** (nigdzie nieuzywany) - wystarczy go
zapelnic `g_config.video_mode` i uzyc w bloku adopcji zamiast `video_mode`.
Wtedy `mode=8`, pitch = 800 i boot-log w kolorze pojawi sie na ekranie.

Bramka **43 testy OK**; obraz przywrocony do dzialajacego mono.
Zrzut z diagnostyka: `docs/screenshots/colour-boot-diagnostic.png`.

### 5.94 800x600x256: DESKTOP DZIALA (domkniete) - co bylo naprawde zepsute

Trzy niezalezne bledy, kazdy znaleziony pomiarem (nie zgadywaniem):

1. **Tryb karty nie docieral do kernela.** Handoff niósł `video_mode` = tryb
   **Shiftera** (2), a nie tryb **karty z `BIOS.CFG`** (3). Zmierzone zrzutem
   stanu: `mode=2 planes=1 framebuffer=65536 scanout=12582912`. Naprawa: nowe
   pole **`handoff.card_video_mode`** (uzyty wolny bajt `disk_reserved` w
   128-bajtowym ABI), wypelniane w `loader/bootstage.c` z `g_config.video_mode`.
2. **Bootowy renderer konsoli byl tylko mono.** `kgfx_console_present()` wolal
   na sztywno `term_render_mono_polled()`. Naprawa: galez
   `term_render_chunky_polled(..., graphics.line_bytes, ...)`.
3. **`kgfx_framebuffer()` wyprowadzal glebie z `planes`** (`planes==1 -> 1 bpp`),
   a chunky **tez** ma `planes=1` - kompozytor dostawal bpp=1 przy pitch 800 i
   malowal mono wiersze w chunky linie (widoczny 'waski pas'). Naprawa: glebia
   z `graphics.mode`.

Dodatkowo w sciezce ROM-init kernel adoptuje geometrie karty
(`kvideo_set_mode_format(800,600,8)` + `kgfx_set_depth(800,600,8)`) i celuje
rysowanie w VRAM karty (`kgfx_set_target(VOFA_MEM)`) - zmierzone
`framebuffer=12582912`.

**Efekt (zrzut `docs/screenshots/desktop-800x600x256.png`):** desktop wstaje w
**800x600**: pelny pasek menu (FILE/OPTIONS), tlo pulpitu na calej szerokosci,
pasek zadan (COMMANDER/TERMINAL/SYSINFO/TRASH). Bramka **43 testy OK**.

**Co zostaje do prawdziwych 256 kolorow:**

- paleta: programujemy tylko wpisy 0/1, dlatego obraz jest czarno-bialo-szary;
  motyw/kolory systemowe (i kolorowe ikony) to kolejny krok,
- ikony bitmapowe sa **planarne** (`theme_icon`) i w chunky sa pomijane -
  trzeba je przerysowac w 8 bpp (albo narysowac wektorowo),
- Twoja uwaga o **szybkosci przewijania**: mechanizm znany
  (`console_next_row()` kopiuje `frame-row_bytes`), plan: bufor cykliczny /
  start adresu CRTC + **cache glifów w stylu NVDI** (pre-render po (znak,fg,bg)).

### 5.95 Start bez przewijania: "LOADING COMPONENTS: XXXXX" (pomysl uzytkownika)

Zaimplementowane dokladnie tak, jak opisane:

1. Po tabeli konfiguracyjnej (`MACHINE`/`CPU`/`ST-RAM`/`TT-RAM`/`BOOT DEVICE`,
   widocznej na zrzucie mono) konsola **przypina wiersz**
   (`kconsole_status_here()`),
2. **pierwszy** komunikat w tym wierszu to staly naglowek
   `LOADING COMPONENTS:` - zostaje na ekranie,
3. **kazdy kolejny** komunikat zapisuje w tym wierszu **tylko nazwe modulu**
   (czesc przed `": "` w istniejacych komunikatach `MODUL: STATUS`), kasujac
   samo pole - czyli `XXXXX` -> `YYYYY` bez ruszania naglowka i bez przewijania.

Dzieki temu: (a) **ekran sie nie przewija** przez cala faze modulow, (b) renderer
i flush dotykaja **jednego pasa wiersza** (~12 800 B w 8 bpp) zamiast calej ramki
(~467 200 B przy scrollu) - czyli okolo **36x mniej** ruchu na komunikat.

Uwaga: nie udalo sie zrobic zdjecia samej podmiany wartosci - w mono boot mija
to zbyt szybko, a w 8 bpp zbyt wolno, zeby trafic moment. Naglowek
`LOADING COMPONENTS:` jest widoczny na zrzutach 8 bpp; mechanizm przechodzi
bramke (43 testy) i jest w kodzie.

Naprawione tez: komunikat ROM-init wypisywal na sztywno `640x400 MONO`, teraz
podaje rzeczywisty tryb (`800x600 256 COLOURS`).

Domyslny tryb startu: **800x600x256** (`video_mode=3`); powrot do mono to jedna
pozycja w setupie ROM-u (VIDEO MODE -> 640X400 MONO).

### 5.96 Kompaktowy start bez przewijania (koncowa wersja wg uwag uzytkownika)

Ekran startowy po zmianach:

```
BOOT.SYS LOADED SUCCESSFULLY
MACHINE: MEGA ST / CPU: 68030 / ST-RAM / TT-RAM / BOOT DEVICE / BOOT PARTITION
LOADING SYSTEM\KERNEL.SYS...  (ROM)
ATARI SYSTEM KERNEL 0.03

CPU: 68030 (CLOCK UNKNOWN)   ST-RAM: 4096 KB   TT-RAM: 65536 KB
VIDEO: 800x600 256 COLOURS
LOADING COMPONENTS:
```

Mieszczi sie na jednym ekranie (~20 linii) => **zero przewijania**.

**Co doszlo:**

1. Tryb cichy konsoli (`kconsole_quiet()`): verbose linie jadra sa nadal
   zapisywane do lustra znakowego (dla ROM-u), ale **nie sa rysowane**;
   wyjatki widac (`* SELF-TEST FAILED`).
2. `kconsole_notice_puts/u32()` - linie, ktore maja zostac widoczne mimo ciszy
   (tabelka systemowa).
3. Usuniete verbose linie startowe (EXCEPTION VECTORS, MEMORY MANAGER READY,
   GRAPHICS BACKEND, SURFACE MANAGER, pule RAM, MEMORY SELF-TEST) - zostaly
   tylko sciezki bledow.
4. Tabelka: nazwa CPU (`68030`; zegar tylko gdy odczytywalny) + ST-RAM + TT-RAM
   + `VIDEO: 800x600 256 COLOURS`.
5. `LOADING COMPONENTS:` jest **od razu po bannerze**; kolejne komunikaty
   modulow podmieniaja tylko pole z nazwa modulu.

Zrzut: `docs/screenshots/boot-compact-800x600.png`. Bramka **43 testy OK**.

### 5.97 POMIAR startu + pierwsza realna optymalizacja rysowania (glify 8 bpp)

**Pomiar (docelowa, 800x600x256, metoda: zrzut + srednia jasnosc):**

```
start -> ATARI SYSTEM KERNEL (cala faza ROM + loader)   ~35 s
kernel: tabelka + LOADING COMPONENTS                    ~38 s
moduly (CORE/VFS/PROC/TIMER/EVENT/INPUT/SHELL)          >16 s
desktop (PRZED optymalizacja)                           ~72-75 s
```

**Kluczowe ustalenie:** mono startuje **tak samo dlugo** (~72 s) - czyli tryb
256 kolorow **nie jest** przyczyna wolnego startu. Wolny jest ROM+loader+moduly.

**Znaleziony i naprawiony realny koszt rysowania:** sciezka tekstu w 8 bpp
pisala kazdy wiersz glifu **bajt po bajcie do VRAM karty** - 8 zapisow na wiersz,
czyli **128 zapisow na znak** (w mono 2 zapisy). Przy takim log/hudzie to
dominowalo. Teraz: tabela pre-ekspandowanych wierszy glifow (`chunky_rows`) i
**dwa zapisy 32-bitowe na wiersz** (4x mniej operacji), w OBU konsolach -
kernela (`term_render_chunky_polled`) i ROM-u (`putc_ccore`).

**Zmierzony efekt:** desktop pojawia sie teraz miedzy **50 a 55 s** (bylo 72-75 s)
- okolo **20 s / 28% krocej** na samym rysowaniu tekstu. Bramka **43 testy OK**.

**Nastepne kandydaci (kolejnosc wg spodziewanego zysku):**

1. **Flush do VRAM 32-bit** w `kvideo_flush()` (dzis po jednym slowie 16-bit) -
   polowa transakcji na kazda pelna ramke (~480 000 B).
2. **Kursor**: `kgfx_cursor_show/hide` w 8 bpp zapisuje/odtwarza 16x16 **bajtow**
   do VRAM przy kazdym ruchu - zamienic na 32-bit (4x mniej).
3. **Cache glifow w stylu NVDI**: pre-render calego glifu (16x8) dla pary
   (fg,bg), potem tekst = czyste kopiowanie; do tego `movem.l`.
4. **Faza ROM+moduly (~50 s)**: potrzebny odczyt `TICKS SINCE POWER-ON`
   (znacznik dodany) zeby rozdzielic ROM / loader / moduly / self-testy.

Tryb domyslny: 800x600x256; powrot do mono = VIDEO MODE w setupie ROM-u.

### 5.98 Optymalizacja rysowania - wyniki pomiarow (przed/po)

| etap | czas do pulpitu (800x600x256) |
|---|---|
| przed optymalizacjami | ~72-75 s |
| po szybkich glifach (2x32-bit/wiersz) | ~50-55 s |
| po flushu 32-bit | **~45-50 s** |

Czyli **~25-30 s / ok. 35-40% krocej**, i to bez zmiany trybu.

**Co dokladnie zmienione:**

1. `term_render_chunky_polled()` (konsola jadra) i `putc_core()` (konsola ROM-u):
   tabela pre-ekspandowanych wierszy glifow (`chunky_rows[256][2]`) i **dwa
   zapisy 32-bit na wiersz** zamiast osmiodu bajtowych - 4x mniej operacji na
   znak (wczesniej 128 zapisow do VRAM na znak).
2. `kvideo_flush()`: kopiowanie po **dwa slowa na zapis 32-bit** (z wyrównaniem
   - poczatkowe/koncowe slowo 16-bit), czyli polowa transakcji CPU na kazda
   aktualizacje ekranu. Pelna ramka w 8 bpp to 240 000 slow.
3. `kconsole_flush()` po liniach `notice` (krotkie komunikaty nie byly
   flushowane, bo batch ma prog 320 znakow).

**Gdzie jeszcze siedzi czas (pomiar):**

- faza ROM + loader do momentu `ATARI SYSTEM KERNEL`: **~35 s**,
- faza dysk/moduły w kernelu (ACSI, blok, partycje, FAT, CORE/VFS/PROC/TIMER/
  EVENT/INPUT/SHELL): **kilkanascie s** - ekran wyglada wtedy na 'zamrozony',
  bo verbose linie sa celowo ukryte (Twoj quiet boot).

To sa koszty **dysku i self-testow**, nie rysowania - kolejny cel optymalizacji.

**Nastepni kandydaci:**

1. faza dysk/moduly: cache wpisow katalogowych FAT dla ladowanych modulow,
   skrocenie self-testow, ewentualnie ladowanie modulow bez posredniego kopiowania,
2. kursor 8 bpp (16x16 bajtow do VRAM na kazdy ruch) - 16/32-bit,
3. cache glifow NVDI-style (pelny glif 16x8 dla pary fg/bg) + `movem.l`,
4. compositor: `glue_blit`/`wm_copy_rect` w 8 bpp na 32-bit.

### 5.99 Optymalizacja cd.: cache blokow 128 -> 1024 sektorow (wynik: bez zmian)

**Co zrobione:** `CACHE_SETS 32 -> 256` (razem 1024 sektory = 512 KB, TT-RAM), zeby
ladowanie 127 KB KERNEL.SYS i kilkunastu modulow nie przeplywalo cache.
Test `test_block_cache.c` zakladal geometrie 32 zbiorow (kolizje sektorow
12/44/76/108/140) - przepisany na **zalezny od `kblock_cache_sectors()`**:
czytamy `total+1` roznych sektorow (wymuszona eksmisja) i sprawdzamy, ze
najnowszy trafia, a najstarszy nie. Bramka **43 testy OK**.

**Pomiar:** desktop nadal **45-50 s** - czyli **disk NIE byl waskim gardlem**
(block layer juz cachowal, a odczyty w emulacji sa tanie). Zmiane zostawiam
(nieszkodliwa, pomaga przy wiekszych plikach), ale uczciwie: **zysku nie ma**.

**Bilans dotychczasowych optymalizacji (czas do pulpitu, 800x600x256):**

| zmiana | czas |
|---|---|
| punkt wyjscia | ~72-75 s |
| glify 8 bpp: 2x32-bit/wiersz zamiast 8 bajtow | ~50-55 s |
| flush 32-bit (2 slowa na zapis) | ~45-50 s |
| cache blokow 1024 sektory | ~45-50 s (bez zmian) |

**Nastepny cel: faza ROM+loader (~35 s z pozostalych ~45-50 s).**
Podejrzani: `diag_ram_pattern()` (test wzorca na 4 MB ST + 64 MB TT-RAM) oraz
detekcja pamieci w ROM-ie. Trzeba dodac znaczniki tickow po stronie ROM-u
(w kernelu juz sa: `TICKS SINCE POWER-ON`), zeby zmierzyc, ile z tych 35 s
zjada POST, ile detekcja RAM, ile loader (odczyt BOOT.SYS), a ile inicjalizacja
karty - i dopiero wtedy optymalizowac.

### 5.100 Tlo pulpitu w 8 bpp: wypelnianie 32-bit (najwiekszy koszt pelnego malowania)

Znalezione: `kgfx_emutos_background()` w trybie chunky rysowalo szachownice
**piksel po pikselu** z testem modulo na kazdy piksel - przy pelnym malowaniu
pulpitu to **480 000 iteracji i 480 000 zapisow bajtowych**. W mono ten sam
obszar to 32 000 bajtow, wiec nic dziwnego, ze po przelaczeniu na 800x600x8
wszystko 'zwolnilo' - kazde pelne malowanie (start, zmiana rozmiaru okna,
odswiezenie) kosztowalo 15x wiecej pracy niz w mono.

**Naprawa:** wzorzec 2x2 jest okresowy, wiec wiersz to dwa powtarzajace sie
slowa 32-bit - teraz **4 piksele na zapis** (z wyrwnaniem: skrajne bajty
osobno), bez arytmetyki na piksel.

Uwaga metodyczna: ta zmiana **nie skraca startu** (tlo maluje sie raz), tylko
**interakcje** - kazde odswiezenie pulpitu. Dlatego w pomiarze 'czas do
pulpitu' nie widac roznicy, a odczuwalna plynnaosc powinna byc lepsza.

**Bilans optymalizacji 8 bpp (stan):**

- czas do pulpitu: **~45-55 s** (bylo 72-75 s),
- glify: 4x mniej zapisow na znak,
- flush: 2 slowa na zapis,
- tlo pulpitu: 4 piksele na zapis (bylo 1),
- cache blokow 1024 sektory (bez wplywu na start).

**Nastepne do zmierzenia/optymalizacji:** koszt interakcji (odswiezenie okna,
przesuwanie, przewijanie terminala) - trzeba zmierzyc na goscie (liczniki
`kdbg_recompose_cycles`/`kdbg_total_cycles` sa w WM), a potem: ikony w 8 bpp
(teraz pomijane), kursor 16/32-bit, `wm_copy_rect` z 32-bit (jest, ale bez
sprawdzania wyrownania zrodla).

### 5.101 Tlo pulpitu w kolorze (Win95 teal) + BUG: niez wyrownany odczyt long

**1. Tlo jak w Windows 95.** Wpis palety **2** to teraz morski `0x088`
(#008A8A) i tlo pulpitu jest wypelniane tym kolorem **na plasko** (bylo:
szachownica 0/1). Zeby kolor dotarl na ekran, paleta musi byc programowana
**po** podniesieniu karty: `kgfx_set_palette()` pisze do RAMDAC tylko gdy
`kvideo_is_vofa()`, a (a) w `kgfx_init()` karta nie jest jeszcze aktywna,
(b) usluga VOFA istnieje dopiero po `kvideo_module_attach()`. Dlatego
`kgfx_palette_defaults()` jest wolane w obu tych miejscach.
Zmierzony kolor tla: **RGB(0,138,138)** - zrzut
`docs/screenshots/desktop-teal-800x600x256.png`.

**2. BUG znaleziony przy okazji (najpewniej 'desktop/mysz nie dziala'):**
`wm_blit.c:wm_copy_rect()` w galezi chunky kopiowal wiersz 32-bitowo,
sprawdzajac wyrownanie **tylko celu** (`(uintptr_t)dp&3`), a nie zrodla.
68030 **wywala sie** (address error) przy niez wyrownanym dostepie `long`, wiec
kazde malowanie okna wywolane mysza moglo zabijac desktop. Teraz warunek
sprawdza **oba** wskazniki - reszta idzie bajtowo. Bramka **43 testy OK**.

To wyjasnia objaw 'na pewno mysz': klikniecie powodowalo odswiezenie okna,
a to niez wyrownany odczyt. Prosba: sprawdzic mysz (klikniecie w pasku zadan,
otwarcie okna, przesuwanie).

### 5.102 Boot bez przewijania: PROBING / status loadera / ramka POST

**1. PROBING ACSI/IDE w jednej linii** (`loader/bootstage.c`): zamiast
`\nPROBING IDE UNIT n` przy kazdym kandydacie, linia jest pisana na staly wiersz
`PROBE_ROW` (21) i **czyszczona** przed kazdym zapisem - naglowek zostaje, zmienia
sie tylko numer. Zweryfikowane na zrzucie: `PROBING IDE UNIT 1` w jednej linii.

**2. Status loadera bez przewijania**: `SEARCHING KERNEL.SYS...`, `KERNEL.SYS
LOAD/ENTRY/SIZE`, `STATUS: OK`, `STARTING ...` to teraz **jeden wiersz**
`STATUS_ROW` (22), kasowany i pisany od nowa. To samo dla `PARTITION ...
SEARCHING SYSTEM/BOOT.SYS...` i `STARTING BOOT.SYS...`. Zweryfikowane:
`KERNEL.SYS READY - STARTING...`.

**3. LOADING COMPONENTS** (konsola jadra, wczesniej): naglowek `LOADING
COMPONENTS:` zostaje, a pole wartosci pokazuje **nazwe modulu** i jest kasowane
przy kazdym kolejnym (XXXXX -> YYYYY).

**4. Ramka POST-u dopasowana do ekranu** (`ui/post.c`): `table_geometry()`
liczy pozycje z `shifter_text_columns()` - 80 kolumn w mono, **100** w 800x600,
**128** w 1024x768 (z ograniczeniem do 128). Wczesniej ramka byla sztywno
80-kolumnowa i na szerokim ekranie wygladala jak waski pasek.

Bramka **43 testy OK**; log startowy jest teraz kompaktowy i nie przewija ekranu.

Uwaga: ekranu POST nie udalo sie sfotografowac (w tym buildzie miga bardzo
krotko), wiec szerokosc ramki jest zweryfikowana tylko kompilacyjnie/logicznie -
do potwierdzenia wzrokowo przy zatrzymaniu auto-bootu (dowolny klawisz).
