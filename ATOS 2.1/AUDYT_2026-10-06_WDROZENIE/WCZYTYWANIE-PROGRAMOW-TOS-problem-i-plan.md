# WCZYTYWANIE PROGRAMÓW TOS W ATOS — problem i propozycja rozwiązania

Dokument do analizy. Stan na 2026-10-09. Wszystkie liczby i adresy pochodzą
z przebiegów i odczytów wykonanych w tej sesji, nie z dokumentacji.

---

## 1. Stan obecny: ATOS ma DWA niezależne mechanizmy uruchamiania kodu TOS

### Ścieżka A — „persona" w jądrze (`system/tos_prg.c`, ~4500 linii)

Program `.PRG` jest ładowany **przez jądro ATOS** do TPA w TT-RAM, dostaje
basepage i stos, i wykonuje się **wewnątrz sesji ATOS** (w oknie, z pulpitem
pod spodem). Jądro udaje TOS: przechwytuje `trap #1/#2/#13/#14` i odpowiada
własnymi implementacjami GEMDOS/BIOS/XBIOS/AES/VDI napisanymi w C.

Zmierzone na tym: LDW Power działa (okno, menu, dialog), KOBOLD 3.51 startuje
i wykonuje 122 wywołania GEM, sonda katalogowa czyta katalog standardowym API.

### Ścieżka B — loader obrazu TOS w BIOS-ie (`loader/tos.c`, `boot/ramjump.S`)

Cały obraz **TOS/EmuTOS** jest ładowany i **maszyna przechodzi w niego** —
ATOS przestaje istnieć (albo wraca dopiero po resecie). To jest ścieżka, którą
odblokowywałem w ostatnich rundach.

---

## 2. Problem właściwy — trzy różne problemy, często mylone

### 2.1 Persona nigdy nie będzie kompletna (problemy zbierane „po programie")

Każdy program odsłania **inny** podzbiór brakujących kontraktów. Zmierzone
w tej sesji:

| program | co odsłonił |
|---|---|
| LDW Power | `wind_get`/`wind_set` (złe numery pól AES), `Pterm` (kod czytany z `usp` zamiast `usp+2`), `evnt_*` |
| KOBOLD 3.51 | 14 nieobsłużonych wywołań: VDI `vqin_mode` (115), AES `appl_find` (13), `appl_write` (12), **BIOS `Setexc` (5)** zwracające 0, zapytania 130 |
| sonda katalogowa | `Fsfirst` **limit 16 wpisów**, ignorowany `attr`, brak `Dsetpath`, brak `Fdatime` |
| analiza statyczna | `rsrc_load` nie istnieje, `ap_ptree` nigdy nie ustawiane, `form_center` liczy tylko obiekt 0, `vq_mouse`/`v_get_pixel` zwracają 0 |

Zbiory te **nie pokrywają się**. Koszt rośnie jak **O(programy × API)**, a każdy
nowy program to kolejna runda „ślad → poprawka → pomiar". To nie jest droga do
kompatybilności — to gaszenie pożarów. Poprawki same w sobie są dobre (i nie
idą na marne, patrz §5), ale **nie mogą być strategią**.

### 2.2 Loader obrazu TOS — łańcuch blokerów (wszystkie już rozwiązane)

| # | bloker | co było naprawdę | stan |
|---|---|---|---|
| 1 | „BLOKER toolchain" (`bios.c` → `can't resolve 0 - sp`) | nie brak toolchainu, tylko **brak `ELF=1`**; EmuTOS sam ustawia wtedy `m68k-elf-` + `-Wa,--register-prefix-optional` (Makefile:205-213, `doc/old_changelog.txt:4276`) | **rozwiązane**, `make 256 ELF=1` → EXIT=0 |
| 2 | brak obrazu ładowalnego w RAM | EmuTOS ma na to **oficjalny cel**: `TARGET_PRG` → `EMUTOS_LIVES_IN_RAM=1` (`include/config.h:34-40`), obraz linkuje się pod `0x2140` | **rozwiązane** |
| 3 | loader nie znał nagłówka TOS | nasz `add_image` czytał pola jak w PRG GEMDOS; dodane rozpoznanie `BRA/version/ENTRY/base` | **rozwiązane** |
| 4 | konfiguracja BIOS-u nie wczytywana | plik musi leżeć w **`\BIOS\BIOS.CFG`** — kopia w katalogu głównym nie wystarcza; dodatkowo `install_boot_sys.py` bierze nazwę pliku źródłowego | **rozwiązane**: `g_config_loaded=1`, `auto_tos=1` |
| 5 | **Bus Error przy starcie EmuTOS** | patrz §2.3 — **otwarte** | ← tu jesteśmy |

