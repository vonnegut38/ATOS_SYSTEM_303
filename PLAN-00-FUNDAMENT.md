# PLAN-00 — FUNDAMENT PROJEKTU (wersja 2.0)

**Status:** dokument kanoniczny (single source of truth).
**Zastępuje jako źródło prawdy:** PLAN-BIOS-MEGA-ST.md, PLAN-EMULACJA-TOS.md,
PLAN-G1-MULTITASKING.md, PLAN-MINT.md, PLAN-NAKLADKA-EMUTOS.md, T2-DESKTOP.md,
analiza-systemu-atari.md. Tamte pliki ZOSTAJĄ w repo jako archiwum ustaleń —
nie są już wiążące.

**Zmiana w 2.0:** domknięte decyzje (własny system oparty na EmuTOS, AES/VDI
z EmuTOS, priorytet = aplikacje TOS w oknach) oraz dopisana specyfikacja BIOS-u
graficznego, dysków, układu C:\SYSTEM i wymagań wydajności/wyglądu.

---

## 1. Wizja

Własny system dla Atari Mega ST, który **emuluje TOS** i jest oparty na źródłach
**EmuTOS**, ale jest zaprojektowany od nowa wokół **okien i wielozadaniowości**:

- natywne aplikacje TOS uruchamiają się **każda we własnym oknie**,
- można **przełączać się** między działającymi aplikacjami,
- awaria jednej aplikacji **nie przewraca** systemu ani innych okien,
- wygląd jest **nowoczesny i szybki**, a zgodność z TOS na tyle pełna, że nie
  wymaga rekompilacji programów.

---

## 2. Konfiguracja docelowa (potwierdzona)

| Element | Wartość |
|---|---|
| Komputer | **Mega ST4** — 4 MB ST-RAM, bez blittera |
| CPU | **TF536 @ 50 MHz** (MC68030) |
| Pamięć | 4 MB ST-RAM + **64 MB ALT-RAM** @ 0x01000000 (dodaje TF536) |
| Dysk | **IDE na TF536** (auto-sonda 0x00F00000 / 0xFFF00000); ACSI historycznie |
| Grafika | **karta ET4000 przez interfejs VOFA** w złączu **MegaBus**; VRAM 0x00C00000, rejestry 0x00D00000, I/O 8-bit |
| Docelowy chip | ET4000 → **ET4000/W32i** (blitter 2D tylko przy W32i) |
| ROM | **256 KB @ 0xE00000** (192 KB @ 0xFC0000 tylko awaryjnie) |
| Monitor | VGA |

Warianty TF534 @ 32 MHz i STGA nie są targetem.

---

## 3. Priorytety (jak rozstrzygamy spory projektowe)

1. **Szybkość** — nie ma prawa być wolno; żaden mechanizm nie może rysować
   całego pulpitu, gdy zmienia się fragment.
2. **Bezpieczeństwo i stabilność** — błąd aplikacji kończy się zamknięciem jej
   okna, nigdy zawieszeniem systemu.
3. **Kompatybilność z aplikacjami TOS** — bez rekompilacji, w zakresie L0–L3.
4. **Nowoczesny wygląd** — ważny, ale **nigdy kosztem punktów 1–3**.

Zasada nadrzędna: wygląd nie może łamać szybkości, a szybkość nie może łamać
bezpieczeństwa ani podstawowej kompatybilności.

---

## 4. Baza: źródła EmuTOS

- Warstwę zgodności TOS budujemy **na źródłach EmuTOS** (GPL-2.0): GEMDOS, BIOS,
  XBIOS, Line-A, AES, VDI, zasoby GEM, fonty systemowe.
- Nasz wkład własny: **jądro** (procesy, MMU, IPC, pamięć), **menedżer okien
  i kompozytor**, **sterowniki ET4000/W32i i dysków**, **graficzny BIOS**.
- EmuTOS AES jest z natury jednowątkowy (jedno okno foreground). Przerabiamy go
  na **serwer wieloklientowy** z kolejką żądań i kolejką zdarzeń per klient —
  wzorzec znany z XaAES.
- Całość zgodna z GPL-2.0: publikujemy źródła i noty pochodzenia.

---

## 5. Architektura warstwowa

    +---------------------------------------------------------------+
    | aplikacje TOS (GEM i niskopoziomowe)  |  aplikacje natywne     |
    +---------------------------------------------------------------+
    | AES (wieloklientowy) + VDI + menedżer okien + kompozytor       |
    +---------------------------------------------------------------+
    | JĄDRO: scheduler, procesy, MMU 030, IPC, VFS, sterowniki       |
    | Persona TOS: GEMDOS / BIOS / XBIOS / Line-A, basepage, trapy   |
    +---------------------------------------------------------------+
    | BIOS graficzny: POST, Setup, detekcja, boot manager, loader    |
    +---------------------------------------------------------------+
    | SPRZĘT: Mega ST4 + TF536 (030/ALT-RAM/IDE) + VOFA ET4000 + VGA |
    +---------------------------------------------------------------+

