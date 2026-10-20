# AUDIO w ATOS — ETAP 1: stan wyjściowy i pomiary odniesienia

Data: 2026-10-10. Wszystko poniżej jest **zmierzone lub odczytane z drzewa**,
nie założone.

---

## 1. Wersje i artefakty stanu wyjściowego

Obraz domyślny (po rundzie Codexa: nowy boot + szybki Flashback):

| artefakt | rozmiar | SHA-256 (pierwsze 20 znaków) | data |
|---|---|---|---|
| `work/atos-v2.img` | 805 306 880 | `fd98af241ace182fdbfb` | 10-10 14:40 |
| `build/bios_mste.bin` | 262 144 | `6a10dfaaf850e82df55f` | 10-10 13:38 |
| `build/system/KERNEL.SYS` | 450 690 | `8386ec7165a7474fffe3` | 10-10 14:34 |
| `build/system/DESKTOP.APP` | 29 626 | `cf0e84338bbbe426837a` | 10-10 13:35 |
| `apps/flashback/build/FLASHBK.APP` | 260 168 | `c3b6eb537cc0242a5e8f` | 10-10 14:38 |

**Kopia obrazu przed zmianami** (klon APFS, 0,004 s):
`work/audio-baseline/atos-v2-before-audio.img` (805 306 880 B).

Uwaga o parowaniu: **nowy ROM musi iść z nowym obrazem** (`docs/BOOT-20261010.txt`).
Sam dysk nie zmieni ekranu rysowanego przez stary ROM.

## 2. Konfiguracja emulatora — dźwięk był WYŁĄCZONY

`atos2.cfg`:

```
[Sound]
bEnableSound = FALSE
bEnableSoundSync = FALSE
nPlaybackFreq = 44100
```

oraz `run-atos2.sh` podaje dodatkowo `--sound off`.

Czyli: dotychczasowe przebiegi (i wszystkie pomiary w tej sesji) były **bez
dźwięku** — dokładnie jak zakłada plan. Do testów audio trzeba to włączyć
(`bEnableSound = TRUE` albo `--sound on`) i **odnotować w pomiarach**, bo
włączony dźwięk w Hatari sam obciąża host, a nie emulowaną maszynę.
`run-tf536.sh` i `run-flashback.sh` nie podają `--sound`, więc dziedziczą
ustawienie z configu.

## 3. Pomiary odniesienia (bez dźwięku)

| program | wynik | jak zmierzone |
|---|---|---|
| **Arkanoid** | **+23,5 fps** ogólnie (pary 15,6–29,2 fps), symulacja +26,3/s przez 30,9 s | `atos2-tools/measure_arkanoid.py` na **kopii** obrazu; liczniki gry `0x800` (klatki) i `0x804` (kroki symulacji) |
| **Flashback** | **29,80 fps** (28,70–30,68, 9 próbek ~9 s) | licznik klatek silnika, pomiar Codexa (`docs/FLASHBACK-START-20261010.txt`), pierwsza plansza |

Rozrzut Arkanoida (16–29 fps) odpowiada stanom gry (bonus/„power"/pauza zmieniają
liczbę rysowanych elementów) — do porównań „przed/po" używać tej samej sekwencji
i raportować zakres, nie jedną liczbę.

## 4. Kto dziś dotyka PSG

**Jedyne miejsce w całym drzewie**: `hw/fdc.c`, funkcja `fdc_select()` —
PSG **rejestr 14** (port A), bity 0–2 = napęd A/B i strona.

```
REG8(0xffff8800)=14; old=REG8(0xffff8800); REG8(0xffff8802)=(old&0xf8)|(n&7);
```

To jest dokładnie ten konflikt z planu: **rejestr wyboru PSG jest stanowy**,
więc wybór rejestru i transfer danych muszą być jedną operacją atomową.
Stan wyjściowy: **nikt tego nie zapewniał** — a od etapu 2 dochodzi drugi
użytkownik (audio).

Inne miejsca dotykające PSG: **brak** (sprawdzone: `0xff8800`, `0xff8802`).