### 2.3 Co konkretnie nie działa teraz

Po odblokowaniu 1–4 pętla `main.c:118-126` faktycznie startuje obraz:

```
auto_tos=1, boot_id=128 (IDE master)
Panic: Bus Error   pc=0x0015fba   addr=0x00e0012c
```

Analiza adresów: `__bss_end` BIOS-u to `0x3a70`, więc PC **nie jest w BIOS-ie**;
obraz EmuTOS ładuje się pod `base=0x2140` i zajmuje `0x2140..0x041BE`;
`0x15fba − 0x2140 = 0x13E7A` → **PC jest ~90 KB w głąb obrazu EmuTOS-a**.
Wniosek (mocne wnioskowanie, nie dowód wprost): EmuTOS **został załadowany
i zaczął się wykonywać**, i po kilkudziesięciu KB własnego kodu trafił na
Bus Error. `addr=0x00e0012c` leży w obszarze ROM-u, w którym siedzi nasz BIOS.

### 2.4 Problem trzeci, najgłębszy: brak izolacji (przyjęte za Waszym audytem)

Gość startuje w **rzeczywistym trybie supervisor** (`system/tos_entry.S:51`,
`SR=0x2500`, S=1, IPL=5). Może zmienić IPL, VBR, rejestry MMU i dowolny rejestr
MMIO. **Żadna tabela uprawnień ani mutex tego nie zatrzyma.** Profile A/B
oznaczają więc na tym etapie **zasady współpracy, nie izolację** — i tak trzeba
je opisywać. To nie jest do naprawienia jedną poprawką; izolacja to osobny
projekt (gość w user mode + programowa obsługa oczekiwanego stanu supervisor,
albo szersza technika wykonania).

---

## 3. Dlaczego „własny loader + własna persona" jest ślepą uliczką

`.PRG` to nie tylko nagłówek i relokacje. To **cały kontrakt środowiska**:
43 punkty inwentarza (`AUDYT_2026-10-06_WDROZENIE/EMUTOS-PRZED-URUCHOMIENIEM-PRG.md`)
— zmienne 0x380–0x800, wektory 2..62 i `etv_*`, KBDVECS/IOREC, basepage i układ
stosu wejściowego, stan AES/VDI przed pierwszym `appl_init`. Nasza persona
odtwarza ten kontrakt **ręcznie, punkt po punkcie** — i każdy brakujący punkt
wygląda jak „program się wywala bez powodu".

EmuTOS ten kontrakt **ma** — przetestowany przez dziesięciolecia.

---

## 4. Propozycja: rozdzielić dwa poziomy wczytywania

Kluczowe rozróżnienie, którego brakowało:

**Poziom 1 — wczytanie SYSTEMU TOS (raz na sesję).**
Nasz loader (albo BIOS) znajduje obraz, ładuje go i przekazuje sterowanie.
Tu zostaje cała dotychczasowa praca: rozpoznanie nagłówka, `auto_tos`,
`CFG`, `bootram`.

**Poziom 2 — wczytanie PROGRAMU `.PRG` (wielokrotnie w sesji).**
Robi to **GEMDOS z EmuTOS** (`bdos/kpgmld.c` + `bdos/proc.c`): nagłówek 0x601A,
relokacje, basepage, środowisko, `p_uft`, stos wejściowy, `Pexec`.
**My tego nie piszemy.** Program dostaje kompletne, prawdziwe środowisko TOS,
więc przestaje być ważne, czy używa `rsrc_load`, `fsel_input`, `wind_get`,
`Setexc` czy czegokolwiek innego — bo wszystko jest z jednego, spójnego źródła.