Decyzje warstwowe:
- BIOS = firmware (POST + Setup + abstrakcja sprzętu + boot manager), nie OS.
- jądro = jedyny właściciel CPU, pamięci i przerwań; AES/VDI i GUI to usługi.
- zgodność realizujemy jako **emulację API** (basepage, trapy #1/#2/#13/#14,
  Line-A, VBR per proces), a nie emulację sprzętu.

---

## 6. Aplikacje TOS w oknach

Dwie klasy programów, dwie drogi tej samej obietnicy „działa w oknie":

- **Aplikacje GEM/AES** — tworzą okna przez AES. Dzięki wieloklientowemu AES
  dostają prawdziwe, niezależne okna; system zarządza Z-order, fokusem,
  przenoszeniem i zamykaniem.
- **Aplikacje niskopoziomowe** (gry, dema, programy piszące wprost do ekranu) —
  dostają **prywatny bufor ekranu** w formacie trybu ST, mapowany pod adres,
  którego oczekują, plus przechwycone wejście. Kompozytor skaluje ten bufor do
  okna na ET4000. Działa też tryb pełnoekranowy dla programów wrażliwych na czas.

Przełączanie zadań: pasek zadań, kliknięcie okna, skrót typu Alt+Tab, lista
procesów. Okno nieaktywnej aplikacji nadal istnieje i odświeża się poprawnie.

Bezpieczeństwo: każdy proces ma **osobną przestrzeń adresową (MMU 030)**;
naruszenie ochrony → proces zostaje zakończony, okno znika, reszta systemu żyje.

---

## 7. Kompozytor i wydajność (wymóg „ma być szybko")

- **Tylko brudne prostokąty** — pełne odświeżenie ekranu jest zabronione
  (to był błąd poprzedniego projektu).
- **Bufor ekranu każdego okna w ALT-RAM** (szybka pamięć 030), VRAM tylko dla
  złożonego obrazu widocznego.
- **Aplikacje GEM rysują od razu w 8bpp chunky** przez VDI — znika konwersja
  planarnej grafiki ST; konwersja dotyczy wyłącznie programów niskopoziomowych.
- **Akceleracja sprzętowa W32i**: BitBlt, wypełnianie, color-expand; zawsze z
  fallbackiem CPU.
- Kursor: sprzętowy kursor karty albo małe zapamiętanie/tło — nigdy pełny redraw.
- **Zakaz ciężkiego rysowania w przerwaniu** (Timer C tylko odkłada pracę).
- Cel wydajnościowy: przeniesienie okna kosztuje O(powierzchni okna), a nie
  O(ekranu); brak migotania dzięki synchronizacji z VSYNC.

---

## 8. Nowoczesny wygląd