> **AKTUALIZACJA 2026-10-11 (przegląd audio / audyt Codexa, pkt 9f).**
> Poniższe stwierdzenie jest **historyczne i nieaktualne**: w jądrze ATOS
> **istnieje** obsługa przerwań okresowych. ISR `kernel_timer_c`
> (`system/kernel_entry.S:330`, wektor 0x45) woła `irq_dispatch(IRQ_SRC_MFP_C)`,
> a warstwa `system/irq.c` ma 4 sloty na źródło. Timer C chodzi z częstotliwością
> zmierzoną w stanie ustalonym na **~50–60 Hz** (58–61 Hz w dwóch oknach
> pomiarowych) i to on napędza interpretację `Dosound`.
> Sekcje o „braku IRQ w jądrze" traktować jako opis stanu sprzed 2026-10-10.

## 5. Właściciele timerów i przerwań

| timer / źródło | kto używa | jak |
|---|---|---|
| **MFP Timer C** | `main.c` (opóźnienia i oczekiwanie na klawisz w BIOS-ie) | programuje `TCDCR`/`TCDR`, **zapisuje i przywraca** oryginał |
| **MFP Timer C** | `hw/vofa_rom.c` (sonda karty graficznej) | to samo: zapis/odtworzenie `TCDCR`/`TCDR` |
| **MFP Timer C/D** | `tests/test_irq.c` (tylko test) | `irq_register(IRQ_SRC_MFP_C, …)`, limit 4 slotów |
| **VBL** | `system/irq.c` `IRQ_SRC_VBL` | podsystem przerwań z rejestracją handlerów |
| **MFP GPIP** | `hw/shifter.c` (bit 7, detekcja mono), `hw/acsi.c` (bit 5), `system/kernel_block.c` (`0xfffffa01` bit 5) | odczyt stanu, nie przerwania |
| **`_hz_200`** | `system/tos_prg.c` (`tos_mfp_ticks`, 200 Hz) | *modelowany* licznik dla persony TOS, nie sprzętowy |

Wniosek dla etapu 5 (plan): **Timer C jest wolny dla audio** tylko o tyle, o ile
nikt go nie trzyma w danym momencie. Dziś używa go BIOS (opóźnienia) z
zapisem/odtworzeniem stanu, więc audio może po starcie przejąć Timer C, ale
**musi** to robić przez `system/irq.c` (albo świadomie obok niego), a nie
programować MFP bezpośrednio. To jest zapisane jako element etapu 5, nie 2–4.

## 6. Co wynika dla etapów 2–4

1. **Etap 3 jest już zrobiony** (patrz niżej) — bo bez niego każdy test audio
   byłby nieważny: dwa użytkownicy PSG bez atomowości psują i dźwięk, i napęd.
2. **Etap 2 (usługa AUDIO)**: w jądrze są już usługi (`ATOS_SERVICE_GET` = 40,
   rejestracja/zapytanie o pid) i wersja ABI; wzorzec do naśladowania:
   `SYSTEM.GFX` (usługa w jądrze + mostek komunikatów). Audio ma iść tą samą
   drogą: deklaracja możliwości, właściciel = pid/sesja, zwolnienie przy
   wyjściu/błędzie/zabiciu.
3. **Etap 4 (Arkanoid)**: logika jest w **`system/game.c`** (131 linii), a nie w
   `arkanoid.c` (336 linii, rysowanie). Hooki zdarzeń są jednoznaczne:
   zniszczenie cegły `game.c:52`, zrzut bonusu `game.c:53`, odbicie od paletki
   `game.c:58` (`bounce_paddle`), odbicie od ściany `game.c:96`, wygrany poziom
   `game.c:25` (`state=ARK_WON`). Kolejka zdarzeń ma powstać **w logice**, a nie
   w pętli odmalowania (`arkanoid.c:243-244` robi pełny przerys — plan wprost
   tego zabrania).

## 7. Zmiany wykonane w etapie 1–3 (stan na teraz)