To jest odpowiedź na „nie możemy dostosowywać każdego programu osobno":
**nie dostosowujemy programów — dostarczamy im prawdziwy TOS.**

### 4.1 Warianty realizacji

| | B: EmuTOS jako warstwa zgodności **w** ATOS | C: EmuTOS **zamiast** ATOS (osobna sesja) |
|---|---|---|
| Kto rządzi sprzętem | ATOS (BIOS, sterowniki, kompozytor) | EmuTOS |
| Pulpit ATOS | zostaje, TOS w oknie/sesji | znika na czas sesji |
| Wysiłek | większy: trzeba podpiąć urządzenia pod EmuTOS | mniejszy: boot obrazu i tyle |
| Zgodność programów | pełna (jeśli urządzenia podpięte poprawnie) | pełna |
| Kiedy sensowne | docelowo | **teraz**, dla programów przejmujących sprzęt (KOBOLD) i jako narzędzie walidacji |

**Rekomendacja: C natychmiast jako narzędzie i profil dla programów
sprzętowych, B jako cel.** Nie trzeba wybierać „albo-albo" — C jest krokiem,
który pozwala sprawdzić komponenty EmuTOS na prawdziwym sprzęcie bez
inwestowania w integrację.

### 4.2 Konkret dla Poziomu 1: przestać reimplementować `bootram`

To jest najważniejszy wniosek techniczny tej analizy. Oficjalny build RAM-TOS
EmuTOS **nie jest gołym obrazem, który się kopiuje i skacze**. `Makefile:798`:

```
$(EMUTOS_PRG): obj/minicrt.o obj/boot.o obj/bootram.o obj/ramtos.o
```

a `util/bootram.S` mówi wprost:

> *„Copy emutos.img from a temporary buffer to its hardcoded location, then run
> it. […] This needs to be done in assembly language, because the copy routine
> may need to be copied to a safe place to avoid being overwritten when ramtos
> is copied to its final destination."*