- **Silnik motywów** oparty na zasobach GEM (nie hardkodowane rysowanie).
- Nowoczesne elementy: pasek zadań, menu systemowe, ramki okien, subtelne cienie
  (cache'owane jako bitmapy, nie liczone co klatkę), spójna paleta.
- **Skalowalny, antyaliasingowany font** obok zgodnego 8×16 dla programów TOS.
- Animacje wyłącznie krótkie i ograniczone; nigdy nie blokują interakcji.
- Wygląd jest warstwą nad API — programy TOS go nie widzą i nie mogą go zepsuć.

---

## 9. Start systemu — BIOS graficzny (minimalny, bez okien)

BIOS to firmware z prostym interfejsem w stylu EmuTOS — **bez okien**. Rysowanie
jest **przyrostowe**: interfejs śledzi zmienione pola i odświeża tylko ich
obszar (brudne prostokąty), a nie cały ekran.

1. **Detekcja grafiki**: jeśli wykryto ET4000 (VOFA na MegaBus) → start od razu
   w VGA; jeśli karty nie ma → start na Shifterze (fallback), z zachowaniem
   pełnej funkcjonalności na tyle, ile pozwala Shifter.
2. Logo karty/systemu (~1 s), potem **POST** — wszystko od pierwszego piksela
   na wybranym wyjściu, bez migania przez Shifter.
3. **Graficzny Setup** — tylko to, co potrzebne:
   - **data i czas RTC** (odczyt i ustawienie),
   - **wybór urządzenia bootowania**: dysk ACSI albo **IDE na TF536**, oraz
     partycja,
   - **zapis ustawień** (trwały),
   - tryb obrazu i podstawowe opcje, test urządzeń.
4. **Lista wykrytych urządzeń**: IDE, ACSI oraz **port cartridge = NetUSBee**
   (sieć RTL8019 i USB ISP1160; pendrive USB może pojawić się jako dodatkowy,
   ostatni dysk).
5. **Boot**: wybór urządzenia → montaż partycji → odczyt C:\SYSTEM → załadowanie
   jądra i warstwy TOS → start usług.

Konfiguracja BIOS-u: **C:\BIOS\BIOS.CFG** na dysku bootującym (z opcją NVRAM).

---

## 10. Pamięć masowa i systemy plików

- Blokowo: **IDE na TF536** jako główny nośnik, ACSI jako obsługa historyczna.
  Cache zapisu z możliwością wyłączenia (write-through).
- **Partycje atarowskie**: AHDI (GEM / BGM / F32) oraz łańcuchy **XGM**.
- **FAT16** i **FAT32** (w tym długie nazwy, jeśli wydajność pozwoli).
- **Port cartridge = NetUSBee**: karta sieciowa RTL8019 (obsługa sieci) oraz
  kontroler USB ISP1160; dysk USB (pendrive) dołączany jako dodatkowy, ostatni
  dysk systemu.
- Tabela partycji AHDI i BPB walidowane przed użyciem; czytelne komunikaty
  o błędach zamiast cichych awarii.

---

## 11. Układ systemu na dysku bootującym

    C:\
      BIOS\      konfiguracja firmware (BIOS.CFG)
      SYSTEM\
        BOOT\      loader, obraz jądra, konfiguracja startowa
        KERNEL\    jądro i moduły (.SYS)
        DRIVERS\   sterowniki: grafika, dysk, wejście, sieć
        TOS\       warstwa zgodności TOS (GEMDOS/BIOS/XBIOS/AES/VDI),
                   zasoby GEM, fonty systemowe
        APPS\      aplikacje dołączone (pulpit, menedżer plików, edytor,
                   terminal, menedżer zadań)
        CONFIG\    BIOS_CFG, system.cfg, konfiguracja motywu
        THEMES\    motywy graficzne
        FONTS\     fonty
        AUTO\      programy uruchamiane przy starcie
        TEMP\

Dysk C: to partycja bootująca wybranego urządzenia; system zawsze szuka
katalogu SYSTEM w korzeniu tej partycji.

---

## 12. Bezpieczeństwo i stabilność

- MMU 68030: tryb użytkownika i nadzorcy, osobne przestrzenie adresowe, pamięć
  jądra niedostępna dla aplikacji.
- Każde wywołanie systemowe walidowane; żadnych wskaźników użytkownika
  przekazywanych wprost do sprzętu.
- Własność zasobów (okna, pamięć, uchwyty) przypisana do procesu; zakończenie
  procesu zwalnia wszystko deterministycznie.
- Brak dostępu aplikacji do rejestrów sprzętowych poza emulowanym API.
- Nadzór nad aplikacjami, które nie oddają sterowania; bezpieczna konsola
  ratunkowa.
- Awaria aplikacji = zamknięcie okna, nie reset systemu.

---

## 13. Zgodność TOS — poziomy

| Poziom | Zakres | Skutek |
|---|---|---|
| L0 | GEMDOS: pliki, procesy (Pexec), pamięć | programy konsolowe .PRG działają |
| L1 | BIOS / XBIOS / Line-A | gry i dema korzystające z biosu ekranu/klawiatury |
| L2 | VDI | programy GEM rysują przez VDI |
| L3 | AES (okna, zdarzenia, menu, zasoby) | niezmodyfikowane aplikacje GEM w oknach |

---

## 14. Kamienie milowe

**M0 — porządek i punkt odniesienia** — zrobione
- `gmake all` odtwarzalny bit-for-bit (92/92 artefakty identyczne po clean),
  `DESKTEST.APP` identyczny, testy check / check-graphics / check-wm /
  check-gui-apps zielone.
- Znalezisko: nieaktualne nazwy celów CTXFAULT/YIELDTEST w build/ (dziś
  CTXFLT/YIELD).
- Do domknięcia: wersjonowanie (git), resztki rozjazdu README vs kod.

**M1 — BIOS graficzny**
- detekcja ET4000 → VGA, fallback Shifter, POST, logo, Setup (zegar, wybór
  urządzenia bootowania, lista urządzeń z cartridge, tryb obrazu), zapis configu.
- Kryterium: boot pokazuje POST i Setup na ET4000; bez karty poprawnie startuje
  na Shifterze.

**M2 — dyski i system plików**
- IDE TF536 + ACSI, partycje AHDI (GEM/BGM/F32/XGM), FAT16, FAT32.
- Kryterium: montaż C:, odczyt i zapis plików, boot z wybranego urządzenia.

**M3 — jądro i bezpieczeństwo**
- 030 MMU, procesy, wywłaszczanie, izolacja, IPC, własność zasobów.
- Kryterium: dwa procesy w osobnych przestrzeniach; celowe wykroczenie jednego
  nie rusza drugiego.

**M4 — warstwa TOS L0–L1**
- GEMDOS/BIOS/XBIOS/Line-A, trapy, basepage, fault-recovery.
- Kryterium: zestaw .PRG (w tym zewnętrznych z C:) uruchamia się w procesie.

**M5 — VDI + kompozytor**
- VDI 8bpp chunky, brudne prostokąty, bufory okien, VSYNC, kursor.
- Kryterium: brak pełnego redraw; przenoszenie okna bez migotania, mierzalnie
  szybkie.

**M6 — AES wieloklientowy + menedżer okien**
- okna/zdarzenia/menu/zasoby, pasek zadań, przełączanie aplikacji.
- Kryterium: dwie niezmodyfikowane aplikacje GEM działają równolegle w oknach,
  przełączanie nie gubi stanu.

**M7 — programy niskopoziomowe w oknach**
- prywatny ekran ST + kompozycja do okna, przechwycenie wejścia, pełny ekran.
- Kryterium: gra/demo uruchomione w oknie, możliwość przełączenia.

**M8 — nowoczesny wygląd**
- silnik motywów, skalowalny font, cienie, spójny pulpit.
- Kryterium: spójny motyw na wszystkich oknach bez spadku płynności.

**M9 — akceleracja W32i i tryby kolorowe**
- BitBlt/patfill/color-expand, 16/256 kolorów, 800×600, 1024×768.

**M10 — realny sprzęt**
- Mega ST4 + TF536 + VOFA, EPROM, dłuższa sesja stabilności.

---

## 15. Środowisko pracy

- Emulator: nasz fork Hatari (--tf536, --graphics-card vofa, --gfx-chip et4000).
  Przykład:
      atari-030-port/hatari-tf536 --machine megast --cpulevel 3 --cpuclock 32
        --memsize 4 --ttram 64 --tf536 on --graphics-card vofa --gfx-chip et4000
        --mono --zoom 2 --tos atari-030-port/disk/emutos-pl-256k.img
- Build: gmake (make 3.81 nie obsługuje celów &:) + m68k-elf-gcc + Pythony z
  atari-030-port/tools/py.
- Pętla: zmiana → test w Hatari → zrzut/wniosek. Nigdy „na raz".
- Jedna instancja Hatari na ten sam obraz dysku.

---

## 16. Co zostaje z dotychczasowej pracy

| Zasób | Rola |
|---|---|
| files-pasted-by-the-user-projekt | baza implementacji: BIOS, jądro, grafika, desktop, import EmuTOS VDI |
| atari-030-port/hatari-src + hatari-tf536 | emulator z TF536/VOFA |
| atari-030-port/tools | obrazy AHDI/FAT/Minix, skrypty zrzutów |
| atari-030-port/disk/emutos-*.img | źródło warstwy TOS / referencja |
| stare PLAN-*.md | archiwum ustaleń |

---

## 17. Rozstrzygnięcia i pytania otwarte

**Rozstrzygnięte:**
- Własny system **oparty na źródłach EmuTOS** (GPL-2.0), nie ścieżka MiNT.
- AES/VDI **ze źródeł EmuTOS**, przerobione na wieloklientowość; własny WM
  i kompozytor.
- Priorytet: **natywne aplikacje TOS w oknach** z przełączaniem.
- Start: **ET4000, gdy wykryta; inaczej Shifter**.
- System na **C:SYSTEM** z podkatalogami BOOT/KERNEL/DRIVERS/TOS/APPS/CONFIG/
  THEMES/FONTS/AUTO/TEMP.
- Konfiguracja docelowa: Mega ST4 + TF536 @ 50 MHz + IDE + VOFA ET4000 + VGA.

**Otwarte (do decyzji później):**
1. Długie nazwy plików w FAT32 — pełne LFN czy tylko 8.3 (wpływ na VFS i pamięć).
2. Dźwięk (YM2149 / DMA) — czy wchodzi do zakresu i na którym etapie.
3. Sieć (NetUSBee / inna) — czy i kiedy.
4. Format zapisu konfiguracji BIOS-u: plik na dysku, NVRAM, czy oba.

---

## 18. Zasady inżynierskie

1. Zmiana → test w Hatari → wniosek. Nigdy dużo na raz.
2. „Raz działa, raz nie" → najpierw surowe bajty, potem zmiana.
3. Nigdy ciężka praca w przerwaniu (Timer C).
4. Nie robić zapisów szerszych niż szyna (MegaBus 16-bit) bez testu.
5. BSS i duże tablice oszczędnie (pamięć psuła zagnieżdżony Pexec).
6. Identyczne zrzuty = zamrożony ekran, nie „program skończył rysować".
7. Każdy zrzut/test zapisujemy z datą i komendą, którą go odtworzyć.
