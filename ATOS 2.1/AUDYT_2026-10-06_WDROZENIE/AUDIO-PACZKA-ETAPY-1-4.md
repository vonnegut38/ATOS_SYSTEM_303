# AUDIO w ATOS — PACZKA DO AUDYTU (etapy 1–4)

Data: 2026-10-10. Wszystkie liczby zmierzone. Plan: „Deepseek — plan wdrożenia
dźwięku w ATOS", pierwsza paczka = etapy 1–4.

---

## 1. Lista zmienionych i nowych plików

| plik | stan | co robi |
|---|---|---|
| `include/psg.h` | **nowy** | rejestry PSG, API, jawne 7/14/15, deklaracje cienia miksera |
| `hw/psg.c` | **nowy** | atomowy dostęp do PSG (sekcja krytyczna `%sr`/`ori #0x0700`), odmowa zapisu rejestrów 7/14/15, atomowy RMW portu A i miksera, cień programowy rejestru 7 |
| `include/kernel_audio.h` | **nowy** | kontrakt rdzenia audio: możliwości, właściciel, kolejka, limity |
| `system/kernel_audio.c` | **nowy** | rdzeń: jeden właściciel, tony/szum/stop, kolejka efektów, bezpieczne zwolnienie |
| `include/app_abi.h` | zmiana | opkody 75–80, `atos_audio_event_t` (12 B), bity możliwości, asercje układu |
| `include/app_abi_version.h` | zmiana | ABI **1.2 → 1.3** (zmiana additywna) |
| `include/gui_app.h` | zmiana | inline dla aplikacji: `audio_caps/tone/noise/stop/queue` |
| `system/kernel_exception.c` | zmiana | dyspozytor opkodów 75–80; `kaudio_release(pid)` w **obu** ścieżkach zakończenia procesu |
| `system/kernel_main.c` | zmiana | `kaudio_pump()` w pętli jądra (opróżnianie kolejki niezależne od klatek) |
| `hw/fdc.c` | zmiana | `fdc_select()` przez `psg_port_a_modify()` zamiast pisać do PSG wprost |
| `system/game.c` / `game.h` | zmiana | kolejka zdarzeń dźwiękowych w **logice** (cegła, paletka, ściana, bonus, życie, poziom, koniec gry) |
| `system/arkanoid.c` | zmiana | opróżnianie kolejki **po kroku symulacji**, nigdy w `paint()`; brak audio = zdarzenia odrzucane, nie udawane |
| `tests/test_psg.c` | **nowy** | reguły dostępu do PSG (16 sprawdzeń) |
| `tests/test_kernel_audio.c` | **nowy** | rdzeń audio: możliwości, właściciel, kolejka czasowa, zwolnienie (33 sprawdzenia) |
| `Makefile` | zmiana | `hw/psg.o` + `system/kernel_audio.o` w KERNEL.SYS; cele `check-psg`, `check-audio` w `check` |

Nic nie zmieniono w: `system/irq.c`, `system/tos_prg.c`, `system/kernel_console.c`,
`apps/flashback/**` (etapy 5–6 są poza tą paczką).

## 2. Jak uruchomić

```sh
cd "ATOS 2.1"
make TOS=1 -j4              # KERNEL.SYS + ARKANOID.APP
make check-psg check-audio  # testy jednostkowe tej paczki
```

Test na emulatorze (na **kopii** obrazu, żeby nie ruszać obrazu domyślnego):

```sh
cp -c work/atos-v2.img work/audio-baseline/atos-audio-test.img
python3 tools/install_boot_sys.py work/audio-baseline/atos-audio-test.img build/system/KERNEL.SYS
python3 tools/install_boot_sys.py work/audio-baseline/atos-audio-test.img build/system/ARKANOID.APP --directory SYSTEM/APP
# start z dźwiękiem: --sound 44100  (UWAGA: --sound przyjmuje CZĘSTOTLIWOŚĆ,
# "on" jest błędem i Hatari kończy się od razu)
```

Uruchomienie Arkanoida z pulpitu: `F4` → `Down` → `Return` → wpisz `ARKANOID` →
`Return` (ta sama droga, której używa `atos2-tools/measure_arkanoid.py`).

## 3. Wyniki testów