Co `bootram()` robi po kolei (fakty z kodu):
1. `move.w #0x2700,sr` — wyłącza przerwania („we are going to destroy everything"),
2. `dest = 8(a4)` — adres docelowy **z nagłówka obrazu** (= nasze `0x2140`),
3. rozstrzyga **przypadek nakładania się** bufora źródłowego i docelowego:
   jeśli procedura kopiująca zostałaby nadpisana, **przenosi samą siebie**
   w bezpieczne miejsce (`copyboot`, `copy_a2_a3_d2_sync_caches`),
4. `clr.l 0x6fc.w` (warm_magic) i `clr.l 0x426.w` (`resvalid`) — **zanim** ramtos
   sprawdzi wartości magiczne,
5. kopiuje z `bcopy` / **`bcopy_backwards`** zależnie od kierunku nakładania,
6. `sync_caches` — synchronizacja cache po skopiowaniu **kodu**,
7. skacze pod `4(a2)` = ENTRY z nagłówka.

Nasza ścieżka (`tos_load_once` + `ram_image_start`) robi **część** z tego:
staging w ST-RAM (więc nakładania nie ma), `clr 0x6fc`, `clr 0x426`,
`_phystop`/`_memvalid`/`memctrl`, wyczyszczenie cache, skok pod ENTRY.
**Nie robi** pełnej sekwencji `bootram` (m.in. własnego przenoszenia procedury
kopiującej i `sync_caches` w takiej samej formie) — a to jest dokładnie ta
różnica, której nie da się „wydedukować", bo ona istnieje po to, by obsłużyć
przypadki brzegowe.

**Propozycja: zamiast łatać własną ścieżkę, wziąć `util/bootram.S` +
`util/ramtos.S` + `util/minicrt.S` z EmuTOS** (są w drzewie, GPL-2.0+ z notami
już w `licenses/`) i wołać upstreamowe `bootram(src, size, cpu)` z naszego
loadera. Wtedy Poziom 1 wykonuje **dokładnie tę sekwencję, którą EmuTOS
przewidział i testuje** — a nasz kod odpowiada tylko za: znalezienie obrazu,
przygotowanie bufora źródłowego i wywołanie. Znika cała klasa „prawie tak samo".

### 4.3 Podział odpowiedzialności (docelowo, wariant B)

```
program .PRG
   |
   |  trap #1/#2/#13/#14  (kontrakt TOS — EmuTOS)
   v
GEMDOS / BIOS / XBIOS / AES / VDI        <- EMUTOS (kpgmld, proc, vdi, aes)
   |
   |  wąski, wewnętrzny interfejs urządzeń
   v
podsystem zasobów ATOS                    <- NOWE (tos_session, kernel_resources)
   |
   |  właściciel, tryb współdzielenia, kolejka, zwolnienie po sesji
   v
sterowniki ATOS (blok, FAT, VOFA/ET4000, IKBD, zegar)  <- ISTNIEJĄ
```

Konkretnie, co EmuTOS dostaje od nas (a czego mu nie oddajemy):

| zasób | jak | uwaga |
|---|---|---|
| pamięć | `_membot`/`_memtop`/`_themd` z naszego alokatora | jedna sesja = jeden TPA |
| ekran | `_v_bas_ad`, `Physbase`/`Logbase`, `V_REZ_*`, `v_lin_wr` → nasza powierzchnia sesji | kompozytor ATOS prezentuje wynik |
| dysk | `Rwabs`/`Getbpb`/`Mediach` → nasza warstwa blokowa i FAT | logiczne woluminy, nie „urządzenie" |
| klawiatura/mysz | **jeden** parser → wspólne IOREC/KBDVECS | inaczej dwa parsery walczą o ACIA |
| zegar/VBL | nasz timer → `_hz_200`, `_vblsem/_nvbls/_vblqueue/_vbclock`, zdarzenie VBL | VBL to nie „tick 50 Hz" — potrzebne prawdziwe źródło klatki |
| wektory | rejestracje sesji z **generacją** | adres z poprzedniego uruchomienia nie może być wywołany po zwolnieniu pamięci |
| DMA/ACSI/FDC/PSG | **transakcje** w naszym sterowniku, nigdy rejestry dla klienta | PSG ma wspólny selektor rejestru; nasz FDC używa go do wyboru stacji |
| MFP/EOI, przerwania | zostaje po stronie jądra | nie zmieniamy globalnie trybu EOI dla zgodności z gościem |

### 4.4 Kryteria zaliczenia (żeby nie było „prawie działa")

1. **Poziom 1**: obraz EmuTOS startuje pod ATOS bez paniki; widać jego pulpit
   albo jego komunikat startowy. Mierzone: brak `kernel_fault_vector`,
   `g_tos_load_reason=0`.
2. **Poziom 2**: ten sam program uruchomiony **trzy razy** (ten sam → ten sam →
   inny) nie zostawia wektorów, blokad ani wskaźników do zwolnionej pamięci.
   Mierzone: wejście reaguje, `_hz_200` rośnie, dysk czyta — test już
   przygotowany (`TOSDIR → TOSDIR → TOSMONO → TOSDIR`).
3. **Żadne nieobsłużone wywołanie nie zwraca fałszywego sukcesu**: dla każdej
   funkcji tabela kontraktu (argumenty, typ wyniku, dopuszczalny błąd).
   Nie „wszędzie -32": część funkcji zwraca **adres**, część status, część nic.
4. **Surowe I/O dopiero po przejęciu woluminu**: blokada transportu to za mało —
   trzeba zatrzymać operacje plikowe woluminu, rozliczyć uchwyty, unieważnić
   cache i zwalidować po oddaniu.

---

## 5. Co robimy z istniejącą personą (nie wyrzucamy)

* **Kontrakty już naprawione zostają** — variant B też ich potrzebuje: VDI/AES
  EmuTOS-a mają te same pola i typy (`WF_*`, `vq_mouse`, `vqt_*`).
* **Persona zostaje jako**: ścieżka awaryjna, narzędzie diagnostyczne
  (dziennik 16 wyników na wywołanie, rejestr nieobsłużonych wywołań z argumentami
  i wynikiem, zapis rejestrów w chwili wyjątku) oraz **wzorzec kontraktów** przy
  podpinaniu EmuTOS-a.
* **Nie rozwijamy jej dalej jako strategii** — tylko jako narzędzie.

---

## 6. Plan etapów (każdy z mierzalnym kryterium)

| etap | co | kryterium |
|---|---|---|
| 0 | `bootram` z EmuTOS zamiast własnej sekwencji; `g_tos_load_reason` + analiza `addr=0x00e0012c` | brak paniki, `reason=0` |
| 1 | EmuTOS RAM-TOS startuje pod ATOS (profil C) | widać pulpit/komunikat EmuTOS |
| 2 | programy uruchamiane przez EmuTOS (Poziom 2) | ten sam program ×2 + inny; wejście/zegar/dysk OK |
| 3 | podpięcie ekranu EmuTOS pod `video_driver_t` (wariant B) | TOS w oknie ATOS, pulpit pod spodem |
| 4 | dysk/klawiatura/zegar przez podsystem zasobów | brak dwóch parserów IKBD, brak dwóch właścicieli DMA |
| 5 | surowe I/O z przejęciem woluminu | plik odczytany przed i po, FAT spójna, awaria w trakcie obsłużona |
| 6 | **osobny** prototyp izolacji 68030 | chroniona strona, zabronione MMIO, kontrolowane `Super`/`Supexec` |

---

## 7. Pytania, na które potrzebuję Waszej decyzji

1. **Cel**: wariant B (EmuTOS w ATOS, TOS w oknie) — czy wariant C jako
   docelowy, a ATOS tylko jako bootloader/menedżer?
2. **Profil C teraz?** Czy zgoda, by programy przejmujące sprzęt (KOBOLD)
   dostawały osobną sesję z prawdziwym TOS-em, zamiast walczyć o współdzielenie
   DMA/PSG?
3. **Zakres persony**: zostawiamy ją wyłącznie jako narzędzie/awaryjną ścieżkę
   i przestajemy łatać kontrakty „po programie"? (Kontrakty naprawione zostają.)
4. **`bootram`**: zgoda na wciągnięcie `util/bootram.S` + `ramtos.S` + `minicrt.S`
   z EmuTOS do naszego loadera (GPL-2.0+, noty są)?
5. **Kolejność**: najpierw domknąć Poziom 1 (start EmuTOS pod ATOS), czy od razu
   ruszyć integrację wariantu B (podpinanie urządzeń pod EmuTOS)?

---

# ERRATA i PLAN ZAAKCEPTOWANY (po ocenie użytkownika, 2026-10-09)

## A. Potwierdzony błąd mojego loadera — znaleziony przez użytkownika

**Objaw:** loader nadpisywał własne dane przed ich ostatnim użyciem.

**Dowód:** `g_system` leży pod **0x000021E0** (potwierdzone w `build/bios_mste.elf`),
a obraz EmuTOS-a ładuje się pod `base=0x2140` i ma 260 282 B, czyli zajmuje
**0x2140–0x419FA**. `g_system` jest **w tym zakresie**. Kod po skopiowaniu
obrazu czytał `g_system.cpu_type` (0x21E1) i `g_system.ram_bytes` — i dostawał
bajty obrazu: **CPU = 0xFC = 252**, **RAM = 0x00064E7B = 413 307** zamiast
wykrytej konfiguracji. Te śmieci trafiały do `ram_image_start()`, czyli do
ustawiania cache i `_phystop`.

**Naprawa (`loader/tos.c`):** wszystkie parametry przekazania są teraz
**snapshotowane przed kopiowaniem** i po kopiowaniu nie czytamy już niczego
z `im` ani z `g_system`:

```c
const uint32_t to_load    = im->load_address;
const uint32_t to_size    = im->payload_size;
const uint32_t to_ram     = g_system.ram_bytes;
const uint32_t to_memconf = (uint32_t)REG8(MMU_MEMCONF);
const uint32_t to_cpu     = g_system.cpu_type;
...
move_bytes((uint8_t *)to_load, (const uint8_t *)stage_address, to_size);
ram_image_start(to_load, to_ram, to_memconf, to_cpu);   /* tylko snapshoty */
```

**Wynik przebiegu (ta sama konfiguracja, co przed naprawą):**

| | przed naprawą | po naprawie |
|---|---|---|
| ekran po ~10 s | `Panic: Bus Error`, pc=0x15FBA, addr=0x00E0012C | **brak paniki**, ekran biały, belka `RS:80` |
| rozmiar zrzutu | 7254 B | 12275 B |

Czyli **udział tego błędu w panice został potwierdzony przebiegiem** — panika
zniknęła po samej naprawie przekazania. Zrzut:
`zrzuty/45-po-naprawie-brak-paniki-EmuTOS-przejmuje-ekran.png`.

**Uczciwie: to jeszcze nie jest „EmuTOS działa".** Biały ekran oznacza, że
ktoś wyczyścił/inicjalizował ekran — może to być EmuTOS, ale równie dobrze
zawieszenie po inicjalizacji. Nie wiem jeszcze, czy CPU jest w kodzie EmuTOS-a,
czy w pętli. Dlatego następny pomiar to **A0, stos i miejsce wywołania** —
dokładnie jak zauważyliście, że sam adres PC niczego nie dowodzi.

## B. Korekty, które przyjmuję (były błędne lub za szerokie)

1. **„90 KB" — błędne.** `0x15FBA − 0x2140 = 0x13E7A = 81 530 B = 79,6 KiB`.
2. **Mapa EmuTOS, którą się posługiwałem, opisuje build ROM** — nie nadaje się
   do diagnozy obrazu RAM ładowanego pod 0x2140. Trzeba mieć **własną mapę
   RAM** z tego samego builda (etap 1 planu).
3. **`bootram` — zawężone.** Bierzemy **tylko `util/bootram.S`**:
   * `ramtos.S` tylko osadza `emutos.img` przez `.incbin` — zbędny, gdy obraz
     czytamy z dysku,
   * `minicrt.S` uruchamia aplikację PRG i woła **już działający GEMDOS**
     (`Mshrink`/`Pterm`) — to nie jest startup z BIOS-u.
   * **Konwersja kodu CPU:** `bootram` oczekuje **30** dla 68030, a ATOS ma
     `CPU_68030 = 3`. Bez przeliczenia ominie obsługę cache.
   * Trzeba zapewnić **wyrównanie, padding i miejsce** na ewentualne
     przeniesienie procedury kopiującej (to jest cały sens `bootram`).
4. **„Pełna zgodność" — usuwam tę obietnicę.** EmuTOS daje znacznie lepszą
   podstawę, ale **sam publikuje listę niekompatybilności**. Plan ma mówić
   „zgodność z rzeczywistymi aplikacjami, mierzona testami", nie „pełna".
5. **C nie realizuje B.** Zwykły start EmuTOS **przejmuje maszynę** — nie da się
   najpierw pokazać go w oknie żyjącego ATOS, a potem „dorobić" mu dysk,
   przerwania i klawiaturę. Minimalna integracja B musi **od początku** objąć:
   pamięć, sposób wykonywania kodu, wejście, zegar, dysk i ekran.
6. **Własność systemu plików.** Jeśli zachowujemy GEMDOS+FAT EmuTOS, musi on
   najpierw dostać **prywatny obraz dysku**. Dwa niezależne systemy FAT **nie
   mogą** swobodnie zarządzać tym samym zamontowanym woluminem.

## C. Plan wykonania (przyjęty w tej kolejności)

1. **Utrwalić jeden czysty build RAM**: obraz, **pasująca mapa**, opcje
   kompilacji i hash. Bez tego diagnoza jest zgadywaniem.
2. **Naprawić przekazanie**: końcowe kopiowanie i skok przez **właściwie
   podłączony `bootram`**, bez późniejszych odczytów BSS BIOS-u.
   *(Część pierwsza — snapshot — jest już zrobiona i zmierzona, patrz A.)*
3. **Zaliczyć C użytkowo**: pulpit, klawiatura, mysz, zegar, odczyt pliku,
   a potem **powtórne uruchomienia** LDW, SysInfo i KOBOLD-a.
4. **Zbudować minimalną sesję B** z prywatną pamięcią i **prywatnym dyskiem**
   oraz kompletem niezbędnych adapterów (pamięć, wejście, zegar, dysk, ekran).
5. **Dopiero potem** rozszerzać współdzielenie zasobów. **Izolację 68030
   prowadzić osobno** — nie jest warunkiem żadnego z powyższych kroków.

## D. Zachowane z persony (zgodnie z Waszą decyzją)

Diagnostyka (dziennik 16 wyników na wywołanie, rejestr nieobsłużonych wywołań
z argumentami i wynikiem, zapis rejestrów w chwili wyjątku), testy oraz
sprawdzone adaptery kontraktów. **Rozwijanie persony jako głównej strategii
zgodności — zakończone.**

---

# WYNIK PRZEŁOMOWY: EmuTOS URUCHOMIONY POD ATOS (profil C)

Po naprawie przekazania (sekcja A) wykonałem pomiar rozstrzygający — odczyt
**zmiennych systemowych TOS-u** po starcie. To odpowiada wprost na pytanie
„czy CPU jest w kodzie EmuTOS-a, czy w pętli".

## Pomiar

```
_sysbase   (0x4F2) = 0x00002140     <- wlasny naglowek OS EmuTOS, pod baza obrazu
_membot    (0x432) = 0x0005005C     <- EmuTOS SAM wyliczyl poczatek TPA
_memtop    (0x436) = 0x003F8000     <- i koniec TPA
_phystop   (0x42E) = 0x00400000
_v_bas_ad  (0x44E) = 0x00C00000     <- EmuTOS sam przydzielil bufor ekranu
_hz_200    (0x4BA) = 2542 -> 3795   <- ZEGAR ROSNIE (przerwania zyja)
_vbclock   (0x462) = 0x0249
_frclock   (0x466) = 0x037A
```

**Wniosek: EmuTOS wystartował, przejął maszynę i działa.**

* `_sysbase` wskazuje na **jego własny** nagłówek OS pod `0x2140`, czyli pod
  adresem, pod który go załadowaliśmy,
* `_membot`/`_memtop`/`_phystop` zostały **przez niego wyliczone** (my ich nie
  ustawialiśmy na te wartości),
* `_v_bas_ad = 0xC00000` — EmuTOS sam przydzielił bufor ekranu,
* `_hz_200` **rośnie** (2542 → 3795 w 6 s ≈ 200 Hz) — czyli **jego przerwania
  działają**,
* `_vbclock`/`_frclock` też się liczą.

Dodatkowo w logu emulatora widać `PC=$2d9c` przy błędach szyny — a
`0x2d9c − 0x2140 = 0xC5C`, czyli **PC jest 3 kB w głąb obrazu EmuTOS-a**.
To rozstrzyga wątpliwość: **CPU wykonuje kod EmuTOS-a**, nie kręci się w pętli.

## Co jeszcze nie działa (uczciwie)

1. **Pulpit EmuTOS nie jest widoczny** — ekran pozostaje biały. EmuTOS
   zainicjalizował ekran (`_v_bas_ad` ustawione), ale nasz BIOS rysuje własną
   belkę statusu, więc trzeba ustalić, kto w tej chwili włada shifterem.
2. **EmuTOS sonduje sprzęt, którego nie ma** — log pokazuje powtarzalne
   `Bus Error` przy `PC=$2d9c`:
   ```
   $ffff8901, $ffff9201   <- obszar ST DMA (ACSI/dysk)
   $fff0005d/9d/dd, $fffffe00
   ```
   EmuTOS obsługuje te błędy własnym handlerem (dlatego żyje dalej), ale to
   znaczy, że **oczekuje sprzętu, którego nasza konfiguracja nie wystawia**
   (mamy IDE, ACSI wyłączone). Dokładnie to ma rozwiązać etap 4 — adaptery
   urządzeń dla sesji B.

## Jak to się ma do planu

To jest **etap 3 w wersji częściowej**: system startuje i żyje, ale bez
widocznego pulpitu i z sondowaniem brakującego sprzętu. Etap 3 zamknie się
wtedy, gdy zobaczymy pulpit EmuTOS, a klawiatura, mysz, zegar i odczyt pliku
będą działać pod jego GEMDOS-em — i gdy powtórzone uruchomienia LDW, SysInfo
i KOBOLD-a przejdą bez śladów.