| plik | zmiana |
|---|---|
| `include/psg.h` | **nowy** — rejestry PSG, API `psg_read/psg_write/psg_port_a_modify`, jawne stałe 7/14/15 |
| `hw/psg.c` | **nowy** — atomowy dostęp (sekcja krytyczna `%sr`/`ori #0x0700`), odmowa zapisu rejestrów 7/14/15 z licznikiem, atomowy read-modify-write portu A |
| `hw/fdc.c` | `fdc_select()` przechodzi przez `psg_port_a_modify()` zamiast pisać do PSG wprost |
| `Makefile` | `build/hw/psg.o` dopisany do listy obiektów |

Build: `make TOS=1 -j4` → **EXIT=0** z `-Werror`. `psg_*` jest w
`build/bios_mste.elf` (tam żyje FDC), czego oczekiwano.

## 8. Czego jeszcze NIE ma (uczciwie)

* usługi AUDIO w jądrze (etap 2) — brak,
* kolejek i sterownika PSG dla audio (etap 2) — brak,
* zdarzeń dźwiękowych w Arkanoidzie (etap 4) — brak,
* testu PSG w `make check` — brak (następny krok),
* dźwięku włączonego w emulatorze — świadomie wyłączony, włączymy przy testach.

---

## 9. Stan testów na starcie (ważne dla audytu)

`make TOS=1 check` **nie jest zielone** na nowym obrazie domyślnym — niezależnie
od moich zmian:

```
make check-console
cc ... tests/test_kernel_console.c system/text_terminal.c -o build/tests/test_kernel_console
build/tests/test_kernel_console
Assertion failed: (!memcmp(&console,&expected,sizeof(console))), function main,
                  file test_kernel_console.c, line 63.
make: *** [check-console] Abort trap: 6
```

Ustalenia:

* pada **tak samo z `TOS=1` i bez** niego,
* test linkuje **wyłącznie** `tests/test_kernel_console.c` (który włącza
  `system/kernel_console.c`) oraz `system/text_terminal.c` — **nie ma żadnej
  ścieżki zależności** do `hw/psg.c` ani `hw/fdc.c`, więc moje zmiany z etapu 3
  nie mogą na to wpływać (dowód konstrukcyjny),
* `system/kernel_console.c` i `tests/test_kernel_console.c` mają datę
  **10-10 13:38** — czyli oba zmienione w rundzie Codexa,
* konsola **też** używa `term_write()` (`kernel_console.c:178`), więc to nie
  tabulatory; różnicę robi **nowa ścieżka wiersza statusu**
  (`kernel_console.c:146-174`, `kconsole_status_row`/`status_label_done`), która
  przechwytuje `kconsole_puts()` i pisze do komórki statusu zamiast przez
  `term_write()`. Test (pisany w tej samej rundzie) tej ścieżki w swojej
  sekwencji nie uwzględnia.

**To jest obszar boot/konsoli Codexa, nie audio** — zgłaszam, ale nie zmieniam,
żeby nie kolidować z ich pracą. Wszystkie pozostałe cele C przechodzą:

```
make TOS=1 check-mono-font check-gui-damage check-trash check-fat-write \
     check-block-cache check-graphics check-wm-core check-wm-blit \
     check-gui-apps check-psg check-irq check-app-abi check-game check-sessions
REST=0
```

W tym **nowy `check-psg`** (16/16 ✓) dopisany do `check` w `Makefile`.

## 10. Kolejność dalszej pracy (etapy 2 i 4)

1. **Etap 2** — usługa `SYSTEM.AUDIO` w jądrze: możliwości, właściciel
   (pid/sesja), kolejka, zatrzymanie przy wyjściu/błędzie/zabiciu, brak alokacji
   i plików w przerwaniu; sterownik PSG przez `hw/psg.c`; opcjonalnie Timer C
   przez `system/irq.c`.
2. **Etap 4** — `system/game.c`: kolejka zdarzeń w logice (cegła `:52`, bonus
   `:53`, paletka `:58`, ściana `:96`, poziom `:25`), wysyłka do usługi,
   **nigdy** z pętli odmalowania.
3. Pomiary po: ten sam fragment Arkanoida (`measure_arkanoid.py`) i Flashbacka,
   **z włączonym dźwiękiem** w `atos2.cfg`, żeby porównanie było uczciwe.