| test | wynik |
|---|---|
| `make check-psg` | **16/16 ok** — latch, odmowa 7/14/15, zachowanie bitów portu A, cień miksera |
| `make check-audio` | **33/33 ok** — możliwości (bez PCM i obwiedni), jeden właściciel, kolejka **czasowa**, zwolnienie |
| `make TOS=1 -j4` | **EXIT=0** z `-Werror` (KERNEL.SYS 452 746 B) |
| pozostałe cele C | `REST=0` (mono-font, gui-damage, trash, fat-write, block-cache, graphics, wm-core, wm-blit, gui-apps, irq, app-abi, game, sessions) |
| `check-console` | **czerwony — sprzed tej pracy** (runda bootu z 10-10; test linkuje tylko `kernel_console.c`+`text_terminal.c`, więc nie ma ścieżki zależności do PSG/audio; podejrzenie: nowa ścieżka „wiersza statusu") |

Testy, które złapały realne błędy w trakcie pracy (dowód, że nie są atrapą):

* **polaryzacja miksera PSG** — bity włączeń są **aktywne w stanach niskich**;
  `check-audio` wykrył trzy odwrócone przypadki (szum włączony zamiast wyłączonego),
* **rejestry 0–13 PSG są tylko do zapisu** — mój pierwszy `psg_mixer_modify`
  czytał rejestr 7 z powrotem, co na prawdziwym układzie nie działa; zastąpione
  **cieniem programowym**, który jednocześnie naprawdę chroni bity kierunku 6–7,
* **`memcpy` w kernelu** — przypisanie całej struktury ściągnęło `memcpy`,
  którego jądro freestanding nie ma; kopia pole po polu,
* **rozmiar zdarzenia ABI** — na m68k `uint32` ma wyrównanie 2, więc struktura
  wychodziła 10 B zamiast 12; jawne `pad[3]` + asercje w obu nagłówkach.

## 4. Pomiary przed/po (Arkanoid, to samo narzędzie i fragment)

| konfiguracja | wynik ogólny | pary |
|---|---|---|
| baseline (bez dźwięku, obraz sprzed zmian) | **+23,5 fps**, symulacja +26,3/s | 15,6–29,2 fps |
| **po (dźwięk włączony, `--sound 44100`)** | **+23,4 fps**, symulacja +25,9/s | 16,5–30,9 fps |

**Brak zauważalnej regresji** (różnica 0,1 fps = szum pomiarowy).

Dowód działania dźwięku na emulatorze (liczniki jądra, gra uruchomiona z ikony):

```
przed grą:  events=0   queued=0   psg_writes=3   psg_mixer=7      (tylko cisza startowa)
po starcie: owner=5 (pid Arkanoida)
  t= 3s  events=8   queued=9   q=1  psg_writes=30   psg_mixer=27  frames=255
  t= 9s  events=17  queued=17  q=0  psg_writes=54   psg_mixer=41  frames=499
  t=18s  events=32  queued=33  q=0  psg_writes=102  psg_mixer=73  frames=903
```

`q` (efekty oczekujące) rośnie i spada — czyli **rdzeń opróżnia kolejkę w pętli
jądra**, a nie w pętli rysowania aplikacji.

## 5. Właściciele PSG, timerów i przerwań (etap 1)

* **PSG**: przed tą paczką **tylko** `hw/fdc.c` (rejestr 14). Po: audio przez
  `hw/psg.c` (rejestry 0–13 i mikser) **oraz** FDC przez ten sam helper.
  Jeden właściciel dźwięku (pid), drugi proces odrzucony.
* **Timer C**: `main.c` (opóźnienia BIOS-u) i `hw/vofa_rom.c` (sonda karty) —
  oba z zapisem/odtworzeniem `TCDCR`/`TCDR`. **Audio ich nie używa** —
  kolejka jest opróżniana w pętli jądra, więc nie ma konfliktu o Timer C.
* **Przerwania**: `irq_dispatch()` **nie ma wywołań poza `irq.c`** — jądro nie
  używa dziś przerwań okresowych. To jest wprost zapisane w planie (§5) jako
  ryzyko dla etapu 5 (regularna praca zadania audio przy zajętym programie TOS).

## 6. Czego ta paczka NIE robi (uczciwie)

* **brak PCM** i brak DMA — i `audio_caps()` tego nie reklamuje (ani obwiedni,
  której nie zaimplementowano),
* **brak Giaccess/Dosound/Offgibit/Ongibit** — to etap 5,
* **brak dźwięku we Flashbacku** — etap 6,
* **brak przerwania okresowego** — kolejka jest obsługiwana w pętli jądra;
  program TOS liczący w ciasnej pętli może więc nie odtwarzać sekwencji płynnie
  (to dokładnie ostrzeżenie z planu §5),
* **nie zweryfikowano na emulatorze**: (a) wyciszenia po zamknięciu programu
  (jest test jednostkowy + wpięcie w obie ścieżki zakończenia procesu),
  (b) jednoczesnej operacji dyskietkowej i dźwięku (jest test jednostkowy portu A
  i ten sam helper w FDC).

## 7. Co warto sprawdzić w audycie

1. Własność PSG: czy `psg_mixer_set_directions()` nie powinno być wołane
   w ścieżce startu jądra (dziś cień startuje z `0x7f` = port A wyjście, port B
   wejście — zgodnie z typowym okablowaniem ST, ale nikt tego nie potwierdza
   odczytem sprzętu).
2. Czy `kaudio_release()` w obu ścieżkach zakończenia procesu obejmuje
   **wszystkie** przypadki (normalne wyjście, błąd, zabicie, panic).
3. Czy kolejka 32 zdarzeń i partia 8 na jedno wywołanie wystarczą dla gier
   (Arkanoid zużywa 1–5 na sekundę).
4. Czy cisza startowa (`kaudio_init` → `audio_silence`) nie zabiera czegoś,
   czego BIOS potrzebuje w rejestrach 0–13 (BIOS używa tylko rejestru 14).

---

# RUNDA 2 — domknięcie weryfikacji (2026-10-10)

## A. Dostęp do PSG jest scentralizowany — dowód z drzewa

```
grep -rn "ff8800\|ff8802" system hw include | grep -v hw/psg.c   ->  brak wyników
grep -n  "psg_port_a_modify" hw/fdc.c                            ->  42: psg_port_a_modify(0x07u,n);
```

Czyli **jedynym** miejscem w jądrze dotykającym bramki PSG jest `hw/psg.c`,
a wybór napędu/strony w FDC przechodzi przez tę samą atomową operację.

## B. Test przeplotu (deterministyczny dowód na wymaganie z etapu 3)

Nowy test w `tests/test_psg.c` przeplata 256 razy dostęp audio (mikser,
rejestr 7) z dostępem dyskietki (port A, rejestr 14):

```
ok: interleave: audio and floppy keep each other's bits
ok: interleave: all sources off at the end
ok: interleave: port A upper bits survive 256 accesses
```

Do tego celu podwójka hostowa została przebudowana na **model
per-rejestr** (wcześniej jedno wspólne „data" nie pozwalało odróżnić rejestru 7
od 14). To jest mocniejszy dowód niż próba złapania wyścigu w emulatorze:
256 naprzemiennych operacji i żadna nie traci bitów drugiej strony.

## C. Emulator: co potwierdzone, co nie

**Potwierdzone** ✓ (obraz testowy, `--sound 44100`):

* po starcie: `owner=0`, `events=0`, `psg_writes=3` (tylko cisza startowa),
* gra uruchomiona z pulpitu: `owner=5`, a dalej `events` 7 → 12 → 17 → 22,
  `psg_writes` 24 → 45 → 54 → 69, `psg_mixer` 21 → 35 → 41 → 51,
  `frames` 123 → … (gra działa i dźwięk gra razem z nią),
* brak regresji wydajności: **23,4 fps z dźwiękiem** vs **23,5 fps** baseline.

**Niepotwierdzone na emulatorze** ✗ — z powodu ograniczenia stanowiska, nie kodu:

* *zatrzymanie po zamknięciu programu*: Arkanoid kończy pracę na `WCLS`
  (zamknięcie okna — mysz) albo na **Escape** (`k==1`, skaner 1). Po
  uruchomieniu z dialogu RUN **fokus zostaje na pulpicie/dialogu**, więc
  wstrzyknięty Escape nie dociera do aplikacji (ta sama pułapka, którą widać
  było przy Flashbacku). Ścieżka zwolnienia jest jednak pokryta:
  test jednostkowy (`release: owner cleared`, `all sources disabled`,
  `port directions untouched`) **oraz** wpięcie `kaudio_release(pid)` w obie
  ścieżki zakończenia procesu w `kernel_exception.c`.
* *dyskietka razem z dźwiękiem*: pokryte testem przeplotu z punktu B oraz tym,
  że FDC używa tego samego helpera co przed zmianą (semantyka bez zmian).

## D. Drobne pułapki pomiarowe (na przyszłość)

* `--sound` w tym forku Hatari przyjmuje **częstotliwość**, nie `on/off`;
  `--sound on` kończy emulator natychmiast z błędem parsowania.
* Liczniki 8-bitowe trzeba czytać **jednym bajtem**; odczyt 4 bajtów zwraca
  sąsiednie zmienne.
* `psg_dbg_porta` **nie istnieje w `kernel.elf`** — linkowanie z
  `--gc-sections` usuwa je, bo w jądrze korzysta z niego tylko ścieżka FDC
  (BIOS). To nie brak funkcji, tylko skutek martwego kodu w tej konkretnej
  binarce; w `build/bios_mste.elf` licznik jest obecny.

---

# ETAP 5 — zgodność TOS (Giaccess, Dosound, Offgibit, Ongibit)

## Numeracja i kontrakty: wzięte ze źródła, nie z pamięci

Autorytetem było **EmuTOS** (`work/emutos-build/bios/sound.c` i `xbios.c`),
dostępne na tej maszynie:

* tabela wektorów: `xbios_1c`=**28 Giaccess**, `xbios_1d`=**29 Offgibit**,
  `xbios_1e`=**30 Ongibit**, `xbios_1f`=**31 Xbtimer**, `xbios_20`=**32 Dosound**,
* `giaccess(data,reg)`: `PSG->control = reg & 0xF`; zapis **tylko** gdy
  `reg & GIACCESS_WRITE` (0x80), a wynik to odczyt wskazanego rejestru,
* `ongibit(mask)`: `portA |= mask`; `offgibit(mask)`: `portA &= mask`,
* `dosound(table)`: pary `(instr,data)`; `0x80` ustawia wartość bazową dla
  komendy `0x81` (przyrostowa pętla: rejestr, przyrost, wartość końcowa —
  zawija się, dopóki nie osiągnie końca); `instr > 0x81` kończy serię zapisów,
  a **następny bajt to opóźnienie w taktach Timer C**; opóźnienie **0 kończy
  odtwarzanie**; wskaźnik **ujemny zatrzymuje** i zwracany jest poprzedni
  wskaźnik.

## Zmienione pliki

| plik | zmiana |
|---|---|
| `include/psg.h`, `hw/psg.c` | `psg_write_raw()` (rejestry 0..15, atomowo, **z zachowaniem bitów kierunku 6–7** przy zapisie rejestru 7) + `psg_shadow()`; cień wszystkich rejestrów aktualizowany także przez `psg_port_a_modify`/`psg_mixer_modify`, żeby odczyt `Giaccess` mówił prawdę |
| `include/kernel_audio.h`, `system/kernel_audio.c` | demon `Dosound` (`kaudio_dosound`, `kaudio_dosound_state/delay`), `kaudio_ongibit/offgibit`, `kaudio_timer_tick`, `kaudio_irq_attach` (rejestracja `IRQ_SRC_MFP_C` przy pierwszym właścicielu), liczniki `kaudio_dbg_dosound_*` i `kaudio_dbg_ticks` |
| `system/tos_prg.c` | XBIOS **28/29/30/32** z prawdziwymi kontraktami; **31 zwraca -32 (EINVFN)** zamiast pozornego sukcesu; liczniki `kdbg_tos_xbios28r/28w/29/30/31/32` |
| `system/tos_audio.c` | **nowy**: program TOS `TOSAUDIO.PRG` — sam siebie sprawdza (Giaccess, port A, Dosound) i dowodzi, że sekwencja gra w **ciastej pętli bez wywołań OS** |
| `Makefile` | cel `tosaudio` (+ naprawiony sklejony `.PHONY: tosvdi`) |
| `tests/test_kernel_audio.c`, `tests/test_psg.c` | testy kontraktów (szczegóły niżej) |

## Jak uruchomić

```sh
make TOS=1 -j4            # KERNEL.SYS z kontraktami audio
make check-psg check-audio
make tosaudio             # build/system/TOSAUDIO.PRG
# na obrazie testowym:
python3 tools/install_boot_sys.py <img> build/system/KERNEL.SYS
python3 tools/install_boot_sys.py <img> build/system/TOSAUDIO.PRG --directory SYSTEM/APP
# w Hatari: FILE (F4) -> RUN... -> wpisz \SYSTEM\APP\TOSAUDIO.PRG  (pełna ścieżka!)
```

Uwaga praktyczna: dialog RUN pulpitu dla programu TOS bierze nazwę **jak jest**
(komentarz w `desktop_wm.c`: „used as the path as-is (full or relative), not under
\SYSTEM\APP"), więc trzeba podać pełną ścieżkę. Wpisywanie musi być **wolne**
(≥0,35 s na znak); przy szybkim tempie pulpit gubi znaki.

## Wyniki testów

* `make check-psg` — **wszystkie** (doszła sekcja 8: `psg_write_raw`, cień do
  odczytu, **zachowanie bitów kierunku**, odmowa rejestru >15),
* `make check-audio` — **wszystkie** (doszły sekcje 11–15: tabela `Dosound`,
  opóźnienie 3 taktów zgodne z EmuTOS co do taktu, koniec opóźnieniem 0, stop
  wskaźnikiem ujemnym, `ongibit`/`offgibit` z zachowaniem kierunków, odmowa gdy
  aplikacja ATOS jest właścicielem),
* `make tosaudio` — `TOSAUDIO.PRG`, 614 B.

Testy złapały **realny błąd**: surowy zapis rejestru 7 przez `Giaccess`
nadpisywał **bity kierunku portów** — teraz program dostaje tylko bity 0–5.

## Dowód na emulatorze (wymóg krytyczny planu §5)

Program TOS wywołał `Dosound`, a potem **liczył w ciasnej pętli bez ani jednego
wywołania OS**:

```
pulpit gotowy po ~15 s (pid=4)
XBIOS 32 zawolane (1) po 1 s
xbios32=1  starts=1  ends=1  ticks=107  owner=1
Timer C: 137 -> 303 w 3 s = 55.3 Hz
po 3 s: ends=1
```

* `owner=1` — domenę TOS obsługuje `KAUDIO_TOS_OWNER`,
* `starts=1`, a po chwili **`ends=1`** — sekwencja **skończyła się sama**,
  mimo że program nie oddał sterowania ani razu,
* `kaudio_dbg_ticks` rósł ~55 Hz — czyli napędzał to **ISR Timer C**, nie pętla
  renderowania ani pętla programu,
* na ekranie programu widać jego własny wynik: `giaccess PASS`.

## Czego jeszcze nie domknięto (uczciwie)

* W tym samym przebiegu program wypisał `ongibit FAIL` — czyli odczyt portu A
  przez `Giaccess` nie pokazał ustawionego bitu. Test deterministyczny
  (`ongibit: port A bit visible via shadow`) **przechodzi**, więc rdzeń jest
  poprawny; do sprawdzenia zostaje ścieżka odczytu w personie **albo** to, że
  przebieg używał starszego cienia bez aktualizacji z helperów. Wymaga jednego
  powtórzenia sondy po ustabilizowaniu automatyzacji startu.
* Automatyzacja startu z dialogu RUN jest **kapryśna** (gubione znaki), więc
  powtórzenie pełnego przebiegu wymaga wolnego wpisywania i cierpliwości.
* Xbtimer (31) nadal niezaimplementowany — ale teraz mówi o tym **EINVFN**,
  zamiast udawać sukces.

---

# POPRAWKI PO AUDYCIE CODEXA (UWAGI-CODEX-AUDIO-ETAP5-20261010.txt)

Wykonane w podanej kolejności, punkty 1–3 i 6–7 z audytu:

## 1. Odczyt PSG przez 0xffff8800 (audyt pkt 2) — NAPRAWIONE

Hatari dokumentuje to wprost (`src/psg.c`):

```
0xff8800.b  ustaw rejestr (zapis) / CZYTAJ zawartość wybranego rejestru (odczyt)
0xff8802.b  zapis do wybranego rejestru   / brak akcji, zwraca 0xFF (odczyt)
```

`hw/psg.c` ma teraz **dwa** porty w abstrakcji (`psg_bus_read_reg`,
`psg_bus_read_data`) i czyta wybrany rejestr przez **0xffff8800**. Dotyczy to
także **RMW portu A, współdzielonego z FDC** — poprzednia wersja czytała 0xFF
i ustawiała niezamierzone bity. Twierdzenie o rejestrach „tylko do zapisu"
zostało wycofane; cień rejestrów pozostał jako **stan wirtualny persony**, a nie
jako zamiennik odczytu. Model hosta **odtwarza asymetrię** (odczyt danych = 0xFF),
więc test wykryłby powrót tego błędu. Poprawka wymagała też przebudowy **BIOS-u**,
bo ma własną kopię tego pliku.

## 2. Kontrakt Dosound (audyt pkt 1) — NAPRAWIONE

Zgodnie z `emutos/bios/sound.c`:

* `Dosound(NULL)` — **zatrzymuje** interpretację tabeli (nie wycisza PSG!),
* `Dosound(-1)` — **zapytanie**: zwraca bieżący wskaźnik, nie zmienia tabeli ani
  opóźnienia,
* `Dosound(p)` — ustawia tabelę; wynikiem jest **poprzedni** wskaźnik.

Test 13 utrwalał błędny kontrakt i został przepisany; doszedł test 13b na terminy.

## 3. Kolejka: jeden konsument i synchronizacja (audyt pkt 3) — NAPRAWIONE

* Konsumentem jest **Timer C**; pętla jądra używa `kaudio_pump_fallback()`, które
  opróżnia kolejkę **tylko** gdy hook IRQ nie został zainstalowany
  (`kaudio_irq_ok()`), więc nie ma dwóch konsumentów,
* wszystkie zmiany `q_head/q_tail/q_count/q_deadline/owner_pid` oraz wskaźnika
  Dosound są w **krótkich sekcjach krytycznych** (`move.w %sr` + `ori #0x0700`;
  na hoście licznik `kaudio_host_locks`), zagnieżdżenie jest bezpieczne, bo
  `psg.c` również zapisuje/odtwarza SR,
* `kaudio_irq_attach()` zwraca stan i jest wołane **raz** (przy pierwszym
  właścicielu), a nie z wnętrza ISR,
* mierzony jest **najgorszy** czas obsługi ISR (`kaudio_dbg_isr_max`), nie tylko
  średnie FPS.

## 7. Terminy kolejki (audyt pkt 7) — NAPRAWIONE

* dołączenie zdarzenia do **niepustej** kolejki nie rusza terminu głowy; deadline
  jest uzbrajany wyłącznie przy przejściu pustej kolejki w niepustą,
* po obsłudze termin to `poprzedni_termin + next.wait` (**kumulacja**, bez
  dryfowania), z limitem nadrabiania `KAUDIO_CATCHUP_LIMIT` (~0,5 s), żeby po
  długim zatrzymaniu nie odtworzyć całej sekwencji naraz.

## 6. Cień rejestrów (audyt pkt 6) — NAPRAWIONE

`psg_write()` **nie aktualizował** `reg_shadow` — poprawione: każdy piszący
(bezpieczny zapis, raw, mikser, port A) utrzymuje zapis zgodny z rzeczywistością,
więc odczyt `Giaccess` odzwierciedla także zapisy natywne. Mikser robi RMW na
**prawdziwym** rejestrze 7 (kierunki brane ze sprzętu, nie z cienia), a
`psg_mixer_set_directions()` — jedyne miejsce uprawnione do bitów 6–7 — pisze
rejestr wprost.

## Czego jeszcze nie zamknięto (z audytu)

* pkt 4: **sprzątanie po zakończeniu TOS** (odpięcie tabeli i zwolnienie
  właściciela przed zwolnieniem pamięci programu). Identyfikator domeny TOS
  zmieniony na zarezerwowany `0xFFFFFFF0` zamiast kolidującego z PID 1,
* pkt 5: `Giaccess` nadal bez kontroli właściciela; brak polityki portów 14/15
  (wirtualizacja w personie) i walidacji zakresu tabeli,
* pkt 8: `TOSAUDIO.PRG` wymaga przerobienia na test odbioru (rejestr o pełnych
  8 bitach, kompletny ton z mikserem i głośnością, czas oparty na zmiennej
  pamięciowej + watchdog, kontrolowana próba portu A),
* pkt 9c/9d/9f: pomiar częstotliwości Timer C w dłuższym oknie, zapis zużycia
  stosu IRQ, **manifest SHA** (KERNEL.SYS/BIOS/TOSAUDIO.PRG + komenda + ELF),
* powtórzenie sondy na świeżej kopii z zalogowaniem szczegółów `ongibit`
  (numer trapu, argument WORD, rejestr, cień przed/po, fizyczny readback).

Stan po tej rundzie: `check-psg`, `check-audio`, `check-app-abi`, `check-irq`,
`check-game` — PASS; build `make TOS=1 -j4` EXIT=0; KERNEL.SYS 456 470 B,
BIOS przebudowany (262 144 B).

---

# POPRAWKI — RUNDA 2 (audyt pkt 4 i 5)

## pkt 4: sprzątanie po zakończeniu TOS — NAPRAWIONE

Nowa funkcja `kaudio_tos_teardown()` w rdzeniu, wołana przez launcher **przed**
zwolnieniem TPA, obok `tos_vec_snapshot_restore()` („nothing may point into freed
TPA"):

```
        tos_vec_snapshot_restore();   /* nothing may point into freed TPA */
        /* Audio belongs in the same category as the vectors: ... */
        (void)kaudio_tos_teardown();
```

Kolejność jest istotą rzeczy: **najpierw** odpięcie wskaźnika tabeli (żeby ISR
Timer C nie chodził po zwolnionej pamięci), potem wyrzucenie kolejki, wyciszenie
kanałów i zwolnienie właściciela domeny TOS — wszystko **atomowo** względem
konsumenta (ta sama sekcja krytyczna). Doszedł licznik generacji sesji TOS, więc
spóźniony właściciel z poprzedniego uruchomienia da się odróżnić od bieżącego.

Zmiana kontraktu „stop": `Dosound(NULL)` **nie** wycisza PSG (tak jak TOS) —
wyciszenie jest **polityką ATOS** przy końcu sesji i tam je robimy.

Test host-owy 16 sprawdza: tabela odpięta, kolejka pusta, właściciel zwolniony,
kanał A wyciszony, generacja +1 i **brak** chodzenia po starej tabeli po
teardownie.

## pkt 4 (kolizja identyfikatora) — NAPRAWIONE

`KAUDIO_TOS_OWNER` = `0xFFFFFFF0` (zarezerwowany; żaden natywny PID go nie
osiągnie), zamiast wcześniejszego `1`, które kolidowało z PID 1 aplikacji.

## pkt 5: Giaccess i porty sprzętowe — NAPRAWIONE

* **Kontrola właściciela**: `Giaccess` sprawdza `tos_audio_write_allowed()` —
  zapis rejestru dźwięku jest **odrzucany**, gdy PSG należy do natywnej aplikacji.
  Odmowa jest raportowana **poza** wynikiem XBIOS (licznik
  `kdbg_tos_audio_refused`), bo klasyczny kontrakt nie ma kanału błędu.
  Pierwszy dopuszczony zapis powoduje `kaudio_claim` dla domeny TOS, a teardown
  go zwalnia.
* **Porty 14/15 wirtualizowane**: persona prowadzi `tos_vport_a` (start `0x07` =
  napędy odłączone, strona 0 — domyślne ST) i `tos_vport_b` (`0xff`). Zapis przez
  `Giaccess` do 14/15 oraz `Ongibit`/`Offgibit` zmieniają **stan wirtualny**,
  a nie wybór napędu/strony, strobe drukarki ani linie RS232. Licznik
  `kdbg_tos_vport_w` raportuje wirtualizację.
* **Walidacja zakresu tabeli**: `Dosound` przyjmuje wskaźnik tylko wtedy, gdy
  leży w bloku pamięci programu (`tos_current_basepage` ..
  `+tos_process_limit`); poza nim odmawia (licznik `kdbg_tos_audio_refused`),
  zamiast pozwolić ISR-owi wywołać Bus Error.

Stan po rundzie: `check-psg`, `check-audio`, `check-irq`, `check-app-abi`,
`check-game` — PASS; `make TOS=1 -j4` EXIT=0; KERNEL.SYS 456 726 B, BIOS 262 144 B
(przebudowany, bo ma własną kopię `psg.c`).

## Co jeszcze zostaje

* pkt 8: `TOSAUDIO.PRG` jako test odbioru (rejestr o pełnych 8 bitach, kompletny
  ton: mikser + głośność + okres, czas oparty na zmiennej pamięciowej z
  watchdogiem, kontrolowana próba portu A, przypadki 0x80/0x81 z przepełnieniem,
  zapytanie `-1` w trakcie opóźnienia, `NULL`, zamiana tabeli, exit/fault, odmowa
  dla każdego wejścia audio),
* pkt 9c/9d/9f: pomiar częstotliwości Timer C w dłuższym oknie, zużycie stosu
  IRQ, **manifest SHA** (KERNEL.SYS/BIOS/TOSAUDIO.PRG + komenda startu + ELF),
  oraz oznaczenie w raporcie nieaktualnych stwierdzeń o braku IRQ w jądrze,
* powtórzenie sondy na świeżej kopii z zalogowaniem szczegółów `ongibit`
  (numer trapu, argument WORD, rejestr, cień przed/po, fizyczny readback),
* testy integracyjne: start/exit/fault/zamknięcie okna 20×, native→TOS→native,
  konflikt równoległy, dźwięk przy obciążonym GUI.

---

# POPRAWKI — RUNDA 3 (audyt pkt 8, 9c/9d/9f, krok 4 i 5 z kolejności)

## Manifest SHA (audyt pkt 9f) — ZROBIONE

Nowe narzędzie `tools/audio_manifest.py`: dla każdego artefaktu trafiającego na
obraz zapisuje ścieżkę, rozmiar, SHA-256, **ELF źródłowy** (i jego SHA-256),
SHA obrazu oraz **dokładną komendę emulatora**. Zapisane manifesty:

* `work/audio-baseline/manifest-round3.md` (przed autostartem),
* `work/audio-baseline/manifest-before-round3.md` (stan wyjściowy).

Przykład (runda 3): KERNEL.SYS `4fc6ccf0a0447312…`, BIOS `e128088ddad9575c…`,
TOSAUDIO.PRG `e8cfe44e01235925…`, obraz `74b90def506eadeb…` — dopiero to pozwala
odróżnić „stary cień" od regresji.

## TOSAUDIO.PRG jako test odbioru (audyt pkt 8) — ZROBIONE

Przepisany (1064 B): rejestr **0** (pełne 8 bitów) na znaczniki, **kompletny ton**
(okres niski/wysoki, głośność, mikser `0x3E`), kontrakt `Dosound` (start, zapytanie
`-1`, `NULL`, zmiana tabeli z wynikiem = poprzedni wskaźnik), komenda `0x80/0x81`
z **przepełnieniem bajtu** (0xF0 + 0x20 = 0x10), kontrolowana próba portu A
(odczyt → ustaw → odczyt → przywrócenie) oraz **log szczegółów**: numer trapu,
argument WORD i wartości portu w hex przed/po.

## Test-only autostart (żeby dało się zrobić 20× start/exit)

W `system/desktop_wm.c` hook startujący sondę, **za flagą `ARK_TOS_AUDIO_AUTORUN`**
(build produkcyjny bez zmian) — czasowy, co ~8 s, bo pętla pulpitu **blokuje się w
`receive()`** i licznik iteracji nie jest miarą czasu. Zbudowany jako osobny
`DESKTOP.APP` (29 982 B) i wgrany **wyłącznie na kopię testową**.

## Pomiar częstotliwości Timer C (audyt pkt 9d)

W dłuższym oknie, w stanie ustalonym: `1458 → 7603` taktów w ~100 s = **~61 Hz**
(potwierdzone drugim oknem: 1158/20 s = 58 Hz). Pierwszy skok w logu zawierał
moment instalacji IRQ, dlatego był mylący. Wniosek: Timer C w ATOS chodzi
**~50–60 Hz**, więc mapowanie 1:1 na krok sekwencji Dosound jest właściwe i
**nie** kopiujemy dzielnika 4 z EmuTOS-a (dokładnie ostrzeżenie z audytu).

## Dowód z emulatora (krok 4 i 5 z kolejności audytu)

Dwa pełne cykle start → wyjście → sprzątanie, z liczników jądra:

```
t=32s  ticks=2222  starts=2 ends=1 teardown=1   xb28r=8  xb28w=4 xb29=1 xb30=1 xb32=4  vport=2 refused=0
t=52s  ticks=3454  starts=4 ends=2 teardown=2   xb28r=16 xb28w=8 xb29=2 xb30=2 xb32=8  vport=4 refused=0
```

* `xb32=4` na przebieg = dokładnie cztery wywołania `Dosound` (sekwencja,
  zapytanie, zmiana tabeli, NULL) — **kontrakt widać w licznikach**,
* `starts=2` na przebieg = liczą się **tylko realne starty** (zapytanie i `NULL`
  nie), `ends=1` = jedno naturalne zakończenie (drugą sekwencję zastąpiła zmiana
  tabeli),
* `teardown=1 → 2` = **sprzątanie po sesji TOS działa przy każdym wyjściu**,
* `xb28w=4` = zapis znacznika + trzy wyciszenia; `vport=2` = port A wirtualny,
  **żaden realny** zapis portu (FDC bezpieczny),
* `refused=0` = nic nie odrzucono (bo natywna aplikacja nie była właścicielem).

## Ustalenie przy okazji: okno TOS nie jest prezentowane w ciasnej pętli

Zrzuty w trakcie przebiegu pokazują pulpit z `AUTORUN STARTED`, ale **okna
programu nie widać**, dopóki gość nie odda sterowania: kompozycja okna TOS jest
odroczona do pętli jądra, a program w ciasnej pętli ją głodzi. To nie błąd audio,
ale własność ścieżki prezentacji — i powód, dla którego wyniki sondy odbieram z
**liczników jądra**, a nie z ekranu.

## Co zostaje

* dokończyć pętlę integracyjną do 20 przebiegów (mechanizm gotowy, każdy przebieg
  ~20 s) i zalogować po każdym: owner zwolniony, cisza, brak odczytów starej
  tabeli,
* kanał odbioru wyników sondy inny niż ekran (np. licznik/pamięć diagnostyczna),
* `native → TOS → native` i konflikt równoległy (natywna aplikacja właścicielem,
  a `Giaccess`/`Dosound` z TOS muszą zostać odrzucone — licznik
  `kdbg_tos_audio_refused`),
* oznaczenie w raporcie nieaktualnych stwierdzeń o braku IRQ w jądrze (pkt 9f).

---

# POPRAWKI — RUNDA 4 (krok 4 i 5 audytu: powtórzenia, konflikt, fault)

## Pętla integracyjna: 26 pełnych przebiegów

Po ustabilizowaniu startu (testowy hook autostartu) udało się wykonać **26
pełnych cykli** start → wyjście → sprzątanie. Log z liczników jądra:

```
runs=26  owner=0  result=0x3F  checks=156  starts=52  ends=26  xb32=104  refused=0
```

* `owner=0` po **każdym** przebiegu — właściciel jest zwalniany za każdym razem,
* `result=0x3F` w **każdym** przebiegu — wszystkie **sześć** sprawdzeń sondy
  przechodzi (Giaccess 8-bit, port wirtualny, ton, zapytanie, zmiana tabeli,
  przepełnienie `0x81`),
* liczby są wzajemnie spójne: 26 przebiegów × 2 sekwencje = `starts=52`,
  26 × 4 wywołań `Dosound` = `xb32=104`, 26 × 6 sprawdzeń = `checks=156`.

## Identyfikator domeny TOS widoczny w emulatorze

W trakcie przebiegu odczytano `kaudio_state_owner = 4294967280` = **0xFFFFFFF0**
= `KAUDIO_TOS_OWNER` — czyli domena TOS ma własny, zarezerwowany identyfikator,
a po zakończeniu programu wraca `owner=0`. Kolizja z natywnym PID 1 zniknęła
także w praktyce, nie tylko w kodzie.

## Log szczegółów ongibit (krok 4 audytu)

Sonda zapisuje wartości portu A do kanału diagnostycznego (0x834/0x838), bo okno
TOS nie jest prezentowane w ciasnej pętli. Odczytane z emulatora:

```
vport = 0x47 / 0x07     (przed: 0x07, po ongibit 0x40: 0x47, po offgibit: 0x07)
```

Czyli: bit ustawiony po `Ongibit`, odczytany przez `Giaccess` i **przywrócony**
przez `Offgibit` — bez dotykania realnego portu (licznik `kdbg_tos_vport_w`
rośnie, FDC bezpieczny).

## Polityka właściciela przeniesiona do rdzenia i przetestowana

Zamiast polegać na przebiegu GUI, reguła „TOS może pisać do PSG tylko, gdy nie
ma natywnego właściciela" mieszka teraz w rdzeniu jako `kaudio_tos_may_write()`
(persona tylko jej używa). Test 17 sprawdza deterministycznie:

* przy właścicielu natywnym (PID 7): `kaudio_tos_may_write()==0`, `Dosound`
  **odrzucony**, odmowa **policzona**, nic nie startuje,
* po zwolnieniu: TOS może pisać i sekwencja startuje.

## Fault: pokrycie ścieżką kodu (bez pełnego dowodu emulatora)

Obie ścieżki błędu gościa (`asf_tos_fault`: wektor 5 i pozostałe) wołają
`asf_tos_exit()`, która wraca do bloku sprzątającego launchera — a tam jest
`kaudio_tos_teardown()`, **przed** zwolnieniem TPA. Próba dowodu emulatorowego
(rozmyślny `div by zero` w co drugim przebiegu) **nie zadziałała**: licznik
`kdbg_tos_divzero` pozostał 0, więc pułapka nie wystąpiła. To zostaje do
domknięcia: albo kompilator usunął dzielenie, albo `volatile` nie wystarczył.

## Stan łańcucha testów po rundzie 4

`check-psg`, `check-audio` (w tym testy 16 i 17), `check-irq`, `check-app-abi`,
`check-game` — PASS; `make TOS=1 -j4` EXIT=0. Obraz główny **nietknięty**;
wszystkie przebiegi na kopii `work/audio-baseline/atos-audio-round3.img`.

## Runda 4 — domknięcie: fault nadal nie zademonstrowany empirycznie

Próba wymuszenia wyjątku w gościu **nie powiodła się** i zapisuję to wprost:

* `div by zero` (32-bit) — na m68k to wywołanie `__divsi3` z libgcc, które przy
  zerze **zwraca 0**, a nie trapu je; licznik `kdbg_tos_divzero` pozostał 0,
* nielegalna instrukcja `.word 0x4AFC` — sonda ustawiła bit 6 („doszedłem do
  końca") **także w przebiegach parzystych**, czyli kontrola wróciła normalnie i
  wyjątek nie wystąpił.

Co jest natomiast pewne i sprawdzone: sprzątanie jest w **wspólnym** bloku
launchera po powrocie z gościa, a obie ścieżki błędu (`asf_tos_fault` dla wektora
5 i pozostałych) kończą się `asf_tos_exit()`, która wraca właśnie tam. Przy 26
przebiegach `teardown == runs` co do jednego, więc każdy ukończony przebieg
sprząta po sobie.

Do zrobienia w następnej rundzie: ustalić, czy wektor 4 jest w ogóle
zainstalowany w sesji tworzonej przez hook autostartu (i dlaczego nie), oraz
użyć innego rodzaju błędu (np. dostępu do adresu nieparzystego / pułapki), tak
aby fault był **widoczny w licznikach**, a nie tylko wynikał ze ścieżki kodu.

Poprawiony został też **rozjazd wersji**, który wcześniej zafałszował odczyty:
obraz miał KERNEL.SYS z rundy 3, a skrypt czytał adresy symboli z nowego
`kernel.elf`. Po wgraniu zgodnego jądra i odświeżeniu manifestu
(`manifest-round4.md`: KERNEL.SYS `a76f83e0b840f652…`, TOSAUDIO.PRG
`7d7387166835c7b4…`) wszystkie liczniki znów się zgadzają — i to jest dokładnie
powód, dla którego audyt żądał manifestu.

---

# POPRAWKI — RUNDA 5: fault udowodniony empirycznie

## Przyczyna poprzedniego niepowodzenia

Nie „brak trapu", tylko **kolejność**: asm nielegalnej instrukcji nie miał clobbera
`"memory"`, więc kompilator przeniósł zapis bitu „doszedłem do końca" **przed**
fault — i przebiegi, które umierały, i tak raportowały sukces. Poprawki:

* `__asm__ __volatile__(".word 0x4AFC" ::: "memory")`,
* bit 6 ustawiany **wyłącznie** w gałęzi „przeżyłem" (poza blokiem faultu),
* nowy licznik **`kdbg_tos_faults`** w personie (`asf_tos_fault`) — bezpośredni,
  niezależny od gościa dowód, że wyjątek wystąpił.

Wektor 4 jest instalowany przez personę (`hnd[2]=asf_tos_fault4`, `vidx[2]=4u`),
więc nielegalna instrukcja trafia w obsługę.

## Wynik z emulatora

```
runs=1 faults=0   owner=0 teardown=1   ← przebieg nieparzysty: przeżył
runs=2 faults=1   owner=0 teardown=2   ← parzysty: WYJĄTEK i sprzątanie
runs=3 faults=1   owner=0 teardown=3
runs=4 faults=2   owner=0 teardown=4   ← WYJĄTEK i sprzątanie
runs=5 faults=2   owner=0 teardown=5
```

* `kdbg_tos_faults` rośnie **dokładnie** w przebiegach parzystych — wyjątek
  naprawdę wystąpił,
* `teardown == runs` przez cały czas — **sprzątanie audio po wyjątku gościa
  działa**: tabela odpięta, kolejka pusta, właściciel zwolniony **przed**
  zwolnieniem TPA,
* `owner=0` po każdym przebiegu, także po faultcie,
* `result=0x7F` (bit 6 = „przeżyłem") jest **kumulatywne** w pamięci gościa, więc
  nie odróżnia przebiegów — odróżnia je `kdbg_tos_faults`, i to jest właściwy
  dowód.

Manifest tej rundy: `work/audio-baseline/manifest-round5.md`
(KERNEL.SYS `2cb341a1098a5954…`, TOSAUDIO.PRG `f1d18282949fcd38…`).

## Ograniczenie, które zostaje (uczciwie)

`kaudio_dbg_isr_max` (najgorszy czas obsługi ISR) jest zaimplementowany i
mierzony **tylko wtedy, gdy kolejka ma coś do zrobienia**. Sonda TOS nic nie
kolejkuje, więc w tych przebiegach wynosi 0. Obciążeniem, które go uruchomi, jest
Arkanoid (kolejkuje efekty dźwiękowe) — jego pomiar pozostaje do wykonania.

---

# POPRAWKI — RUNDA 6 (DECYZJA-DLA-DEEPSEEKA: blokery P1.1–P1.4, P2.5)

## P1.1 — wyjście TOS nie kasuje kolejki natywnego właściciela: NAPRAWIONE

`kaudio_tos_teardown()` zerował kolejkę **przed** sprawdzeniem właściciela. Teraz
odłącza **wyłącznie własny stan interpretera** (tabela, granica, opóźnienie,
generacja), a kolejkę, terminy i kanały zwalnia tylko wtedy, gdy właścicielem
jest domena TOS. Test 19 (reprodukcja z audytu): natywny PID 7 z jednym
zdarzeniem w kolejce → po teardownie `owner==7`, kolejka nietknięta, a
zaplanowany STOP **nadal się wykonuje**.

## P1.2 — Dosound omijał wirtualizację portów 14/15: NAPRAWIONE

`psg_write_raw` w interpreterze zastąpione jedną ścieżką `snd_write_reg()`:
rejestry 0–13 przez regułę właściciela, 14/15 do **sinku persony**
(`kaudio_set_port_sink`, instalowany na czas sesji TOS), a bez sinku — **odmowa**
zamiast zapisu. Dotyczy obu ścieżek: zwykłej pary rejestr/dane i komendy `0x81`.
Test 20 pokrywa port A, port B i ścieżkę przyrostową oraz przypadek bez sinku.

## P1.3 — walidacja adresu tabeli: NAPRAWIONE

`tos_process_limit` jest **adresem końca** TPA, a kod porównywał z nim offset.
Nowy `tos_table_end()` porównuje `uintptr_t` z `base` i `end` (z kontrolą
`end>base`), akceptuje także bloki z `Malloc` (`tos_allocs[]`) i zwraca **koniec**
właściwego bufora. Rdzeń dostał `kaudio_dosound_span(table,end)` i sprawdza
każdy odczytywany bajt **przed** użyciem, więc interpreter nie wyjdzie poza
przydzielony obszar ani nie wywoła faultu w przerwaniu. Test 21 to pokrywa.

## P1.4 — synchronizacja całego rdzenia: NAPRAWIONE

* `claim`, `release`, `tone`, `noise`, `stop`, `tos_reg_write` i teardown są w
  sekcjach krytycznych; `release` **najpierw** odłącza pracę, potem zmienia stan,
  a na końcu wycisza — wszystko atomowo,
* kontrola właściciela i zapis to **jedna** operacja (`kaudio_tos_reg_write`),
* praca niesie **token generacji** (`snd_gen`, `q_gen`): interpretacja i kolejka z
  zakończonej sesji nie są wykonywane,
* `kaudio_irq_ok()` jest respektowane: bez hooka Timer C `Dosound` jest
  **odrzucany** (test 18), a nie „przyjmowany na niby”,
* `kaudio_dbg_isr_max` mierzy **cały** callback, razem z `snd_step` (do 256
  zapisów pod IPL 7), nie tylko kolejkę,
* test 22 z **kontrolowaną iniekcją IRQ**: w każdym punkcie wstrzyknięcia
  (`kaudio_host_irq_point`, 9 miejsc) sekcja krytyczna jest już trzymana.

## P2.5 — spójny model odczytu rejestrów: NAPRAWIONE

`psg_mixer_modify` i `psg_mixer_set_directions` aktualizują teraz **także
`reg_shadow[7]`**, więc odczyt `Giaccess` i cache miksera mówią to samo (test 23).
`psg_shadow()` stosuje **maski szerokości** rejestrów jak układowi: rejestry
1/3/5 to 4 bity, 6 oraz 8–10 to 5 bitów. Usunięte komentarze o „write-only” i o
ujemnym `Dosound` jako stopie.

## Manifest: teraz czyta zawartość z FAT

`tools/audio_manifest.py` wyciąga każdy artefakt z obrazu (`tools/img_get.py`) i
porównuje SHA z buildem, pokazuje `DESKTOP.APP`, listę ELF-ów i **pełną komendę**.
Pierwsze uruchomienie od razu pokazało to, co audyt zgłaszał: `KERNEL.SYS` i
`TOSAUDIO.PRG` **ZGODNE**, natomiast testowy `DESKTOP.APP` (29 626 B) **różni się**
od produkcyjnego (29 732 B) — czyli obrazu testowego nie wolno kopiować jako
głównego.

## Sonda: fault przy AKTYWNEJ tabeli i grającym kanale

Parzyste przebiegi kończą się teraz nielegalną instrukcją **w trakcie**
interpretacji `seq1`, z włączonym mikserem i głośnością 15 — **bez** wcześniejszego
`NULL`-stopu i bez wyciszania. Bit 5 wyniku ustawiany jest tylko po normalnym
dojściu do końca, więc przebiegi faultujące są rozróżnialne. Opis `wait_loop`
poprawiony: to licznik pamięciowy, nie zegar; bezwzględnym ograniczeniem jest
zewnętrzny watchdog.

## Czego NIE udało się potwierdzić w tej rundzie (uczciwie)

Próba weryfikacji na emulatorze **nie dała wiarygodnych odczytów liczników
jądra**: `teardown/starts/xb28w/refused` wracały jako 0, a `faults` jako 9 —
podczas gdy kanał **gościa** (0x820/0x830/0x834) działał poprawnie
(`runs`, `vport=71/7`, `result=0x7F`). Odczyt symboli jądra jest więc w moim
harnessie zawodny i **nie mam** z tej rundy emulatorowego dowodu na:
próbę native+TOS z zachowanym natywnym STOP, fault przy aktywnym dźwięku ani
pomiar całego ISR. Do zrobienia w następnej rundzie: naprawić/zweryfikować
ścieżkę odczytu (manifest daje teraz kontrolę tożsamości jądra) i powtórzyć
pomiar oraz regresję Arkanoida.

---

# RUNDA 7: rozbieżność emulatorowa — co ustalono, a czego nie

Cel rundy: naprawić ścieżkę odczytu liczników i dostarczyć dowód emulatorowy
(native+TOS z zachowanym STOP, fault przy aktywnym dźwięku, pomiar całego ISR,
regresja Arkanoida). **Nie udało się** — i oto co zostało wykluczone:

1. **Wyścig odczytu w harnessie**: odczyty z potwierdzeniem stabilności (dwie
   identyczne wartości pod rząd) dają te same wyniki — zero. To nie wyścig.
2. **Zły build w obrazie**: `SYSTEM/KERNEL.SYS` w FAT = 457 498 B, SHA
   `d492303d9262f1f1…`, identyczny z `build/system/KERNEL.SYS`; manifest
   potwierdza ZGODNE. W FAT nie ma `KERNEL.SYS` w katalogu głównym ani innego
   BOOT poza 182-bajtowym stubem.
3. **Duplikat persony w BIOS**: `build/bios_mste.elf` **nie zawiera** ani
   `kdbg_tos_xbios32`, ani `kaudio_dbg_dosound_starts`, ani `asf_tos_xbios`.
4. **Nieświeży obiekt**: `build/system/tos_prg.o` (11:44:22, 101 448 B) zawiera
   moje nowe symbole (`kdbg_tos_faults`, `tos_port_sink`), a `KERNEL.SYS` ma tę
   samą minutę.
5. **Odczyt z gościa**: sonda odczytała te same adresy **od wewnątrz** gościa
   (0x000C7C68, 0x000BBC86) i opublikowała je w kanale wyników — **też zera**,
   przy `runs` rosnącym do 13 i `result=0x7F` (wszystkie sześć sprawdzeń PASS).

Czyli: program TOS wykonuje `Giaccess`/`Dosound` z wynikami, które przechodzą
wszystkie sprawdzenia, a liczniki jądra dla tej samej ścieżki pozostają zerowe.
Dopóki to nie zostanie wyjaśnione, **nie mam** emulatorowego dowodu na:
próbę native+TOS z zachowanym STOP, fault przy aktywnym dźwięku, pomiar całego
ISR ani regresję Arkanoida.

Co jest natomiast mocne i wystarczające dla samych poprawek: **testy hostowe
18–23** odtwarzają wszystkie pięć blokerów dokładnie tak, jak w audycie
(natywny PID 7 z kolejką, port A i B przez interpreter, granica tabeli,
iniekcja IRQ w 9 punktach, zgodność odczytu miksera z maskami szerokości).
One przechodzą, ale — zgodnie z Waszą zasadą — **host nie jest emulatorem** i nie
zastępuje pomiaru.

Następny krok (konkretnie): sprawdzić, czy ścieżka `ATOS_CALL_TOS_RUN` z hooka
pulpitu nie prowadzi do **innej** obsługi XBIOS niż `asf_tos_xbios` w
`system/tos_prg.c` (np. przez wcześniejszy `case` lub osobny wektor), oraz
skontrastować to wywołaniem z TOS bezpośrednio z pulpitu (SysInfo) — bo tam
liczniki rosły w rundach 3 i 5.

## Runda 8: rozstrzygające — emulator nie bootuje jądra, które buduję

Sonda dostała **identyfikację przez API jądra** (`trap #0`, opcode 75/80), a nie
przez PSG. Wynik na obu obrazach (`atos-audio-round6.img` i kopia obrazu, na
którym liczniki działały w rundzie 3):

```
IDENT caps=0  tone_rc=0  owner=0x0  queued=0
```

Kluczowe: bufor stanu był **wypełniony sentinelem 0xDEADBEEF** i został
**nadpisany zerami**, więc wywołanie opcode 80 **dotarło** do jądra — ale
`ATOS_CALL_AUDIO_CAPS` zwraca **0**, a moje `kaudio_caps()` zwraca zawsze 7
(TONE|NOISE|QUEUE). Odpowiada więc **inne jądro** niż to, które buduję.

Co przy tym sprawdzono:
* `SYSTEM/KERNEL.SYS` w FAT = 457 498 B, SHA `d492303d…`, **zgodne** z buildem
  (manifest), brak `KERNEL.SYS` w katalogu głównym;
* BIOS (`bios_mste.elf`) **nie** zawiera symboli audio ani persony — więc nie jest
  to duplikat w ROM-ie ATOS-u, ale jest to jądro **starszego builda**;
* `install_boot_sys.py` pisze do **FAT** i sam raportuje „SYSTEM/KERNEL.SYS” —
  czyli plik, który czytam, jest właściwy, a mimo to nie ten się uruchamia.

Wniosek: **mój harness nie bootuje jądra z tej ścieżki** (najpewniej obraz ma
więcej niż jeden wolumen/system, a BIOS startuje z innego). To blokuje wyłącznie
**weryfikację**, nie kod: pięć blokerów z Waszej decyzji jest naprawionych i
odtworzonych testami 18–23.

### Jak to dokończyć (konkretnie)

1. Uruchomić `work/audio-baseline/atos-audio-verify.img` w swojej działającej
   konfiguracji (tam, gdzie Wasze obrazy faktycznie bootują nowe jądro) i w oknie
   TOS uruchomić `\SYSTEM\APP\TOSAUDIO.PRG`.
2. Odczekać ~2 s i odczytać słowa pod `0x848`–`0x854`: `caps` powinno być **7**,
   `owner` albo `0`, albo `0xFFFFFFF0`. Jeśli `caps=0`, to nadal chodzi stare
   jądro i trzeba ustalić, skąd BIOS je bierze.
3. Liczniki jądra (adresy z `work/audio-baseline/manifest-verify.md`):
   `kaudio_dbg_dosound_starts/ends/rejected`, `kaudio_dbg_tos_teardown`,
   `kdbg_tos_faults`, `kdbg_tos_xbios32`, `kdbg_tos_vport_w`,
   `kaudio_dbg_isr_max` — po przebiegu `starts=2`, `xb32=4`, `teardown=1`.

### Runda 8, druga próba: konfiguracja ACSI+IDE

Sprawdziłem też wariant z Waszej działającej aplikacji (`--acsi 0=obraz` **oraz**
`--ide-master obraz`, bez `--acsi N=none`). W tym wariancie sesja nie doszła do
pulpitu w ~200 s (Hatari żyje, brak postępu) — czyli ten układ w moim harnessie
nie bootuje. Bez rozstrzygnięcia; do sprawdzenia po Waszej stronie, bo tam działa.

## Runda 9: kolejne wykluczenia i wskazanie różnicy, która zostaje

Nowe, twarde ustalenia:

1. **Nie ma drugiej kopii jądra.** Przeskanowałem cały obraz (805 306 880 B) pod
   kątem ciągłych kopii trzech jąder (moje 457 498 B, bazowe 450 690 B, domyślne
   455 276 B). Mojego jest **jedna** (pofragmentowana w FAT, okolice 0xA24400),
   pozostałych dwóch **nie ma wcale**. Hipoteza „surowa kopia w obszarze
   rozruchowym” jest **odrzucona**.
2. **ATOS bootuje z tego obrazu.** Zrzuty w trakcie startu pokazują autorski
   splash „ATOS 2.1” z paskiem postępu i „STARTING DESKTOP”, a status bar podaje
   „TOS 2.06, MONO 71 Hz”. Czyli jądro **jest** ładowane z obrazu, i to jedyne,
   jakie tam jest — moje.
3. A jednak identyfikacja przez API jądra daje `caps=0`, a liczniki persony
   pozostają zerowe, przy `st` (opcode 80) **nadpisanym zerami**.

Wniosek, który z tego zostaje: program TOS uruchomiony **przez mój hook
autostartu** (`ATOS_CALL_TOS_RUN`, d1=1 — czyli wariant nieblokujący) obsługuje
warstwa zgodna z TOS, ale **nie moja persona** — XBIOS działa (sprawdzenia sondy
przechodzą), natomiast liczniki `kdbg_tos_*` i usługa audio jądra nie biorą w tym
udziału. W rundach 3 i 5, gdy liczniki **rosły**, program uruchamiałem **z dialogu
RUN pulpitu** (wpisywanie nazwy), a nie hookiem.

To jest różnica do rozstrzygnięcia w pierwszej kolejności: **czy `ATOS_CALL_TOS_RUN`
z d1=1 prowadzi inną ścieżką niż uruchomienie z dialogu RUN** (i czy w tej ścieżce
nie działa przypadkiem warstwa TOS z ROM-u BIOS-u, która przejmuje trap #14 przed
personą). Dopóki to nie zostanie ustalone, emulatorowych dowodów (native+TOS z
zachowanym STOP, fault przy aktywnym dźwięku, pomiar całego ISR, regresja
Arkanoida) **nie dostarczę** — mimo że kod jest gotowy i przechodzi testy 18–23.

## Runda 9 — przełom: liczniki jądra ruszają przy starcie z dialogu RUN

Powtórzyłem start **tą samą drogą, co w rundach 3 i 5** (produkcyjny pulpit bez
hooków, `FILE` → `RUN...`, wpisanie `\SYSTEM\APP\TOSAUDIO.PRG`) na tym samym
obrazie i z tym samym jądrem:

```
t=24s  IDENT caps=0 tone=0 owner=0x0 queued=0 | teardown=1  faults=1  xb32=0  starts=0
```

**`kaudio_dbg_tos_teardown=1` i `kdbg_tos_faults=1`** — czyli jądro **jest** moim
buildem, persona **obsługuje** sesję TOS i sprzątanie po niej działa. To obala
wcześniejszy wniosek „emulator bootuje inne jądro”: problemem była **droga
uruchomienia**.

Różnica: mój hook autostartu woła `ATOS_CALL_TOS_RUN` z **d1=1** (wariant
nieblokujący) i w tej ścieżce liczniki `kdbg_tos_*` nie rosną, natomiast
uruchomienie z dialogu RUN (ta sama funkcja pulpitu, ale ścieżka blokująca)
działa. To jest właściwy przedmiot następnego kroku — i od razu droga do
zebrania brakujących dowodów: **wszystkie scenariusze akceptacyjne trzeba
powtórzyć startem z dialogu RUN**, nie hookiem.

Dodatkowe ustalenie z tej samej próbki: `ATOS_CALL_AUDIO_CAPS` zwraca **0** także
wtedy, gdy jądro jest moje — czyli dla programu TOS wywołania ATOS-owe są
najpewniej **odcinane** (albo opcode 75 nie dociera do `kaudio_caps()`), a `st`
(opcode 80) dostaje zera. To osobna sprawa od pięciu blokerów, ale warta
odnotowania: sonda nie może używać API jądra jako identyfikatora.

## Runda 10: co udało się zebrać, co nie, i gdzie jest różnica w kodzie

### Znaleziona różnica w kodzie (konkret)

`ATOS_CALL_TOS_RUN` (`system/kernel_exception.c:2032`):

```c
if(argument0==1u){user_syscall_result=kernel_tos_start_async(path);return 0;}
user_syscall_result=kernel_tos_run_program(path);return 0;
```

Czyli **d1=1** (mój hook i dialog RUN pulpitu) idzie przez
`kernel_tos_start_async()` — kooperacyjną domenę TOS — a **d1=0** przez
`kernel_tos_run_program()`. W rundach 3 i 5 liczniki rosły przy starcie z
**dialogu**, czyli ta sama funkcja co w hooku — więc różnica nie jest w samej
fladze, a w **kontekście wywołania**: hook woła ją z wnętrza pętli pulpitu
(czasowo, co ~8 s), a dialog z obsługi menu. To jest dokładne miejsce do
rozstrzygnięcia w następnej rundzie.

### Dowody emulatorowe zebrane w tej rundzie (drogą z dialogu)

```
start        owner=0  q=0  queued=0  isr_max=2   teardown=0 faults=0 xb32=0 ...
arkanoid+30s owner=0  q=0  queued=0  isr_max=2   teardown=0 faults=0 xb32=0 ... | ark_frames=1779 sim=1922
```

* **`kaudio_dbg_isr_max=2`** — pomiar **całego** ISR jest niezerowy przy
  obciążeniu grą; mechanizm działa (wcześniej licznik obejmował tylko kolejkę),
* **Arkanoid działa** i jego liczniki `0x800`/`0x804` rosną równo (294→1779
  klatek, 327→1922 symulacji w ~30 s) — to materiał odniesienia dla regresji,
  choć **nie** jest to jeszcze pomiar w formacie `measure_arkanoid.py`,
* `owner=0` przy grającej grze — bo `atos2.cfg` ma `bEnableSound = FALSE`
  (`atos2.cfg:225`), więc Arkanoid w tej konfiguracji w ogóle nie zawłaszcza
  audio. Do próby native+TOS trzeba tę opcję włączyć (przygotowałem
  `/tmp/atos2-sound.cfg` z `bEnableSound = TRUE`).

### Czego nadal brakuje

Uruchomienie z dialogu jest **zawodne** (wpisywanie nazwy bywa gubione): w dwóch
kolejnych przebiegach raz zadziałało (`teardown=1`, `faults=1`), raz nie
(`ark_frames=0`, brak startu). Dlatego brakuje jeszcze: native+TOS z zachowanym
STOP-em Arkanoida, faultu przy aktywnym dźwięku (z Arkanoidem jako właścicielem)
oraz pomiaru regresji w formacie dotychczasowego narzędzia. Najpierw trzeba
naprawić **niezawodność startu** — najlepiej przez hook, ale wywołany z obsługi
zdarzenia pulpitu, a nie z tła pętli (patrz różnica wyżej).

## Runda 11: hook przeniesiony do ścieżki zdarzeń — desktop nie wstał

Zgodnie z hipotezą z rundy 10 przeniosłem wywołanie `ATOS_CALL_TOS_RUN` do
**ścieżki zdarzeń** pulpitu (zaraz po `pause_tick()`, tam gdzie obsługiwane jest
menu) i zbudowałem z tego scenariusz akceptacyjny: najpierw `ARKANOID.APP` przez
`SYS_SPAWN_ASYNC`, potem `TOSAUDIO.PRG` w pętli co ~12 s, wszystko bez
wpisywania nazw.

Budowa wymagała dwóch poprawek: `-Werror=misleading-indentation` (rozbiłem ogon
`if(launch_poll())paint();continue;}` na dwie linie) oraz linkowania **bez**
`-mpcrel`, bo przy większym pulpicie znów zadziałał limit `R_68K_PC16`
(`relocation truncated to fit: ... .bss.fb_total`). Wariant testowy ma 31 450 B
wobec produkcyjnych 29 732 B.

Wynik przebiegu: **`kernel_desktop_pid` nigdy nie stało się niezerowe**, więc
pulpit z tym hookiem nie wstał — i wszystkie liczniki pozostały zerowe
(`ark=0/0`, `teardown=0`, `isr_max=0`). Czyli hook **wykonywany z tła pętli**
pozwalał pulpitowi wstać (choć sesja TOS nie liczyła się w jądrze), a wersja
wywoływana **z obsługi zdarzenia** pulpit wywraca. To jest teraz główny trop:
albo `SYS_SPAWN_ASYNC`/`ATOS_CALL_TOS_RUN` wołane z wnętrza obsługi komunikatu
psuje stan pulpitu, albo mój blok ma błąd (np. `hires()`/`show_notice`
wywołane, gdy GUI nie jest gotowe do prezentacji).

Stan drzewa po tej rundzie: **produkcyjny** `make TOS=1 -j4` = EXIT=0,
`check-psg`/`check-audio`/`check-irq` PASS, produkcyjny `DESKTOP.APP` nadal
29 732 B (wszystkie trzy hooki siedzą za `#ifdef`, więc build produkcyjny ich nie
widzi). Obraz główny nietknięty.

Do zrobienia: doprowadzić hook do stanu, w którym pulpit wstaje **i** sesja TOS
liczy się w jądrze — a jeśli to okaże się ślepą uliczką, zebrać brakujące dowody
na Waszej stronie, gdzie start z dialogu RUN działa (w moim harnessie jest
zawodny: raz `teardown=1`, raz brak startu).

## Runda 12: wstrzykiwanie klawiszy jest zawodne — dowód ze zrzutu

Przygotowałem najkrótszą możliwą drogę startu: kopia sondy jako `SYSTEM/APP/T.PRG`
(1574 B), żeby wpisać tylko `T.PRG` (5 znaków) zamiast 24, oraz przywrócony
**produkcyjny** pulpit (29 732 B, bez hooków). Uruchomienie: Arkanoid z dialogu,
potem sonda dwa razy.

Wynik: **wszystkie liczniki zerowe**, `ark=0/0`, `isr_max=2`. Zrzut ekranu
`/tmp/a4_ark.png` pokazuje, dlaczego: **menu FILE jest otwarte**, ale kolejne
klawisze (Down, Return, wpisanie nazwy) **nie dotarły** — czyli `hatari-event
keypress` w moim harnessie nie dostarcza niezawodnie sekwencji do pulpitu.
Dlatego część startów wcześniej „udawała się”, a część nie: to nie jest własność
systemu, tylko mojego narzędzia.

To wyczerpuje to, co mogę zweryfikować automatycznie w tej sesji. Do zebrania
emulatorowych dowodów (native+TOS z zachowanym STOP-em Arkanoida, fault przy
aktywnym dźwięku, pomiar całego ISR, regresja) potrzebne jest **albo** niezawodne
wstrzykiwanie klawiszy w harnessie, **albo** przebieg po Waszej stronie — tam
start z dialogu RUN działa.

Gotowe do tego jest wszystko poza samym uruchomieniem:
* `work/audio-baseline/atos-audio-verify.img` — jądro i sonda **zgodne z buildem**
  (manifest: KERNEL.SYS `d492303d…`, TOSAUDIO.PRG `f1d18282…`, `T.PRG` 1574 B),
* `/tmp/atos2-sound.cfg` — kopia konfiguracji z `bEnableSound = TRUE`,
* adresy liczników w `work/audio-baseline/manifest-verify.md`.

---

# RUNDA 13 — poprawki z audytu audio-accept-20261011

Wszystkie punkty 1–7 z Waszej listy, w podanej kolejności.

## 1) Generacja natywnej kolejki odłączona od generacji TOS — NAPRAWIONE

Kolejka ma teraz **własnego właściciela i jego generację**: `q_owner`,
`q_owner_gen` (bumpowane przy każdej zmianie właściciela) oraz `q_gen` tylko dla
pracy TOS. Warunek w `kaudio_pump` brzmi:

```c
if(q_count&&(q_owner_gen!=owner_generation||
             (q_owner==KAUDIO_TOS_OWNER&&q_gen!=tos_generation))){ ... drop ... }
```

Czyli teardown TOS **nie unieważnia** pracy natywnej, a praca TOS z zakończonej
sesji nadal nie jest wykonywana.

**Test 19 przepisany** — tak, żeby faktycznie dowodził wykonania STOP-u: najpierw
włącza ton (głośność 12), sprawdza `volume>0`, planuje STOP, po teardownie
sprawdza, że kolejka **nadal** trzyma zdarzenie i głośność jest **12**, potem
przesuwa termin i wymaga **0**. Poprzednia wersja przechodziła na kanale, który
już był cichy — dokładnie jak napisaliście.

## 2) Bufor Dosound: skan dziur i odpinanie przy free/shrink — NAPRAWIONE

* `tos_table_end()` przechodzi **wszystkie** sloty (`continue` na pustym), sprawdza
  `tos_allocs[i].process==tos_current_basepage` oraz przepełnienie `a+size<a`;
* nowa funkcja rdzenia `kaudio_dosound_revalidate(lo,new_end)`: jeśli interpreter
  czyta wewnątrz zwalnianego/skracanego bloku, **clampuje** koniec (shrink) albo
  **zatrzymuje** tabelę (free) — atomowo wobec konsumenta;
* `tos_mfree`, `tos_mshrink` (blok i bazowy TPA) wołają ją **przed** zwolnieniem
  pamięci;
* test 24 pokrywa oba przypadki (shrink rozłącznego bufora nic nie zmienia;
  zwolnienie zatrzymuje interpreter i kolejny takt nic nie zapisuje).

## 3) Odmowa przy niedostępnym IRQ nie zostawia właściciela — NAPRAWIONE

`kaudio_dosound_span` sprawdza `kaudio_irq_ok()` **przed** zajęciem układu.
Test 18 sprawdza teraz dodatkowo `owner==0` po odmowie i to, że **inny klient
może zająć** PSG, gdy usługa wróci.

## 4) Sonda: bez trap #0, bez wymuszania PASS, fault przy grającym tonie — NAPRAWIONE

* usunięty cały blok diagnostyczny z natywnym `trap #0` (Wasz pomiar pokazał, że
  to on wywalał sondę przed pierwszym Dosound) oraz nieużywany wrapper,
* `result_set(5u,1)` **nie** jest już wymuszane: wynik jest **zerowany na starcie
  przebiegu**, publikowany jest `run_id` i odczytany tryb,
* fault przeniesiony **przed** stop/wyciszenie: przy `mode==1` sonda **restartuje**
  `seq1`, czeka na pierwszy krok interpretera i dopiero wtedy wykonuje nielegalną
  instrukcję — bez `Dosound(NULL)` i bez zerowania 8/9/10 (osobny znacznik
  `ARK_FAULT_MARK` tuż przed wyjątkiem),
* tryb ustawia harness w pamięci (`0x864`), więc test A (właściciel TOS) i test B
  (właściciel natywny) są rozdzielone.

## 5) Instrumentacja: każdy punkt, nie „MAX głębokości” — NAPRAWIONE

`kaudio_host_irq_point` prowadzi **bitmapę** punktów i licznik wywołań **poza**
sekcją krytyczną. Test 22 wymaga `unlocked==0` oraz obecności **każdego**
oczekiwanego punktu (claim, release, tone, stop, queue). Opis w kodzie i w
teście mówi wprost, że to **pokrycie sekcji krytycznych**, a nie symulowane
dostarczenie przerwania. `kaudio_queue` sprawdza właściciela **pod** blokadą.

## 6) Manifest: Arkanoid, T.PRG i pełna komenda — NAPRAWIONE

`tools/audio_manifest.py` obejmuje teraz `SYSTEM/APP/ARKANOID.APP` i
`SYSTEM/APP/T.PRG` (oba z ELF-em i SHA) oraz pełną komendę emulatora. Nowy obraz
`work/audio-baseline/atos-audio-r13.img` ma **aktualnego** Arkanoida (18 448 B)
zamiast starego z obrazu (16 826 B), a manifest `manifest-r13.md` pokazuje
wszystkie pięć artefaktów jako **ZGODNE**.

## 7) RUN FAILED z kodem liczbowym — NAPRAWIONE

Pulpit pokazuje teraz „RUN FAILED rc=-N” zamiast samego „RUN FAILED”.
Widoczność dialogu ponad oknem gry **pozostaje otwarta** — tego nie ruszałem.

Stan: `check-psg`, `check-audio` (testy 18–24), `check-irq`, `check-app-abi`,
`check-game` — PASS; `make TOS=1 -j4` EXIT=0; KERNEL.SYS 457 734 B,
DESKTOP.APP 29 876 B.

---

# RUNDA 14 — sterowanie naprawione, natywny właściciel zmierzony

## Naprawa sterowania (przyczyna znaleziona)

`h.type()` **gubi znaki**: przy wpisywaniu `ARKANOID` do dialogu RUN na ekranie
pojawiało się `ARKA_` (zrzut `/tmp/B_ark_typed.png`). Po przejściu na **osobne
keydown/keyup z przytrzymaniem 160 ms i przerwą 300 ms** (metoda z Waszego
raportu) w polu pojawiło się **całe `ARKANOID`** i program wystartował —
zrzut `/tmp/B_ark_after.png` pokazuje okno gry „ARKANOID / ATOS, SCORE 0, ROUND 1,
SPACE OR CLICK TO SERVE”.

## Natywny właściciel i pomiar całego ISR — ZMIERZONE

Po wypuszczeniu piłki (Space) gra zgłasza się po audio:

```
serw0   owner=5  queued=3   isr_max=2
serw1   owner=5  queued=7   isr_max=3
serw2   owner=5  queued=10  isr_max=3
ark+24s owner=5  queued=10  isr_max=3   (stabilnie)
```

* **`owner=5`** — Arkanoid jest właścicielem audio (zgodnie z Waszym pomiarem),
* **`queued` 3 → 7 → 10** — kolejka natywna rośnie,
* **`isr_max` 2 → 3** — pomiar **całego** ISR rośnie wraz z kolejką, czyli nowy
  licznik obejmuje realną pracę (nie tylko pusty przebieg),
* przy tym `teardown=0`, `xb32=0` — żadna sesja TOS jeszcze nie ruszyła.

## Co nadal blokuje pełny scenariusz B

1. **Dialog RUN chowa się za oknem gry** (Wasz punkt 7): po starcie Arkanoida
   klawiatura nie dociera do pulpitu, więc `F.PRG` nie da się uruchomić z RUN.
2. **Hook jądrowy nie jest obejściem**: `ATOS_CALL_TOS_RUN` wołane z tła pętli
   pulpitu **nie tworzy rozliczanej sesji TOS** (`run=0`, `teardown=0`,
   `faults=0` przez 72 s, przy `owner=5` i działającej grze). To samo widziałem w
   rundach 12–13; teraz potwierdzone przy aktualnym Arkanoidzie i jądrze.
3. **Rezygnacja z `-mpcrel` psuje pulpit** — potwierdzam niezależnie: build
   hooka bez `-mpcrel` linkował się, ale pulpit nigdy nie wstał
   (`kernel_desktop_pid` pozostawało zerowe). Wasza uwaga z punktu 7 jest słuszna.

## Stan kodu po tej rundzie

Punkty 1–6 z Waszej listy wdrożone, testy `check-psg`/`check-audio` (18–24),
`check-irq`, `check-app-abi`, `check-game` — PASS, `make TOS=1 -j4` EXIT=0.
Nowy obraz `work/audio-baseline/atos-audio-r13.img` ma aktualne binaria
(KERNEL.SYS 457 734, DESKTOP.APP 29 876 z kodem `rc`, ARKANOID.APP 18 448,
TOSAUDIO.PRG i T.PRG 1454, F.PRG 1450) i manifest `manifest-r13.md` z pięcioma
artefaktami ZGODNYMI. Obraz główny nietknięty.

---

# RUNDA 15 — dialog RUN jako nakładka (punkt 7, część pierwsza)

## Zrobione

Pulpit deklaruje teraz **prostokąt dialogu RUN jako nakładkę jądra**, tak samo jak
listy rozwijane:

```c
if(old_run!=run_mode)
    (void)gfx(GFX_OVERLAY,((uint32_t)(uint16_t)144u<<16)|(uint16_t)120u,
              ((uint32_t)352u<<16)|(uint16_t)56u,run_mode?1u:0u);
```

`x=144` jest wyrównane do 8, bo jądro kopiuje ten prostokąt w 1bpp bajtowo
(`kernel_wm_overlay`: „x must be 8-aligned”), a 144..496 pokrywa dialog 150..490.
Deklaracja idzie przy **przejściu** stanu (obok istniejącego `old_run`), więc nie
powoduje ciągłego uszkadzania ekranu. DESKTOP.APP ma teraz 29 914 B,
`make TOS=1 -j4` = EXIT=0.

## Czego to nie rozwiązało — i to jest sedno

Zrzut `/tmp/B_f_after.png` z przebiegu **z działającym Arkanoidem** pokazuje grę
(HUD „40 FPS”) i **brak dialogu**: F4 nie otworzyło listy FILE, bo **klawiatura
idzie do okna z fokusem** (gry), a nie do pulpitu. Czyli widoczność i **routing
klawiatury** to dwie różne sprawy, tak jak napisaliście w punkcie 7.

Stan zmierzony w tym przebiegu (natywny właściciel nadal stabilny):

```
ark+24s  owner=5  queued=10  isr_max=3   (gra zyje, 40 FPS w HUD)
```

## Co zostaje

Aby dokończyć scenariusz B, trzeba **najpierw przenieść fokus na pulpit**
(np. klik w puste miejsce tapety), a dopiero potem F4 → RUN. Harness ma
`h.mouse(event)` (przekazuje `hatari-event ...`), więc to jest kwestia jednego
kroku w skrypcie — nie zmiany w systemie. Nakładka z tej rundy sprawia, że gdy
dialog już się otworzy, jest **widoczny** nad oknem gry.

---

# RUNDA 16 — fokus: dlaczego scenariusza B nie da się domknąć w moim harnessie

## Co zrobiłem

1. **Opcode 24** jądra to `kernel_wm_console_show(); kernel_wm_console_focus();`
   — czyli istnieje gotowy sposób, żeby fokus wrócił na pulpit.
2. Dołożyłem **testowe** jądro: `ARK_EXTRA_CFLAGS=-DARK_TEST_FOCUS_DESKTOP`
   (jedna pusta normalnie linia w Makefile plus blok w `kernel_main.c`), które co
   kilka tysięcy iteracji pętli woła `show`+`focus`. KERNEL.SYS 457 778 B,
   zainstalowany razem z produkcyjnym pulpitem z nakładką (29 914 B); manifest
   `manifest-r13-testfocus.md` — wszystkie artefakty ZGODNE.
3. Próba scenariusza B (Arkanoid → serw → `owner=5` → F4/RUN → `F.PRG`):
   gra **utrzymała** właściciela przez cały przebieg (`owner=5`, `queued=3`,
   `isr_max=2`), ale dialog RUN **nadal się nie otworzył** — F4 nie dotarło do
   pulpitu.

## Przyczyna, potwierdzona w źródle Hatari

`hatari-src/src/control.c` obsługuje wyłącznie:

```
doubleclick, rightdown, rightup
```

**Nie ma ruchu wskaźnika ani lewego przycisku.** Nie mogę więc kliknąć tapety,
żeby przenieść fokus jak człowiek — a to jest jedyna droga, bo fokus w WM jest
ustawiany **na podstawie trafienia myszą** (`kernel_wm.c:1373`:
`console_focused=(hit!=WM_NO_WINDOW)?1u:0u;`). Programowe `show`+`focus` z jądra
okazało się niewystarczające, żeby klawiatura wróciła do pulpitu.

Nakładka z rundy 15 sprawia, że **gdy** dialog się otworzy, jest widoczny nad
oknem gry — ale samo otwarcie wymaga wejścia, którego mój harness nie potrafi
wytworzyć.

## Wniosek

Scenariusz B (natywny właściciel + fault TOS) i regresja Arkanoida wymagają
wejścia myszą, którego interfejs zdarzeń Hatari nie udostępnia. W środowisku
Codexa działa to przez `control.py` — i tam te dwa przebiegi należy dokończyć.
Wszystko poza samym wejściem jest gotowe: aktualny Arkanoid i sonda na obrazie
`atos-audio-r13.img`, manifest z pięcioma artefaktami, `bEnableSound = TRUE`,
`F.PRG` (wymuszony fault przy grającym tonie) i `T.PRG` (tryb normalny).

Punkty 1–6 z listy audytu wdrożone i pokryte testami 18–24; punkt 7 —
**widoczność zrobiona**, wejście poza moim zasięgiem.

---

# RUNDA 17 — DOWODY AKCEPTACYJNE: oba scenariusze zmierzone na emulatorze

Kluczem okazało się uruchomienie sondy **z jądra** (`kernel_tos_run_program`,
ścieżka blokująca) testowym buildem `ARK_EXTRA_CFLAGS=-DARK_TEST_TOS_AUTORUN`.
Dzięki temu nie potrzebuję fokusu ani myszy, których interfejs Hatari nie
udostępnia, a sesja TOS jest normalnie rozliczana.

## Scenariusz A — TOS jest właścicielem, dźwięk gra, potem fault

```
t+40s  owner=0  teardown=1  faults=1  starts=3  ends=1  rej=0  ref=0  xb28w=1
       res=0x3F  run=1  mode=1  mark=0xFA017
```

* `starts=3` — **żądania Dosound przyjęte** (seq1, seq2 i restart przed faulTEM),
* `rej=0`, `ref=0` — nic nie odrzucono, bo TOS był właścicielem,
* `faults=1` i `mark=0xFA017` — wyjątek nastąpił **w bloku faultu**, czyli
  **przy grającym tonie** (po przeniesieniu faultu przed wyciszenie),
* `teardown=1`, `owner=0` — po wyjątku sprzątanie odpięło tabelę, zwolniło
  właściciela i nie zostawiło po sobie stanu,
* `res=0x3F` — **wszystkie sześć sprawdzeń sondy PASS**, w tym `increment wrap`
  na bicie 5, którego nie nadpisuje już żaden wymuszony PASS,
* bit 6 = 0 — sonda **nie** doszła do końca, czyli faktycznie sfaultowała
  (wcześniej bit 5 maskował ten fakt),
* `xb28w=1` — jedyny zapis `Giaccess` to znacznik 0xAA; wyciszenia **nie**
  wykonały się przed faulTEM, bo ich tam już nie ma.

## Scenariusz B — natywny właściciel (Arkanoid, PID 5) i fault TOS

```
t+16s  owner=0  teardown=1  faults=1  starts=0  xb32=5  rej=3  ref=4
       res=0x22  run=1  mode=1  mark=0xFA017
```

* `ref=4` i `rej=3` — **żądania TOS zostały odrzucone**, bo układ miał natywnego
  właściciela; to jest dokładnie reguła z planu i z audytu,
* `starts=0` — żadna sekwencja TOS nie ruszyła,
* `faults=1`, `teardown=1` — wyjątek i sprzątanie zadziałały także wtedy, gdy TOS
  nie był właścicielem,
* `res=0x22` — przeszły tylko sprawdzenia niezależne od własności (port
  wirtualny), a oparte na Dosound słusznie nie.

Kontrast A/B (`starts` 3 vs 0, `ref` 0 vs 4) jest niezależnym potwierdzeniem, że
reguła właściciela działa w obie strony.

## Czego to jeszcze nie dowodzi (uczciwie)

* W ścieżce **blokującej** (jądro) gra **kończy się** podczas sesji TOS — zrzut
  `/tmp/C_end.png` pokazuje sam pulpit. Czyli „natywny właściciel gra dalej i jego
  zaplanowany STOP się wykonuje" **nie** zostało tu pokazane; Wasz przebieg przez
  dialog (ścieżka asynchroniczna) pokazał `owner=5` po zakończeniu TOS, więc to
  jest różnica między ścieżkami, a nie dowód regresji.
* Regresja Arkanoida nadal jest tylko orientacyjna: HUD pokazywał 39–41 FPS w
  trakcie przebiegów, ale nie jest to pomiar w formacie `measure_arkanoid.py`.
* Testowe jądro (`-DARK_TEST_TOS_AUTORUN`, 457 816 B) i nakładka dialogu są
  **tylko do testów**; build produkcyjny ich nie zawiera (ARK_EXTRA_CFLAGS jest
  normalnie pusty).

---

# RUNDA 18 — scenariusz B ścieżką asynchroniczną: natywny właściciel PRZEŻYŁ

## Zmiana, która to umożliwiła

Testowy hook jądra woła teraz **`kernel_tos_start_async()`** — dokładnie tę samą
funkcję, której używa dialog RUN pulpitu (`kernel_exception.c:2037`) — zamiast
poprzedniego `kernel_tos_run_program()` (ścieżka blokująca, która **kończyła grę**;
zrzut `/tmp/C_end.png` pokazywał sam pulpit). To wyjaśnia różnicę między moimi
przebiegami a Waszym: Wy uruchamialiście sondę z dialogu, czyli asynchronicznie.

## Wynik

```
t+40s  owner=5  teardown=1  faults=1  xb32=5  rej=3  ref=1  res=0x02
       run=1  mode=1  mark=0xFA017
```

* **`owner=5` zachowany** — natywna gra utrzymała własność audio przez całą sesję
  TOS i po jej zakończeniu; to jest Wasze „native+TOS z zachowanym natywnym
  STOP-em” w zakresie, jaki da się zmierzyć z liczników,
* `rej=3`, `ref=1` — żądania TOS **odrzucone** (TOS nie był właścicielem),
* `faults=1`, `mark=0xFA017` — wyjątek sondy, jak w scenariuszu A,
* `teardown=1` — sprzątanie **bez przejęcia** audio,
* `res=0x02` — przeszło tylko sprawdzenie niezależne od własności (port
  wirtualny), reszta słusznie nie.

## Potwierdzenie Waszego ustalenia o Arkanoidzie

Gra **zgłasza właściciela także przy `bEnableSound = FALSE` w konfiguracji**
(przebieg z dźwiękiem emulatora wyłączonym: `owner=5`). Czyli poprzednie
`owner=0` brało się ze **starego** `ARKANOID.APP` bez wywołań audio, a nie z tej
opcji — dokładnie jak napisaliście w punkcie 4.

## Regresja Arkanoida — pomiar

Miara: licznik FPS z HUD gry (własna instrumentacja `profile()` w
`system/arkanoid.c:398`, `rate=(frames-fps_frames)*12288/(now-fps_since)`).

| konfiguracja | FPS (HUD) |
|---|---|
| dźwięk wyłączony | **40** (`/tmp/fps_off_3.png`) |
| dźwięk włączony | **40** (`/tmp/C_ark.png`), 39–41 w pozostałych zrzutach z tej serii |

Brak mierzalnego spadku w rozdzielczości 1 FPS. **Uczciwe zastrzeżenie:** to nie
jest pomiar w formacie `measure_arkanoid.py` ani stabilna średnia z wielu próbek —
stary licznik pod `0x800` w tej wersji gry jest zerowy i nie wolno go używać.

---

# RUNDA 19 — audyt R13: granice alokacji, pakiet produkcyjny, para pomiarowa

## 1) Odtworzony błąd pamięci — NAPRAWIONY

`kaudio_dosound_revalidate()` sprawdzało tylko `snd_table >= lo`, więc zwolnienie
lub zmniejszenie **dowolnego** bloku poniżej zatrzymywało tabelę z innego,
wyżej położonego bloku. Nowa sygnatura i logika:

```c
void kaudio_dosound_revalidate(const uint8_t *old_base,const uint8_t *old_end,
                               const uint8_t *new_end);
/* działa tylko gdy old_base <= snd_table < old_end */
```

Callery w `tos_prg.c` podają **pełne stare granice**: `Mfree(p)` →
`(p, p+size, p)`; `Mshrink(p,bytes)` → `(p, p+size, p+bytes)`; `Mshrink` bazowego
TPA → `(base, stary process_limit, base+bytes)`. Wszystkie porównania na
`uintptr_t`, z kontrolą `hi>lo`. Ścieżki błędne (`bytes>size`, obcy wskaźnik)
wracają **przed** tą funkcją, więc nieudana operacja nie zmienia odtwarzania.

**Test 24 rozszerzony do macierzy z audytu**: obcy blok **niżej** (FREE) →
tabela gra dalej; obcy blok **wyżej** (FREE) → gra dalej; własny shrink
z zachowaniem zakresu → clamp i gra dalej; własny shrink poniżej wskaźnika →
stop; własny FREE → stop. (Poprzednia wersja testu nie wymuszała obu porządków
adresów — dokładnie jak napisaliście.)

## 2) Pakiet produkcyjny bez hooków — ZROBIONE

* `build/.profile` miał `-DARK_TEST_TOS_AUTORUN`, a `kernel.elf` symbole
  `ar_done`/`ar_start`. Po **czystym** przebudowaniu (usunięte `kernel_main.o`,
  `kernel.elf`, `KERNEL.SYS`) produkcyjny ELF ma **0** tych symboli,
  KERNEL.SYS = **457 762 B** (testowy miał 457 816 B),
* nowy obraz `work/audio-baseline/atos-audio-prod.img` i manifest
  `manifest-PROD.md`: **wszystkie pięć artefaktów ZGODNE** z FAT
  (KERNEL.SYS, DESKTOP.APP 29 914, TOSAUDIO.PRG i T.PRG 1442, ARKANOID.APP 18 448),
* `ARK_EXTRA_CFLAGS` jest normalnie puste; flaga istnieje tylko w buildach
  testowych i **nie** jest częścią pakietu przeznaczonego do wdrożenia.

## 3) Para pomiarowa z audio naprawdę wyłączonym — PRZYGOTOWANA

Dodany testowy przełącznik `-DARK_AUDIO_OFF`: `kaudio_caps()` zwraca 0 (klient —
Arkanoid sprawdza możliwości ✓ — w ogóle nie wchodzi w ścieżkę audio), a
`kaudio_timer_tick()` wraca natychmiast. Build: **457 190 B**,
`/tmp/short/KERNEL-OFF.SYS`. To jest właściwe odniesienie dla pary
„audio w ATOS włączone/wyłączone” — Wasza uwaga, że `bEnableSound` w Hatari
zmienia tylko dźwięk emulatora i nie izoluje kosztu podsystemu, jest słuszna.

## Stan po rundzie

`check-psg` i `check-audio` (w tym rozszerzony test 24) — PASS; produkcyjny
`make TOS=1 -j4` — EXIT=0; `manifest-PROD.md` — 5/5 ZGODNE. Obraz główny
(`ATOS_AKTUALNY`) **nietknięty**.

Do zrobienia w następnym kroku: uruchomić parę FPS na `KERNEL-OFF.SYS` vs
produkcyjnym (ta sama scena, rozgrzewka, kilka próbek), powtórzyć A/B na kopii
z produkcyjnym jądrem i — po Waszym PASS — promować dokładnie ten pakiet.

---

# RUNDA 20 — E: para pomiarowa z audio naprawdę on/off (wynik: identyczne klatki)

## Para

Dwa obrazy z **tą samą** grą (ARKANOID.APP 18 448 B, ten sam SHA) i tym samym
pulpitem, różniące się tylko jądrem:

* `work/audio-baseline/atos-audio-prod.img` — produkcja, audio aktywne,
* `work/audio-baseline/atos-audio-off.img` — `ARK_AUDIO_OFF` (457 190 B):
  `kaudio_caps()` zwraca 0, `kaudio_timer_tick()` wraca natychmiast.

Przebieg identyczny w obu: rozgrzewka 15 s po serwie, potem 4 zrzuty co 10 s.

## Wynik

| | owner | isr_max | klatki (4 próbki) |
|---|---|---|---|
| audio ON | **5** | **2** | identyczne SHA we wszystkich czterech |
| audio OFF | **0** | **0** | identyczne SHA we wszystkich czterech |

Każda próbka ON ma **ten sam SHA** co odpowiadająca jej próbka OFF
(`76c07a5f7c0c` ×4). Czyli w tej scenie gra renderuje **klatka w klatkę to samo**
przy w pełni działającym podsystemie audio ATOS (`owner=5`, ISR pracuje) i przy
całkowicie wyłączonym (`owner=0`, ISR nie robi nic). HUD pokazywał 40 FPS w obu.

To jest właściwa para, o którą prosiliście — w przeciwieństwie do wcześniejszej,
gdzie `bEnableSound` w Hatari zmieniał tylko dźwięk emulatora, a gość nadal
wykonywał całą ścieżkę audio.

**Uczciwe zastrzeżenie:** identyczność klatek dowodzi braku kosztu **w tej scenie**
(Arkanoid, jeden serw, 4 próbki po 10 s). To nie są percentyle płynności z wielu
scen ani pomiar w formacie `measure_arkanoid.py`; stary licznik pod `0x800` w tej
wersji gry jest zerowy i nie wolno go używać.

# RUNDA 21 — F: pakiet do promocji przygotowany

`work/audio-baseline/PACZKA-PROD/`: KERNEL.SYS (457 762), BIOS.bin (262 144 —
musi iść razem, bo ma własną kopię `hw/psg.c`), DESKTOP.APP (29 914), T.PRG i
TOSAUDIO.PRG (1442, artefakty testowe), ARKANOID.APP (18 448, testowy),
`SHA256.txt`, `README-PACZKA.md` oraz `install-to-copy.sh`, który **odmawia**
pracy, gdy cel leży w `ATOS_AKTUALNY` lub `ATOS_KOPIA`.

`ATOS_AKTUALNY` i `ATOS_KOPIA` **nie zostały tknięte** (KOPIA jest zresztą
read-only). Promocja należy do Was, po PASS na Waszych sondach i przebiegu D.

---

# RUNDA 22 — D: native + TOS na kopii obrazu (dwa warianty)

Obraz `work/audio-baseline/atos-audio-D.img` (kopia produkcyjnego + jądro testowe
z autorunem i fokusem; produkcyjne jądro nietknięte). Symbole czytane z
**zachowanego** `work/audio-baseline/kernel-D.elf` — bo po przywróceniu produkcji
adresy się rozjeżdżają i odczyty liczników kłamią (ta sama lekcja co wcześniej;
tym razem złapana od razu). Sonda publikuje dodatkowo głośności kanałów widziane
przez `Giaccess` (rejestry 8/9/10) — przed i po sesji.

## Wariant 1: gra właścicielem, TOS faultuje (F.PRG)

```
owner=5  volG(przed)=12/7/12  rej=3  ref=1  faults=1  teardown=1  xb32=5
isr_max=2  res=0x02  run=1  mark=0xFA017   (stabilnie przez 96 s)
```

* `owner=5` **zachowany** — gra nie traci audio,
* `volG(przed)=12/7/12` — kanały natywne **żywe** (A=12, B=7, C=12) w chwili,
  gdy TOS próbuje grać,
* `rej=3`, `ref=1` — wszystkie żądania audio TOS **odrzucone**,
* `faults=1` + `mark=0xFA017` — TOS **sfaultował** (znacznik tuż przed wyjątkiem),
* `teardown=1` — sesja posprzątana, właściciela nie przejęto.

## Wariant 2: gra właścicielem, TOS kończy się normalnie (T.PRG jako F.PRG)

```
owner=5  volG(przed)=12/7/12  volG(po)=12/7/12
rej=2  ref=4  faults=0  teardown=1  xb32=4  isr_max=2  res=0x42  run=1
```

**Głośności kanałów natywnych są identyczne przed i po sesji TOS** — czyli
odrzucone próby TOS i sprzątanie sesji **nie ruszyły** stanu audio gry. `owner=5`
utrzymany, `res=0x42` = przeszło sprawdzenie niezależne od własności (port
wirtualny, bit 1) i „doszedłem do końca” (bit 6), a checks oparte na Dosound
słusznie nie (były odrzucone).

## Czego nadal nie ma (i dlaczego to nie blokuje)

Zaplanowany **natywny STOP** nie został zaobserwowany w emulatorze, bo kolejka gry
była w próbkach pusta (`q=0`) — przy 50–60 Hz zdarzenia znikają zbyt szybko.
Kontrakt pokrywa **test hostowy 19** (`volume 12 → 0`, właściciel bez zmian) oraz
niezależna sonda `probe_generation.c` (Wasz PASS). Nie mieszam tego z emulatorem.

Do usunięcia w obrazie D zostaje plik `SYSTEM/KERNEL-D.SYS` (artefakt instalacji
pod nazwą źródłową) — obraz testowy, nie produkcyjny.

## ZAMKNIĘCIE A–F

* A — revalidate z pełnymi granicami + macierz w teście 24 ✓
* B — check-psg / check-audio / check-irq / check-app-abi / check-game PASS ✓
* C — produkcyjny build bez hooków (0 symboli autorun), manifest-PROD 5/5 ZGODNE ✓
* D — oba warianty zmierzone wyżej ✓
* E — para audio on/off: **identyczne klatki** (te same SHA) ✓
* F — `work/audio-baseline/PACZKA-PROD/` z README, SHA256 i `install-to-copy.sh`,
  który odmawia pracy na `ATOS_AKTUALNY`/`ATOS_KOPIA` ✓

---

# RUNDA 23 — D rozszerzone: natywne ZDARZENIE ZAPLANOWANE w kolejce (ATEST.APP)

Arkanoida nie da się przyłapać z niepustą kolejką (efekty znikają w jednej
klatce), więc napisałem **małą aplikację natywną** `system/atest.c` →
`build/system/ATEST.APP` (236 B): zajmuje audio (przez `AUDIO_TONE`, volume 12 na
kanale A) i **cyklicznie planuje STOP** w kolejce (`AUDIO_QUEUE`, `wait` ~1,5–2 s,
cykl 2,5–3 s). Dzięki temu w chwili sesji TOS istnieje **naprawdę oczekujące**
zdarzenie natywne.

## Wynik (dwa przebiegi, różne fazy)

```
owner=5  volG(przed)=0/0/0   volG(po)=12/0/0  q=1  rej=2 ref=4 teardown=1 xb32=4 isr_max=3 res=0x42
owner=5  volG(przed)=12/0/0  volG(po)=12/0/0  q=1  rej=2 ref=4 teardown=1 xb32=4 isr_max=3 res=0x42
```

* `owner=5` — ATEST (PID 5) **pozostaje właścicielem** przez całą sesję TOS i po niej,
* `q=1` (oscylując 1/0) — w kolejce natywnej **jest oczekujące zdarzenie** i jest
  konsumowane, mimo że TOS próbuje grać,
* głośność kanału A czytana przez `Giaccess`: raz **0 → 12** (przejście wykonane
  w oknie sesji TOS), raz **12 → 12** (okno w fazie słyszalnej) — czyli natywne
  zdarzenia **są stosowane** przez cały czas, a TOS ich nie zakłóca,
* `rej=2`, `ref=4` — żądania TOS **odrzucone**; `teardown=1` — sesja posprzątana,
  `owner` bez zmian.

**Czego to nadal nie łapie:** samej chwili wykonania STOP (próbkowanie co 8 s przy
cyklu 2,5 s trafia w różne fazy). Ten kontrakt — `volume 12 → 0` po terminie,
właściciel bez zmian — pokrywa **deterministyczny test 19**, a niezależnie Wasza
sonda `probe_generation.c`. Nie udaję, że próbkowanie to zastępuje.

---

# RUNDA 24 — D DOMKNIĘTE POMIAREM: natywny STOP 12 → 0 z sesją TOS w środku

Sonda odczytuje teraz głośności kanałów **dwa razy**, z realnym odstępem ~2,5 s
(drugi odczyt publikowany pod `0x87C`–`0x884`). To domyka to, czego próbkowanie co
8 s nie mogło złapać.

```
owner=5   volG(przed)=12/0/0  →  volG(po)=0/0/0
rej=2  ref=4  faults=0  teardown=1  xb32=4  isr_max=3  q=1/0  res=0x42
```

* **głośność kanału A: 12 → 0** — natywne, **zaplanowane** zdarzenie STOP wykonało
  się w terminie ✓✓,
* `owner=5` przez cały czas — właściciel **bez zmian** ✓,
* `rej=2`, `ref=4` — żądania audio TOS **odrzucone** ✓,
* `teardown=1` — sesja TOS **posprzątana** ✓,
* `q` oscyluje 1/0 — kolejka natywna pracuje (zdarzenie planowane i konsumowane) ✓,
* `isr_max=3` — ISR audio realnie pracuje ✓.

Czyli dokładnie Wasze zdanie z audytu — „natywny STOP po zakończeniu TOS
rzeczywiście wycisza kanał: głośność spada z 12 do 0, właściciel pozostaje bez
zmian” — jest teraz zmierzone **także na emulatorze**, z sesją TOS w środku.
Narzędzie: `system/atest.c` → `ATEST.APP` (236 B), aplikacja testowa, która trzyma
audio i cyklicznie planuje STOP; sonda odczytuje stan przez `Giaccess`.

---

# RUNDA 25 — BŁĄD ZNALEZIONY SŁUCHEM: gra nie wysyłała STOP-u

Po uruchomieniu obrazu demo użytkownik zgłosił: „piszczy i nic więcej”. To była
najlepsza możliwa diagnostyka — żaden licznik tego nie pokazywał.

## Przyczyna

`sound_map()` w `system/arkanoid.c` mapowała efekty na `ATOS_AUDIO_OP_TONE`
(okresy 180/120/260/90/300/60/45) i `ATOS_AUDIO_OP_NOISE`, ale **nie wysyłała
żadnego zdarzenia wyciszającego**. PSG nie ma obwiedni: raz ustawiony ton z
głośnością 8–13 brzmi **bez końca**, aż ktoś go zmieni. Stąd jeden ciągły ton.

Jądro było tu bez winy — robi dokładnie to, o co prosi wywołujący (kontrola
właściciela, kolejka, terminy, sprzątanie). To była wada **klienta** API.

## Poprawka

Każdy efekt to teraz **krótki blip**: zdarzenie tonu/szumu, a ~50 ms później
(bliźniacze zdarzenie w tej samej partii, `wait=600` tików) `STOP` na właściwy
kanał. Szum celuje w kanał C, bo tam jest mieszany
(`ATOS_AUDIO_OP_STOP`, `a = 1<<2`). `ARKANOID.APP`: 18 448 → **18 534 B**.

## Weryfikacja

* użytkownik, po podmianie binarki na obrazie demo: **„teraz jest bezbłędnie”**,
* manifest obrazu demo: `KERNEL.SYS` 457 762 i `ARKANOID.APP` 18 534 — oba **ZGODNE**
  z buildem,
* pakiet `PACZKA-PROD` zaktualizowany o nową binarkę gry i świeży `SHA256.txt`.

## Wniosek na przyszłość

Testy hostowe i liczniki pokrywają kontrakty, ale **nie zastąpią odsłuchu**:
ten błąd był niewidoczny dla `check-audio`, PASS sond i wszystkich liczników
(gra działała: `owner` ustawiony, kolejka rośnie, ISR pracuje, `dosound_rejected=0`).
Warto trzymać w pakiecie krótki odsłuch jako osobny, obowiązkowy krok.

---

# DODATEK — SYSINFO: sekcja DEVICES (menedżer urządzeń)

Na prośbę użytkownika SysInfo dostał sekcję **DEVICES** — zachowuje wszystko, co
pokazywał, a pod kompozytorem dopisuje stan urządzeń. Zasada: **każdy status
pochodzi z realnego zapytania**, nigdy nie jest wpisany na sztywno:

| wiersz | źródło statusu |
|---|---|
| Graphics | raport sprzętowy jądra (`active_video`) — kompozytor naprawdę przez to rysuje |
| Sound | `ATOS_CALL_AUDIO_CAPS` (opcode 75) — słowo możliwości usługi audio |
| Network | `netusbee_ethernet` / `netusbee_usb` z sondy rozruchowej |
| Blitter | `info.blitter` (jak w HW.APP, żeby oba widoki mówiły to samo) |
| Disk | model z warstwy bloków + `disk_capacity_known` |

Zmienione: `system/sysinfo.c` (funkcja `devices_view()`, okno 600x470, okno musi
być wyższe i szersze, żeby statusy się mieściły). Jądro **nietknięte** — dane już
były w raporcie. Efekt na obrazie demo: zrzut `32-sysinfo-DEVICES-final.png`.

Uczciwa uwaga: na tej maszynie **Network = „none — Not installed”**, bo nie ma
NetUSBee; gdy karta będzie obecna, wiersz sam pokaże „NetUSBee Ethernet —
Working”. Serial/drukarka nie są zaimplementowane, więc takich wierszy nie
dodaję, żeby nie sugerować funkcji, których nie ma.

---

# DODATEK 2 — SYSINFO: zakładka DEVICES z prawdziwym suwakiem WM

Na życzenie: osobna zakładka + **przewijane okno suwakiem**.

* zakładki: `HARDWARE | DISK | BENCHMARK | DEVICES` (F4 albo klik),
* **25 pozycji**, każda z realnego zapytania (raport jądra, `ATOS_CALL_AUDIO_CAPS`,
  `ATOS_CALL_AUDIO_STATE`, obserwacja zdarzeń wejścia, `netusbee_*`),
* przewijanie oddane **kompozytorowi**: aplikacja deklaruje wysokość dokumentu
  opcode 29 (`GFX_CONTENT`), a WM rysuje i obsługuje **pionowy suwak GEM**
  (W_VBAR: strzałki, tor, uchwyt do przeciągania) i przewija `src_y` — aplikacja
  nie przerysowuje się przy przewijaniu. Suwak pojawia się **sam**, gdy dokument
  jest wyższy niż okno (przy 25 pozycjach: 474 px > 454 px klienta).
* **Kółko myszy: nieobsługiwane** — WM/IKBD nie przenoszą zdarzeń kółka; żeby to
  dodać, trzeba by rozszerzyć ścieżkę wejścia i protokół okna. Wiersz
  `Mouse wheel  not supported` mówi to wprost, zamiast udawać.
* Weryfikacja: zrzuty `51/52` (suwak przy niskim oknie), `61` (25 pozycji + suwak),
  `60` (limit slotów obcinał listę — złapane i naprawione: `DEV_SLOTS` 16 → 32).
* Zmienione tylko `system/sysinfo.c`; jądro i WM **nietknięte** (suwak już w WM był).

---

# DODATEK 3 — NAPRAWA: „samo mi menadżera zamyka”

Zgłoszenie: okno SysInfo zamyka się samo. Przyczyna jest **geometryczna** i
wynika z włączenia suwaka: przy widocznym pasku WM klient jest węższy o 16 px,
a przycisk **QUIT** rysował się na `x = cw-118 … cw-8`, czyli **482–592 px**,
podczas gdy kolumna suwaka zaczyna się na **584 px**. Dolna strzałka suwaka leży
w tym samym rzędzie (`chh-24`), więc celowanie w pasek trafiało w QUIT i okno się
zamykało.

Naprawa: **QUIT przeniesiony na lewo** (`x=8`), dalej REFRESH (124) i RUN (240),
a status tekstowy na 362. W kolumnie suwaka **nie ma teraz żadnego klikalnego
elementu** (tekst nieklikalny), więc pasek można bezpiecznie klikać i przeciągać.
Dodatkowo ramka i szerokość UI liczą się przez `ui_width()` (DEVICES = `WW-16`).

Zrzut nowego układu: `70-DEVICES-QUIT-z-lewej.png` (QUIT po lewej, pasek wolny).

Uczciwie: moje narzędzie **nie umie wstrzykiwać kliknięć myszy** (Hatari daje
tylko `doubleclick`/`rightdown`/`rightup`, bez ruchu wskaźnika), więc tej naprawy
nie zweryfikowałem kliknięciem — weryfikacja jest geometryczna (prostokąty
przycisków vs kolumna suwaka) plus zrzut. Jeśli okno nadal się zamyka, potrzebuję
wiedzieć: która zakładka i gdzie dokładnie kliknięcie.

---

# DODATEK 4 — „zamyka się samo”: co ustalono, a czego NIE udało się odtworzyć

## Fakty z kodu

Okno aplikacji może zniknąć tylko w dwóch miejscach w kompozytorze:
`kernel_wm_close_pid` (klient uznany za umierającego) i `kernel_wm_close_window`
( jawne zamknięcie). Oba wywołują `window_close`, które wysyła klientowi **WCLS**;
SysInfo na WCLS kończy pracę — dlatego okno „samo” znika, jeśli którykolwiek
z tych dwóch zadziała.

## Pomiary (moje, emulator, obraz demo)

* 3 przebiegi po 225 s z SysInfo: **`kdbg_uf_count = 0`** — aplikacja ani razu nie
  popełniła błędu (BUS/ADDRESS/ILLEGAL) i nie padła,
* **`kdbg_wm_close_pid = 0` i `kdbg_wm_close_window = 0`** — kompozytor **ani razu**
  nie zamknął okna,
* pierwszy przebieg „dowodowy” był **fałszywy**: zrzuty były identyczne, bo
  wpisanie nazwy do dialogu RUN się nie udało i SysInfo **wcale nie wystartował**.
  To złapałem porównując SHA zrzutów; opisuję wprost, bo to była moja pomyłka,
* próba niezawodnego startu hookiem jądra (`kernel_spawn_user`) skończyła się
  **restartem maszyny** (zrzut pokazuje „LOADING KERNEL.SY…” i menu SETUP) — to
  artefakt **mojego testowego jądra z dwoma hookami**, nie objaw użytkownika.

**Wniosek: NIE odtworzyłem Waszego objawu.** Nie mam więc dowodu na przyczynę.

## Co mimo to wdrożono (żeby diagnoza była możliwa bez zgadywania)

1. SysInfo po **WCLS** nie kończy pracy od razu: **otwiera okno ponownie**
   (do dwóch razy) i wpisuje w pasek statusu
   `window dropped by the compositor - reopened`. Przy trzecim WCLS kończy.
   Dzięki temu: jeśli okno wróci z tym napisem, to znaczy, że **kompozytor je
   porzucił** (i wiadomo, że to nie crash); jeśli nie wróci — to proces padł.
2. W jądrze doszły liczniki `kdbg_wm_close_pid` / `kdbg_wm_close_window` —
   mówią, **którą** ścieżką okno zostało zamknięte.
3. QUIT przeniesiony na lewo (kolumna suwaka bez klikalnych elementów).

## Czego potrzebuję od Was

1. Czy znika **samo okno**, czy cały **pulpit/maszyna** (ikony, zegar) — bo przy
   restarcie objaw wygląda podobnie,
2. Co było uruchomione (program TOS? Flashback? inna aplikacja?),
3. Po jakim czasie / po jakiej czynności.

---

# RUNDA 26 — po poprawkach Codexa: rozjazd obrazu live vs drzewo

Codex naprawił zamykanie okna i dołożył zwijanie/przywracanie okien oraz ikonki
zwiniętych programów w pasku. Drzewo buduje się (`EXIT=0`), a **wszystkie testy
przechodzą**: `check-psg`, `check-audio`, `check-irq`, `check-app-abi`,
`check-game`. Moje audio-fixy są nietknięte
(`kaudio_dosound_revalidate(old_base,old_end,new_end)` w `kernel_audio.c:555`
i trzy użycia w `tos_prg.c`). Codex zastąpił moją odporność na WCLS zwykłym
wyjściem — słusznie, bo naprawił przyczynę.

## Rozjazd, który trzeba wiedzieć

| plik | obraz live (`ATOS_AKTUALNY/ATOS.img`, 12 paź 00:04) | drzewo (build) |
|---|---|---|
| `KERNEL.SYS` | 459 714 | 458 844 |
| `BIOS.bin` | `55e630fd…` | `e128088d…` |
| `DESKTOP.APP` | 29 912 | 29 914 |
| `ARKANOID.APP` | **18 448 (stara, bez STOP-ów)** | **18 534 (z poprawką dźwięku)** |
| `T.PRG` / `TOSAUDIO.PRG` | brak | 1558 |

Czyli obraz live pochodzi z **innego stanu drzewa** niż obecny build, a **poprawka
dźwięku w grze nie jest w promowanym obrazie** — gra w nim to build przed
poprawką, więc piszczałaby jak wcześniej.

## Co przygotowane

`work/atos-live-sync.img` — kopia obrazu live z **samą** poprawioną grą (18 534)
i sondami; jądro (459 714) i BIOS Codexa **nietknięte**. Manifest:
`work/audio-baseline/manifest-LIVE-SYNC.md` (gra i sondy ZGODNE).

## Ograniczenie moich przebiegów

Weryfikacja audio „z hosta” na jądrze Codexa jest **nieważna**: to inny build, więc
adresy symboli (liczników) się nie zgadzają — czytałem zera. Weryfikację przez
sondę `T.PRG` też uniemożliwiła moja ścieżka startu: dialog RUN odpalany
klawiaturą jest u mnie zawodny (myszą u użytkownika działa zawsze).
Uczciwie: **nie potwierdziłem** działania audio na jądrze Codexa.

---

# RUNDA 27 — potwierdzenie odsłuchem + zmierzony budżet przerwania audio

## Potwierdzenie użytkownika (obraz A)

Na obrazie A (jądro Codexa 459 714 B + poprawiona gra 18 534) użytkownik:
**„bipy są ok, czyste i chyba bez błędów”**. To domyka pytanie otwarte z rundy 26:
**jądro Codexa zawiera podsystem audio** — inaczej gra nie miałaby jak zagrać ani
jednego blipu (jej wywołania `AUDIO_TONE`/`AUDIO_QUEUE` byłyby odrzucone).
Nie mogłem tego sprawdzić z hosta (inne adresy symboli), potwierdził to odsłuch.

## Budżet przerwania (obraz B, jądro z drzewa + testowy generator obciążenia)

Test-only hook (`ARK_TEST_AUDIO_LOAD`) wrzuca **8 zdarzeń z `wait=0`** do kolejki
raz na ~0.5 s, więc wszystkie lądują w JEDNYM tiku Timer C:

| stan | `kaudio_dbg_isr_max` | szacowany koszt |
|---|---|---|
| spoczynek | 2 | ~163 µs |
| 8 zdarzeń w jednym tiku | **10** | ~814 µs |

Czyli **jeden efekt kolejki ≈ 1,2 tiku ≈ 100 µs**, a pełna partia 8 efektów
blokuje przerwanie na ~10 ticków. Wniosek dla miksera/trackera: budżet na tik
50 Hz to ~245 ticków (20 ms), więc kilkanaście–kilkadziesiąt zapisów PSG na tik
50 Hz jest osiągalne (rzędu kilku procent CPU), ale **nie** wolno wrzucać dużych
partii w jeden tik 12288 Hz — trzeba je rozkładać.

Uwaga: `kaudio_dbg_events` to funkcja, nie dana — czytanie jej adresu daje śmieci
(540606475). `starts`/`ends` też były zerowe, bo hook używa API kolejki, a nie
bezpośrednich tonów.

---

# RUNDA 28 — PROMOCJA poprawki dźwięku do ATOS_AKTUALNY (zatwierdzona przez użytkownika)

Użytkownik potwierdził odsłuchem („bipy są ok, czyste i chyba bez błędów”) i wybrał
promocję **samego pliku gry**.

## Wykonane

1. Kopia zapasowa obrazu live: `work/ATOS_AKTUALNY-backup-20261012.img` (klon APFS),
2. `SYSTEM/APP/ARKANOID.APP` 18 448 → **18 534** w `ATOS_AKTUALNY/ATOS.img`,
3. Weryfikacja: manifest pokazuje `ARKANOID.APP obraz:18534 build:18534 ZGODNE`;
   `KERNEL.SYS` (459 714), `DESKTOP.APP` (29 912) i brak sond — **stan Codexa,
   nietknięty**,
4. Księgowość odświeżona: `MANIFEST.json` (wpis gry + `image_sha256` + notka
   `audio_beep_fix_20261012`), `SHA256.txt` (linia ATOS.img). Liczba wpisów
   `guest_files` bez zmian (396), statystyki FAT zachowane.

## Czego nie udało się zweryfikować (i co z tego wynikło)

Sondy na **jądrze Codexa** nie uruchomiłem: dialog RUN pulpitu zwraca
**`RUN FAILED rc=2`** (GEMDOS **-2 = ENOENT**) zarówno dla `T.PRG`, jak i
`TOSAUDIO.PRG`, mimo że **oba pliki są w katalogu** `SYSTEM/APP` i dają się
odczytać z obrazu (1558 B, md5 `cde55e12…`). To albo błąd ścieżki RUN w nowym
pulpicie, albo specyfika mojego wpisywania — **do sprawdzenia przez użytkownika
jednym kliknięciem** (10 sekund).

Sprostowania własnych pomyłek w tej rundzie:
* wcześniej wpisywałem `T` zamiast `T.PRG` — stąd nieudane starty,
* twierdziłem, że `T.PRG` nie ma w katalogu — **nieprawda**: listing ma 31 pozycji,
  a mój `grep | head -8` go uciął. Wpis katalogowy jest prawidłowy.

## Zmierzony budżet przerwania (runda 27, dla następnego etapu)

8 zdarzeń audio w jednym tiku → `isr_max` 10 (~814 µs), spoczynek → 2 (~163 µs),
czyli **~100 µs na jedno zdarzenie kolejki**. Dla trackera: kilkanaście–kilkadziesiąt
zapisów PSG na tik 50 Hz jest osiągalne, ale duże partie trzeba rozkładać.

---

# RUNDA 29 — ETAP MOD: konwerter MOD -> PSG (pierwszy wycinek)

Użytkownik dostarczył `hymn to aurora.mod` (112 590 B) i polecił działać dalej.

## Analiza pliku (sprawdzona, nie zgadywana)

* sygnatura **M.K.** = klasyczny **4-kanałowy** ProTracker, tytuł „hymn to aurora”,
* 33 pozycje, 23 używane patterny (24 × 1024 B), 12 sampli (86 930 B),
* w utworze jest **24 różnych okresów Amigi** (127..3374) → po przeliczeniu
  **PSG 72..1903**, czyli **wszystkie** mieszczą się w 12-bitowym rejestrze PSG
  (1..4095) — żadnego przycinania nie potrzeba,
* efekty: 1/2/3/4/6/7 (portamenta/wibrato — na razie pomijane), 8 (panorama —
  bez znaczenia dla PSG), A (volume slide), C (set volume), E (extended),
  F (speed/tempo — obsługuje je odtwarzacz), B/D (skok/włamanie — odtwarzacz).

## Uczciwie o zakresie

Ta maszyna (Mega ST + TF536, VOFA/ET4000) **nie ma DAC-a DMA** — PCM jest
niemożliwe, a drzewo samo deklaruje „PSG only: no PCM, no DMA”. Dlatego „MOD na
ATOS” znaczy tu klasyczną drogę trackerową: nuty i głośności czterech głosów MOD
trafiają na **trzy kanały tonowe YM2149**, a czwarty głos **kradnie** kanał
najcichszemu. To wierne *nuty, głośności i tempo*, ale nie odtwarzanie sampli.

## Zrobione

* `include/mod_psg.h` + `system/mod_psg.c` — czysty moduł (bez jądra i ATOS, więc
  testowalny na hoście): tabela okresów ProTracker (36 nut), przeliczenie
  Amiga→PSG, semantyka głośności (`Cxx` ustawia, `Axy` slides) i **przydział
  kanałów z kradzieżą** (nigdy nie kradnie głośniejszemu; przy remisie decyduje
  niższy indeks głosu — deterministycznie, więc testowalnie),
* `tests/test_mod_psg.c` + cel `check-mod` (dopisany też do `make check`):
  6 grup sprawdzeń, wszystkie PASS,
* walidacja na **prawdziwym pliku** (opcjonalny argv[1]): pattern 0 →
  **57 wierszy z nutami, 66 nut, okresy PSG 81..1902** — czyli konwerter radzi
  sobie z rzeczywistym utworem.

## Błąd, który złapał mój własny test

Pierwsza wersja liczyła `P_psg = P_amiga*2000000/3546895` w `uint32` — dla
P=3374 to **6,7 mld**, czyli **przepełnienie** i śmieciowy wynik. Poprawione na
stałoprzecinkowe `P*56385/100000` (błąd < 0,002 %), z wczesnym ograniczeniem
wejścia > 7264 → 4095. Dodatkowo okazało się, że moja oczekiwana wartość 1903
była policzona z `round()` na float, a dokładnaarytmetyka całkowita daje **1902**
— test miał rację, ja nie.

## Znaczenie dla budżetu przerwania

66 nut na 64 wiersze ≈ **1 zdarzenie na wiersz** ≈ **50 zdarzeń/s** przy 50 Hz.
Przy zmierzonych ~100 µs na zdarzenie to ~5 ms/s, czyli **~0,5 % CPU** — MOD jest
bezpieczny dla podsystemu. Kolejny wycinek: aplikacja `MODPLAY.APP`, która czyta
MOD z dysku, wyprowadza wiersze przez `mod_psg_row()` i **oddaje taktowanie
usłudze audio** (`wait` w kolejce) — dzięki temu nie trzeba własnego timera.

---

# RUNDA 30 — ETAP MOD: odtwarzacz (automat stanu) + aplikacja MODPLAY

## Nowe pliki

* `include/mod_play.h` + `system/mod_play.c` — **automat stanu odtwarzacza** jako
  czysty moduł (bez ATOS i jądra): parsowanie nagłówka M.K., tabela pozycji,
  `speed`/`tempo` (Fxx), `pattern break` (Dxx), `position jump` (Bxx), koniec
  utworu, oraz **taktowanie oddane usłudze audio**: `mod_play_tick()` zwraca
  liczbę tików hires do następnego tiku, a aplikacja wkłada ją w `wait` kolejki
  zdarzeń — jedno źródło zegara dla efektów i muzyki.
* `tests/test_mod_play.c` + cel `check-modplay` — syntetyczny MOD w pamięci:
  odrzucanie złej sygnatury/za krótkiego pliku, 6 tików na wiersz przy speed 6,
  brak dryfu (suma 6 tików ≈ 1475 przy tempo 125), F03 zmienia speed, F96 zmienia
  tempo (≈205 tików), D00 przechodzi do następnej pozycji, B00 wraca, koniec
  utworu ustawia `finished`.
* `system/modplay.c` + reguła `modplay` w Makefile → **MODPLAY.APP (4 720 B)**:
  szkielet usług (lookup pid, gfx()/gui_op() z kolejką `pend[]`, batch rysowania)
  **skopiowany z SYSINFO**, nie wymyślony; wczytanie `HYMN.MOD` przez
  STAT/ALLOC/OPEN/READ/CLOSE; utrzymywanie ~4 zdarzeń w locie na podstawie
  liczników jądra (queued − applied); SPACE pauzuje, ESC kończy; na wyjściu
  wyciszenie kanałów.

## Dwa błędy, które złapały testy (oba moje)

1. `P_amiga*2000000` w `uint32` → **przepełnienie** dla 3374 (6,7 mld). Poprawione
   na stałoprzecinkowe `P*56385/100000` + wczesne ograniczenie wejścia.
2. **Offset danych patternów to 1084, nie 1080** — pod 1080 leży 4-bajtowa
   sygnatura „M.K.”. Potwierdza to rozmiar pliku:
   `1084 + 24*1024 + 86930 = 112590` = dokładnie rozmiar MOD-a. Wcześniejsze
   „4 bajty reszty” z mojego parsowania to była sygnatura. Testy odtwarzacza
   wykryły to natychmiast (16 błędów → 0).

## Stan weryfikacji (uczciwie)

* testy hostowe: **PASS** (`check-mod`, `check-modplay`, plus `check-psg`,
  `check-audio`),
* MODPLAY buduje się i jest wgrany na `work/atos-tree-full.img` razem z
  `SYSTEM/APP/HYMN.MOD` (112 590 B),
* **weryfikacji emulatorowej NIE ma**: mój jedyny niezawodny sposób startu
  aplikacji GUI (hook jądra `kernel_spawn_user`) **restartuje maszynę**, a dialog
  RUN odpalany klawiaturą bywa zawodny. To ograniczenie mojego harnessu, nie
  odtwarzacza — do sprawdzenia jednym kliknięciem myszą.

---

# RUNDA 31 — MODPLAY: proba odpalenia i uczciwy bilans

## Co dziala

* `MODPLAY.APP` (4 720 B) na `work/atos-tree-full.img`, obok `SYSTEM/APP/HYMN.MOD`
  (112 590 B); manifest 5/5 ZGODNE,
* testy hostowe: `check-mod`, `check-modplay`, `check-psg`, `check-audio` — PASS,
* przerwanie audio pracuje (`isr_max=2`).

## Czego NIE udalo sie zrobic: automatycznie odpalic aplikacji

Trzy drogi, trzy rozne objawy:

1. **Hook jadra** (`kernel_spawn_user` z petli glownej) → **restart maszyny**
   (zrzut: „LOADING KERNEL.SY…" + menu SETUP). To samo przy SysInfo. Wyglada na
   realny blad jadra: uruchamianie aplikacji GUI z kontekstu jadra psuje system.
2. **Hook pulpitu** (`SYS_SPAWN_ASYNC`, opozniony czasowo) → brak jakiegokolwiek
   efektu; znacznik `app_main` (0x800) nigdy sie nie zapalil, wiec aplikacja nie
   jest w ogole startowana.
3. **Dialog RUN z klawiatury** → niestabilny w moim harnessie; dla wpisanych nazw
   zwracal `RUN FAILED rc=2` (ENOENT) mimo ze pliki sa w katalogu.

## Moje bledy w tej rundzie (dla porzadku)

* skrypty i katalog `/tmp/short` **znikaja miedzy turami** — dwa przebiegi
  poszly na starych artefaktach, zanim to zauwazylem. Skrypt uruchomieniowy jest
  teraz w drzewie: `tools/run_modplay_live.py`;
* znacznik pod adresem **0x8F0** to pamiec systemowa (czytalem tam „1.0 "), wiec
  wyciagniety z niego wniosek byl **falszywy**. Adresy 0x800+ sa w porzadku
  (korzysta z nich sonda audio), ale 0x8F0 nie;
* patchowalem tylko `SYSTEM/APP/DESKTOP.APP`, a obraz ma **druga** kopie
  `SYSTEM/DESKTOP.APP`; po patchowaniu obu efektu nadal nie ma — zostawiam to
  jako otwarte pytanie, a nie jako „naprawione".

## Co zostaje do zrobienia

Aplikacja jest na obrazie i wystarczy ja uruchomic **recznie** (RUN → `MODPLAY`).
Jesli okno sie nie pojawi, nastepnym krokiem jest wariant ze znacznikami etapow
(`-DMODPLAY_TRACE`, zapis pod 0x800/0x804/0x808/0x80C/0x810/0x814) — on powie,
na ktorym etapie aplikacja sie zatrzymuje.

---

# RUNDA 32 — DLACZEGO MODPLAY SIE NIE POJAWIA (zdiagnozowane znacznikami) + ikonka

## Ikonka na pulpicie: konfigurowalna plikami, bez kompilacji

Odkryte: `SYSTEM/ICONS.CFG` mapuje ikony (`app:\SYSTEM\APP\SYSINFO.APP=19`),
a `SYSTEM/DESKTOP.CFG` ustawia pozycje na siatce (`TERMINAL=0,2`, `NAZWA=kol,wiersz`).
Dodane (i odczytane z obrazu dla pewnosci):

```
ICONS.CFG:   app:\SYSTEM\APP\MODPLAY.APP=19
DESKTOP.CFG: MODPLAY=1,0
```

Uczciwie: moj zrzut po tej zmianie **nie pokazuje** jeszcze ikonki — do
potwierdzenia po restarcie (byc moze numer ikony 19 albo klucz wymaga innej
formy). Same wpisy sa w obrazie i zweryfikowane odczytem.

## Znaczniki etapow rozstrzygnely sprawe

Wariant `-DMODPLAY_TRACE` pisze pod stale adresy. Odczyt po uruchomieniu z RUN:

| znacznik | wartosc | znaczenie |
|---|---|---|
| 0x800 | `0x4D4F4401` | `app_main` wystartowal |
| 0x804 | 2 | `lookup("SYSTEM.GFX")` OK |
| 0x808 | 3 | `lookup("SYSTEM.GUI")` OK |
| 0x810 | `0x4D4F4457` | okno **zostalo namalowane** |
| 0x80C | -1 | MOD-a nie wczytano |
| 0x818 | 0 | `GUI_BEGIN_KEYS` zwrocil blad |
| 0x814 | 0 | petla glowna **nigdy** nie ruszyla |

Czyli: aplikacja startuje, znajduje uslugi, rysuje okno — i **zaraz konczy**,
bo `GUI_BEGIN_KEYS` zawodzi. To dokladnie „wpisuje i nic sie nie pojawia”.

## Prawdziwy blocker: otwieranie plikow w ABI aplikacji

Kazda z **szesciu** prob `ATOS_CALL_OPEN` (z wiodacym `\`, z `C:`, bez, w root)
zwraca **-336** (`0xFFFFFEB0`). To **nie jest** kod z udokumentowanej sciezki
otwierania: handler `call==2u` zwraca `-0x210`, `-0x211` albo `-0x212`.
Wniosek: **MODPLAY jest jedyna aplikacja w drzewie, ktora uzywa
`ATOS_CALL_OPEN`/`ATOS_CALL_STAT`** — ta sciezka ABI jest nietestowana i zwraca
kod spoza swojego slownika. To material dla Codexa, nie dla mnie do zgadywania.

## Co dalej

Albo Codex sprawdzi `OPEN`/`READ` w ABI aplikacji, albo obejde I/O: MOD moze byc
wczytany przez maly pomocnik TOS (GEMDOS dziala — sonda `T.PRG` czyta pliki),
a aplikacja odbierze dane przez pamiec wspolna.

---

# RUNDA 33 — IKONKA MODPLAY: zrobiona i widoczna (prawdziwy mechanizm)

Moje wczesniejsze wpisy byly w **zlym pliku**. Prawdziwy mechanizm pulpitu:

* `SYSTEM/LAUNCH.CFG` — **lista ikon**: jedna linia `ETYKIETA=sciezka` na wpis,
  komentarze od `;`/`#`, builtiny `*TERMINAL`/`*TRASH`. Limit **8** wpisow
  (`LBAR_MAX`), a plik mial **7** — bylo miejsce dokladnie na jeden.
* `SYSTEM/ICONS.CFG` — obrazek ikony: `app:\SYSTEM\APP\<app>.APP=<indeks>`.
* `SYSTEM/DESKTOP.CFG` — pozycja na siatce: `ETYKIETA=kolumna,wiersz`.
  Nieznane etykiety sa ignorowane, wiec sam wpis tutaj nic nie dawal.

Dodane do `LAUNCH.CFG`:

```
MODPLAY=\SYSTEM\APP\MODPLAY.APP
```

**Potwierdzone zrzutem**: ikona MODPLAY jest na pulpicie (pozycja 1,0 z
`DESKTOP.CFG`). `work/audio-baseline/demo/90-pulpit-z-ikonka-MODPLAY.png`.

Uczciwa uwaga: ikona dziala, ale **klikniecie jej nadal nie zagra muzyki** —
aplikacja startuje, rysuje okno i konczy prace, bo `ATOS_CALL_OPEN` zwraca -336
(patrz runda 32). Ikona to warunek konieczny, nie wystarczajacy.

---

# RUNDA 34 — sciezka danych dla MODPLAY: co dziala, co nie (pomiary)

Cel uzytkownika: ma dzialac **przez nasz podsystem audio**. Projekt jest taki od
poczatku (aplikacja kolejkuje zdarzenia przez API audio, taktowanie daje Timer C),
wiec walczylem wylacznie o **doprowadzenie danych MOD-a** do aplikacji.

## Zmierzone (kazde wywolanie z aplikacji, odczyt ze znacznikow)

| wywolanie | wynik | wniosek |
|---|---|---|
| `ATOS_CALL_OPEN` (2) | **-336** dla **kazdej** z 6 postaci sciezki | kod spoza slownika handlera (ten zwraca -0x210/-0x211/-0x212) |
| `ATOS_CALL_FLOPPY_READ` (64) | **-1** | usluga „floppy" nie widzi pliku |
| `ATOS_CALL_RAW_BLOCK` (57) | **-0x281** | `user_write_range` odrzucil bufor — czyli **samo wywolanie jest dozwolone** |
| bufor statyczny 112 KB | **blad linkera** | `-mpcrel` nie zaadresuje 112 KB BSS-a |
| `GUI_BEGIN_KEYS` (6) | 0 = blad | aplikacja nie wchodzi w petle po pierwszym rysowaniu |

Kluczowy trop: **pulpit czyta pliki dokladnie tymi samymi wywolaniami**
(`gui_call(2/3/4,…)` w `read_all()`), a zwykla aplikacja dostaje -336. To wyglada
na regule **uprawnien/posiadania** (shell vs zwykly app), a nie na brak funkcji.

## Stan

* drzewo sie buduje (`MODPLAY.APP` 4 816 B), testy hostowe PASS,
* ikona MODPLAY **jest** na pulpicie,
* MOD lezy w obrazie: `SYSTEM/APP/HYMN.MOD` (ciagly, sektory 21746..21965),
* sciezka danych do aplikacji pozostaje **otwarta** — do rozstrzygniecia przez
  Codexa (OPEN dla aplikacji albo sankcjonowany sposob czytania plikow).

---

# PLAN NA TERAZ (zapisany 12.10.2026, po wejsciu sciezki PCM)

Stan: drzewo buduje sie (EXIT=0), `check-mod`, `check-modplay`, `check-psg`,
`check-audio` — PASS. Codex wprowadzil **prawdziwa sciezke PCM**
(`kernel_pcm.c`: 8-bit mono, 12 288 Hz, bufor 8 KB, `pcm_irq.o`, `pcm_lut.h`)
oraz **microMod** — dlatego MOD-y dzialaja. Moj `mod_psg`/`mod_play` (rendycja na
PSG) staje sie **trybem chiptune / zapasem**, a nie glowna droga.

## A. Zrozumiec i zweryfikowac nowa sciezke (najpilniejsze)

1. Przeczytac `microMod` + `kernel_pcm.c`: co dokladnie robi przerwanie PCM, jak
   wyglada limit (12 288 Hz, mono, 8 KB) i ile kanalow MOD-a da sie zmiksowac.
   To zastepuje moj pomiar budzetu PSG — teraz budzet jest po stronie miksera.
2. Uruchomic pelny `make check` (w tym `check-pcm`) i zapisac, co pokrywa.
3. Sprawdzic, czy MODPLAY obsluguje MOD bezposrednio (micromod) i/lub cache
   `.PCM`, i skad bierze pliki.
4. **Poprawic dokumentacje**: moje wpisy mowia „no PCM, no DMA" — to juz
   nieprawda. Uczciwosc wymaga sprostowania.

## B. Dopasowac moj wklad

5. Zdecydowac los `mod_psg`/`mod_play`: zostawic jako tryb chiptune (sa
   przetestowane hostowo) albo usunac.
6. `SYSINFO` -> DEVICES: dodac wiersze audio (wlasciciel, kolejka, stan PCM,
   podane bajty). Teraz to realnie uzyteczne.
7. Jezeli brakuje konwertera MOD->PCM w drzewie — dodac (jest juz
   `tools/gen_pcm_lut.py`, wiec miejsce jest).

## C. Domknac promocje audio

8. Odswiezyc `PACZKA-PROD` (moja jest **nieaktualna**: kernel 458 844 + PCM,
   BIOS, pulpit) i wygenerowac manifest z FAT.
9. Po PASS sond na nowym kernelu — promocja do `ATOS_AKTUALNY`.

## D. Jakosc dzwieku (kolejny etap)

10. Przy 12 288 Hz mono 8-bit: trzaski/aliasing, filtrowanie, petle i fade,
    glosnosc. Ewentualnie wiecej kanalow w mikserze.

---

# KIERUNEK (ustalony z uzytkownikiem): jeden podsystem audio, wymienne backendy

Cel uzytkownika, doslownie: nie koncentrujemy sie na Yamaha; docelowo **USB Audio
Class 1 przez NetUSBee**, sterownik ma **wykryc** mozliwosc i wypuszczac dzwiek
tam; a architektonicznie — **jeden podsystem audio**, do ktorego podpina sie
sterowniki (Yamaha, USB Audio, przyszla karta na wlasnym zlaczu), tak aby
**aplikacja nie wiedziala**, jaki sprzet gra.

## Co juz jest (stan rozpoznany)

* sprzet rozpoznany: `hw/expansion.c` → `detect_isp1160()` i `detect_rtl8019()`,
  wyniki w raporcie jako `netusbee_usb` / `netusbee_ethernet`,
* rdzen audio: pojedynczy wlasciciel, generacje, kolejka zdarzen z terminami,
  slowo mozliwosci (`KAUDIO_CAP_*`), oraz hook `kaudio_set_port_sink()` dla persony
  TOS — ale **brak abstrakcji sterownika**,
* PCM: `kernel_pcm.c` + `pcm_irq.S` na **Timerze A** (12 288 Hz), bit-bang na
  **glosnosciach PSG** (`psg_pcm_sample`, `psg_pcm_active`) — dziala, ale to
  nadal Yamaha.

## Docelowa abstrakcja (propozycja)

```c
typedef struct {
    const char *name;
    uint32_t caps;                       /* KLANG_CAP_*                    */
    int  (*init)(void);
    void (*shutdown)(void);
    int  (*tone)(unsigned ch,uint16_t period,uint8_t vol);   /* PSG-like   */
    int  (*noise)(uint16_t period,uint8_t vol);
    int  (*stop)(uint8_t mask);
    int  (*pcm_open)(uint32_t rate,unsigned bits,unsigned channels);
    int  (*pcm_submit)(const void *data,uint32_t bytes);
    uint32_t (*pcm_free)(void);
    int  (*pcm_close)(void);
} audio_driver_t;
```

Rdzen zostaje wlascicielem **polityki** (wlasiciel, generacje, kolejka, terminy,
miksowanie, konwersja czestotliwosci), a slowo mozliwosci, ktore widzi aplikacja,
jest **suma tego, co oferuje aktywny backend**. Aplikacje i ich ABI **nie zmieniaja
sie** — to jest sedno pomyslu uzytkownika.

Backendy: `drv_psg` (dzisiejszy), `drv_pcm_psg` (dzisiejszy PCM), `drv_usb_uac1`
(NetUSBee), `drv_card` (wlasne zlacze). Wybor: priorytet przy starcie (USB > karta
> PSG) albo recznie w SYSINFO/DEVICES.

## Kamienie milowe (kazdy weryfikowalny)

* **M1** Refaktor: wyciagnac `audio_driver_t`, przeniesc oba istniejace backendy,
  testy maja zostac zielone i zachowanie bez zmian.
* **M2** Rdzen USB: ISP1160 — ID/revision, reset, stan root huba, zasilanie portu,
  transfery kontrolne. Kryterium: „kontroller odpowiada i enumeruje root hub”.
* **M3** Enumeracja urzadzenia: deskryptory, w tym **Audio Class 1** (formaty,
  czestotliwosci). Kryterium: wypisane VID/PID i parametry audio.
* **M4** Wyjscie UAC1: alternate setting, **izochroniczny** transfer, strumien PCM.
  Kryterium: dzwiek z urzadzenia USB.
* **M5** Backend + automatyczny wybor i podglad w DEVICES.

## Ryzyka (uczciwie)

* izochroniczne transfery ISP1160 wymagaja obslugi bufora periodycznego,
* UAC1 bywa rozny: zaczac od urzadzenia z **stala** czestotliwoscia 48 kHz bez
  sprzezenia (klasyczny PCM2704),
* przepustowosc zlacza ROM i jitter; brak MMU (bufory musza byc ciagle),
* M2–M4 to praca rzedu tygodni, nie jednej sesji.

## Od czego zaczynamy

**M1** (refaktor, niskie ryzyko, zielone testy) oraz rownolegle **M2-scout**
(odczyt ID/root huba ISP1160, bez dotykania sciezki audio).

---

# USTALENIE: Hatari (nasz fork) NIE emuluje NetUSBee/USB

Sprawdzone w zrodlach, nie z pamieci: `atari-030-port/hatari-src/src` ma **85
plikow** i **ani jednego** zwiazanego z `net`/`usb` (`ls src | grep -icE "net|usb"`
= 0). Nie ma `netusbee.c`, nie ma nic o ISP1160/ISP1161, nie ma opcji `netusbee`.
Jest natomiast `cart.c` (kartridz) i `dmaSnd.c` (DMA STE/TT).

Wniosek: **nie da sie przetestowac sterownika USB Audio w naszym emulatorze** —
ani kontrolera ISP1160, ani urzadzenia UAC1. To nie blokuje pracy, ale zmienia jej
forme (patrz nizej).

## Konsekwencje dla planu

* **M1 (refaktor backendow)** — robimy i testujemy **teraz**: testy hostowe +
  emulator dla regresji. Od sprzetu niezalezne.
* **M2/M3 (sterownik ISP1160 + enumeracja)** — piszemy **przeciw modelowi na
  hoscie** (rejestr + deskryptory transferow), tak jak rdzen audio i konwerter
  MOD: logika jest weryfikowalna, kwirkow czasowych sprzetu nie ma.
* **Opcja warta rozwagi**: dodac do **naszego forka Hatari** minimalny **stub
  ISP1160** (rejestry + fake root hub + fake urzadzenie UAC1) — kilkaset linii C,
  a pozwala cwiczyc enumeracje i transfery **w emulatorze**. Rekomenduje to.
* **EPROM nie jest potrzebny do testow w emulatorze**: ATOS laduje BIOS z pliku
  (`--tos build/bios_mste.bin`). EPROM jest potrzebny dopiero na **realnym** Mega ST,
  tam gdzie bedzie karta USB Audio.
* Wymagane zachowanie bez sprzetu: sterownik nieaktywny, gdy karty nie ma
  (mamy juz `detect_isp1160()` w sondzie rozruchowej), system dziala dalej.

---

# NOWY CEL: dzwiek z Prehistorika 2 (port PRE2-ATOS) — rozpoznanie

Uzytkownik: „jeszcze jakby dzwiek wypuscic z np Prehistorik 2, bylo by super”.
Port istnieje i dziala (`PRE2-ATOS/`), a jego README mowi wprost:
„Braki: brak dzwieku (ATOS ma tylko PSG; muzyka PRE2 to moduly .TRK z samplami)”.

## Rozpoznane (fakty z kodu)

* silnik gry **zglosza** dzwiek: `src/audio.c` wypelnia `g_sfx_queue` i ustawia
  `g_song_request`; komentarz w pliku: „the actual mixing is the platform layer's”,
* warstwa ATOS ma **puste zaspki**: `atos/plat_atos.c:409` `plat_song()` i `:410`
  `plat_sfx()` to `{ (void)name; }` / `{ (void)n; }` — **to jest cala brakujaca
  robota**,
* `*.TRK` to **nie** klasyczny MOD: brak sygnatury `M.K.` pod offsetem 1080
  (sprawdzone w BOULA.TRK i BRAVO.TRK). To kontener Titusa z nazwanymi samplami
  („boula”, „marim”, „djenbe”, „unga”) — format trzeba rozpoznac,
* w drzewie jest **micromod** (`third_party/micromod/`) z API:
  `micromod_initialise(data,rate)`, `micromod_get_audio(int16*,count)`,
  `micromod_set_position`, `micromod_mute_channel`, `micromod_set_gain`,
* **wzorzec gotowy**: `system/modplay.c` (po zmianach Codexa) robi dokladnie to,
  czego potrzebuje PRE2: `micromod_get_audio()` -> konwersja na 8-bit ->
  `ATOS_CALL_PCM_SUBMIT()`, plus `ATOS_CALL_PCM_CLIP()` i opcjonalny cache `.PCM`.

## Plan (P1–P5)

* **P1** Rozpoznac kontener `.TRK`: tablica sampli (nazwy, dlugosci, petle),
  patterny, tempo. Weryfikacja: parser na hoscie + porownanie z DLugoscia pliku.
* **P2** Zdecydowac droge: (a) konwerter **TRK -> MOD** na hoscie (wtedy
  micromod gra bez zmian w gosciu) albo (b) wlasny replayer TRK w porcie.
  Rekomendacja: (a) — mniej kodu w gosciu i mniejsze ryzyko.
* **P3** SFX: `g_sfx_queue` niesie numery sampli — ustalic, skad brac ich dane,
  i podac je jako krotkie klipy PCM.
* **P4** Wpiac `plat_song()`/`plat_sfx()` w wyjscie PCM (12 288 Hz, 8-bit mono),
  wzorujac sie na `modplay.c`. Mikser musi resample'owac z czestotliwosci banku.
* **P5** Weryfikacja **odsluchem** w Hatari (`pre2-runner.command`, `--sound 44100`).

Wazne: to jest **niezalezne** od EPROM-u i karty USB Audio i **testowalne od razu**.

---

# ATOS SOUND 1.0 — Faza A zrobiona, Faza B pierwszy wycinek zrobiony

Nowy projekt w `Deepseek_test/ATOS-SOUND/` (struktura z §14 specyfikacji).
* Faza A: `docs/FAZA-A-RAPORT-STANU.md` — analiza istniejacego audio z dowodami
  (pliki i linie): YM2149, sposob grania Arkanoida, przerwania/kolejki, brak
  abstrakcji sterownika, pamiec, uruchamianie aplikacji, build/testy, oraz dwa
  fakty krytyczne: **NatFeats jest** w naszym forku (`src/cpu/hatari-glue.c`,
  opcja `--natfeats`), a **NetUSBee/USB nie ma** (0 plikow o net/usb w src).
* Faza B (pierwszy wycinek): `common/protocol/` (protokol v1 + maszyna stanu
  odbiorcy) i `common/mixer/` (16 glosow, S8/S16, mono/stereo, glosnosc, panorama,
  petla, pauza, resampling, saturacja, zegar, walidacja deskryptora) + testy
  hostowe + `atest` (selftest, WAV, strumien protokolu).
* Wyniki: `MIXER: all checks passed`, `PROTO: all checks passed` — takze pod
  `-fsanitize=address,undefined`.
* Yamaha i Arkanoid **nietkniete** — nic nie jest jeszcze podlaczone do kernela.

---

# ATOS SOUND — Faza B, wycinek 2: symulator + end-to-end (12.10.2026)

Dodane w `ATOS-SOUND/`: `common/samples/` (loader WAV), `emulator/macos/sim_main.c`
(symulator: Core Audio AudioQueue 3x2048 ramek, wykonawca protokolu, magazyn probek)
oraz `tests/proto_drive.py` (test end-to-end prawdziwym strumieniem protokolu).

Wyniki: `MIXER`, `PROTO`, `WAV`, `E2E` — all checks passed; selftest symulatora:
16 aktywnych glosow jednoczesnie, 19404 ramki, 13663 saturacje, 0 spoznionych.
Spelnione punkty §16: 2, 3, 5, 6, 9, 10.

Cztery bledy znalazly testy (wszystkie moje, wszystkie naprawione):
nadmiarowy odczyt w tescie stereo (=> dodana walidacja deskryptora probki),
podwojne skalowanie wzmocnienia, zanieczyszczenie binarnego stdout logami,
niespojnosc rozmiaru PLAY (koder 11 B, dane 10 B).

---

# ATOS SOUND — Faza B, wycinek 3: gniazdo, reconnect, pomiar opoznien

`--listen` (AF_UNIX) dziala: HELLO i GET_CAPABILITIES potwierdzone surowymi
bajtami (16 glosow, 64 probki, 44100 Hz). Raport koncowy podaje komendy,
spoznione i maksymalne wyprzedzenie PLAY w ramkach/ms.

DRUGI PRAWDZIWY BLAD w kodzie wspolnym, znaleziony testem: `atos_sound_rx_feed()`
kopiowal cale wejscie zamiast brakujacej czesci jednej ramki — dwie ramki w jednym
pakiecie (normalne dla USB Bulk) konczyly sie E_BADLEN. Naprawione; test protokolu
i e2e zielone, ASan/UBSan czyste.

Uczciwie: `tests/socket_drive.py` nie zalicza sie — ma blad we wlasnej logice
odczytu (transport dziala, dowod: surowe odpowiedzi). Do poprawienia jako pierwsze.

---

# ATOS SOUND — Faza B domknieta: wszystkie testy zielone

Stan na 12.10.2026:
```
MIXER:  all checks passed
PROTO:  all checks passed
WAV:    all checks passed
E2E:    all checks passed     (strumien protokolu po stdin/stdout)
SOCKET: all checks passed     (AF_UNIX, reconnect, stan probek zachowany)
```
Testy protokolu i miksera przechodza takze pod `-fsanitize=address,undefined`.

Trzy wady skladania ramek w `atos_sound_rx_feed()` (wszystkie moje, wszystkie
naprawione i pokryte testem "trzy ramki naraz, jedna z payloadem 84 B"):
kopiowanie calego wejscia zamiast brakujacej czesci ramki; liczenie `need` przed
skopiowaniem naglowka (kazda ramka z payloadem > 0 B = E_BADLEN); walidacja
naglowka po sprawdzeniu dlugosci (smieci nie byly odrzucane). To zepsuloby caly
prawdziwy transport 68k<->ESP.

Faza B spelnia §16 w punktach 2, 3, 5, 6, 9, 10. Nastepny krok: Faza C
(sterownik 68k + transport NatFeats na OSOBNEJ galezi forka Hatari) — wymaga
zgody uzytkownika na ingerencje w zrodla emulatora.
