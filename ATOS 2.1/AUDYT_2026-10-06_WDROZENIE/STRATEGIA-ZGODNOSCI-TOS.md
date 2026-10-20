# STRATEGIA ZGODNOŚCI TOS — dlaczego nie per program i co robimy zamiast tego

Data: 2026-10-08. Autor: analiza na podstawie kodu ATOS 2.1, drzewa EmuTOS
(`files-pasted-by-the-user-projekt/work/emutos-reference`), dokumentów projektu
(`docs/EMUTOS-INTEGRATION.md`, `docs/DESKTOP-ON-EMUTOS.md`, `docs/V10-STATUS.md`)
oraz przebiegów w Hatari z pełną diagnostyką.

## 1. Odpowiedź na pytanie: nie, nie możemy dostosowywać każdego programu osobno

**Dowód z naszej własnej pracy.** Każda poprawka w tej sesji była *poprawna
kontraktowo*, ale **odkrywana pojedynczo przez kolejny program**:

| program | co odsłonił | czego to dowodzi |
|---|---|---|
| LDW Power | `wind_get`/`wind_set` (złe numery pól), `Pterm` (kod z `usp+2`), `evnt_*` | jedno narzędzie → kilka luk kontraktu |
| KOBOLD 3.51 | 14 nieobsłużonych wywołań, w tym VDI `vqin_mode` (115), AES `appl_find` (13), `appl_write` (12), BIOS `Setexc` (5) | **inny** program → **inny** podzbiór luk |
| sonda katalogu | `Fsfirst` (limit 16 wpisów!), brak `attr`, brak `Dsetpath`/`Fdatime` | kolejny podzbiór |

Zbiory tych luk **nie pokrywają się**. Koszt rośnie jak O(programy × API) ✗,
a każdy nowy program to nowa runda „ślad → poprawka → pomiar". To nie jest
kompatybilność — to gaszenie pożarów.

**Wniosek:** potrzebujemy **jednej implementacji TOS** dostarczającej kontrakty
(GEMDOS/BIOS/XBIOS/AES/VDI), a nie własnej persony łata nej program po programie.

## 2. Decyzja w projekcie już zapadła — i jest słuszna

`docs/V10-STATUS.md`: **„Decyzja: wariant B"** — ATOS BIOS/kernel/`video_driver_t`
zostają, a **GEMDOS/VDI/AES bierzemy ze źródeł EmuTOS** (GPL-2.0+, noty w
`licenses/`). `docs/DESKTOP-ON-EMUTOS.md` dodaje: pulpit staje się **aplikacją GEM**
na tej warstwie, bo stary kompozytor ma błąd arbitrażu odświeżania.

To jest właściwy kierunek: **ATOS rządzi sprzętem i bootem, EmuTOS dostarcza
kontrakty TOS**. Stare programy nie wiedzą o zmianie.

## 3. Co dziś blokuje wariant B (fakty, nie domysły)

| droga | stan | dowód |
|---|---|---|
| Build EmuTOS z `m68k-elf-` | **failuje** | `docs/V10-STATUS.md`: `bios/bios.c` → „can't resolve 0 - sp"; EmuTOS zakłada profil `-mshort` z `m68k-atari-mint-` |
| Toolchain `m68k-atari-mint-` | **niedostępny** | `brew search m68k` → tylko `m68k-elf-gcc`, `m68k-elf-binutils`; brak pakietu mint |
| Gotowy **RAM-TOS** EmuTOS | brak obrazu „loadable" | loader `loader/tos.c:78` przyjmuje tylko adresy `0x800..0x80000`; gotowe `emutos-*-256k.img` mają `rom_address=0x00E00000` ✗ → „ROM IMAGE NEEDS MMU OR ROM BANK" |
| Gotowe ROM-y EmuTOS | **są** | `emutos.img`, `etos256us.img`, `atari-030-port/disk/emutos-{us,pl}-*.img` |
| Ścieżka MMU (68030 + ROM) | **przeniesiona**, test niepełny | `docs/V10-STATUS.md`: `boot/mmu030.S` + `boot/mmu_boot.c`, build OK |
| `emu256pl.prg` (EmuTOS jako program) | **nie przejmuje maszyny** | patrz §4 |

## 4. Eksperyment wykonany w tej rundzie: EmuTOS jako program TOS pod ATOS

Zainstalowałem gotowy `emu256pl.prg` (258 278 B) w obrazie i uruchomiłem go
przez naszą personę (`kernel_tos_run_program`), z pełną diagnostyką.

Nagłówek: `tlen=258236 dlen=0 blen=1028 slen=0`, `abs=0`, **`flags=0x07`**
(fastload + ładowanie i pamięć w TT-RAM), tablica relokacji dokładnie na końcu
obrazu — więc nasz loader **powinien** go przyjąć i faktycznie przyjmuje.

Wynik przebiegu:

```
t=24s..120s   GEM=0   fault=0   exit=0   unsup=1 (VDI fn=115 — z NASZEGO bootstrapu)
zrzut ekranu: nasz pulpit nietkniety (menu, ikony A/B/C/D, zegar, belka statusu)
```

**Wniosek:** EmuTOS jako program **nie przejmuje maszyny w naszym środowisku**.
Nie ma faultu i nie ma wywołań AES — czyli program kończy się (albo wraca) bez
skutku. To **dokładnie** ten sam powód, dla którego raporty mówiły „braki AES/VDI
mogą zatrzymać go wcześniej": EmuTOS-as-PRG oczekuje **kompletnego środowiska TOS**
(boot, wektory, sysvars, VBL, dysk), a nasza persona daje mu podzbiór.

Dodatkowo: przy okazji **potwierdziłem własny błąd** — `kernel_tos_bootstrap()`
woła VDI `0x73` = 115 (`vqin_mode`), którego nie implementujemy, i mimo to
drukuje „VDI AES READY". To do usunięcia (raport luk zgłaszał to samo).

## 5. Rekomendowana kolejność (do zatwierdzenia)

**Krok 0 (natychmiast, mały):** usunąć atrapę `kernel_tos_bootstrap` i nie
udawać gotowości środowiska. Uczciwy komunikat zamiast „VDI AES READY".

**Krok 1 (fundament zgodności): uruchomić prawdziwy TOS pod ATOS.**
Dwie drogi, w tej kolejności:
1. **RAM-TOS EmuTOS z naszym loaderem** — wymaga builda; najpierw **zbadać, czy
   `m68k-elf-gcc -mshort` jest w stanie zbudować EmuTOS** (zapisany błąd
   „can't resolve 0 - sp" to problem asemblera/składni, nie architektury).
   Jeśli tak → wariant B staje się wykonalny bez instalowania obcego toolchainu.
2. **Ścieżka MMU/ROM** (`boot/mmu030.S`, `--mmu on`) — już przeniesiona,
   wymaga dokończenia testu; daje GEM bez budowania czegokolwiek.

**Krok 2 (właściwy wariant B):** podpiąć rysowanie EmuTOS pod `video_driver_t`
(plan w `docs/EMUTOS-INTEGRATION.md` §3: backendy `vdi_line/fill/text/blit/raster`
oraz `bios/screen.c`/XBIOS → `video_driver_t`; AES bez zmian, bo używa VDI).

**Krok 3:** pulpit jako aplikacja GEM (`docs/DESKTOP-ON-EMUTOS.md`).

**Krok 4 (równolegle, dla programów przejmujących sprzęt):** profil C — osobne
uruchomienie TOS/EmuTOS (KOBOLD i podobne), zgodnie z oceną użytkownika.

## 6. Inwentarz, który jest specyfikacją integracji

`AUDYT_2026-10-06_WDROZENIE/EMUTOS-PRZED-URUCHOMIENIEM-PRG.md` — 43 punkty:
co EmuTOS ustawia **zanim** uruchomi .PRG (sysvars 0x380–0x800, wektory, VBL,
KBDVECS/IOREC, basepage i stos wejściowy, stan AES/VDI). To nie jest opis
„jak działa TOS" — to **lista wymagań wobec naszego kernela**, jeśli ma pod nim
działać prawdziwy TOS. Do wykorzystania wprost w wariancie B.

## 7. Czego świadomie NIE robimy

* Nie łata my kontraktów „po programie" jako strategii (można tak **domykać**
  pojedyncze luki, nie tak budować zgodność).
* Nie wracamy do zbiorczego `-32` (kontrakty różnią się: adres/status/void).
* Nie udajemy izolacji: dopóki gość działa w rzeczywistym supervisorze, może
  ominąć podsystem zasobów — profile A/B to na tym etapie **zasady współpracy**.
* Nie zaczynamy od faultowania każdego dostępu MMIO przez MMU (koszt na 030).

---

# AKTUALIZACJA 2026-10-08: BLOKER TOOLCHAINU USUNIĘTY

## Ustalenie

Zapis w `docs/V10-STATUS.md` („Wariant B — build EmuTOS: BLOKER toolchain",
`bios/bios.c` → „can't resolve 0 - sp") był **nieprawdziwy**. Prawdziwa przyczyna:

* EmuTOS pisze rejestry w inline-asm **bez prefiksu `%`** (`include/biosbind.h`):
  `move.w %d0,-(sp)` / `trap #13` / `addq.l #2,sp`,
* dlatego wymaga asemblera z `--register-prefix-optional`,
* **EmuTOS sam to obsługuje**: `Makefile:205-213` ma `ELF = 0/1`; przy `ELF=1`
  ustawia `TOOLCHAIN_PREFIX = m68k-elf-` oraz
  `TOOLCHAIN_CFLAGS = -fleading-underscore -Wa,--register-prefix-optional
  -fno-reorder-functions --param=min-pagesize=0 -DELF_TOOLCHAIN`,
* udokumentowane wprost w drzewie: `doc/old_changelog.txt:4276`
  — „Use with `make 512 ELF=1`".

Czyli **nie trzeba instalować obcego toolchainu** (`m68k-atari-mint-`).

## Dowód (wykonany)

Kompilacja pojedynczego pliku naszym toolchainem:

```
m68k-elf-gcc -m68000 -mshort -fleading-underscore -Wa,--register-prefix-optional
             -fno-reorder-functions --param=min-pagesize=0 -DELF_TOOLCHAIN ...
             -c bios/bios.c -o obj/bios.o        -> bios.o 10152 B, bez błędu
```

Pełny, czysty build (po usunięciu nieświeżych `obj/*.o` — drzewo referencyjne
miało obiekty z 22 wrz pomieszane między celami 256k/512k):

```
make 256 ELF=1 -j4
  -> mkrom pad 256k emutos.img etos256us.img
  -> etos256us.img done (1862 bytes free)
  EXIT=0,  0 błędów
  świeży emutos.img: 260282 B
```

Artefakty zapisane: `work/emutos-fresh/emutos-256k-elf.img`,
`work/emutos-fresh/etos256us-elf.img`.

## Co to zmienia

* **Wariant B jest wykonalny** bez zmiany toolchainu: mamy EmuTOS zbudowany
  z naszych źródeł (`files-pasted.../work/emutos-reference`) naszym `m68k-elf-`.
* Kolejny krok to **uruchomienie tego ROM pod ATOS**: albo ścieżką MMU
  (`boot/mmu030.S` + `--mmu on`, już przeniesiona — obraz ma
  `rom_address` w górnej pamięci, więc loader bez MMU go nie przyjmie), albo
  buildem **RAM-TOS** z adresem linkowania w `0x800..0x80000`, który nasz
  loader `loader/tos.c` przyjmie bezpośrednio.
* Uwaga o koszcie: drzewo referencyjne nie lubi mieszania celów — po zmianie
  `256`/`512` trzeba usunąć `obj/*.o` (inaczej link zgłasza brak symboli
  `_has_videl`, `_dsp_init`, `_tt_mfp_init`).

---

# RUNDA: start świeżego EmuTOS pod ATOS — wyniki i następny krok

## 1. Krok 0 zrobiony: koniec atrapy bootstrapu

`kernel_tos_bootstrap()` **nie udaje już gotowości środowiska**:
* wołał `tos_vdi_call(0x73u, …)` = VDI 115 (`vqin_mode`), którego **nie mamy**
  → `default: return 0`, a mimo to drukował „VDI AES READY",
* teraz naprawdę otwiera stację fizyczną przez `v_opnwk` (VDI 1 — tę funkcję mamy)
  i mówi prawdę: „VDI workstation opened" albo „VDI workstation FAILED".
`make check = 0`, 40 PASS.

## 2. Ścieżka MMU: BIOS startuje, ekran się sypie

Zbudowałem obraz startowy z **naszym świeżym EmuTOS** i włączyłem MMU:

```
make TOS=1 image-v10-clean                 -> work/atos-v10-clean.img
install_boot_sys.py … work/EMUTOS.IMG --data   -> \SYSTEM\EMUTOS.IMG (260282 B)
make_bios_cfg.py work/BIOS.CFG --auto-tos 1 --boot-device 1
install_boot_sys.py … work/BIOS.CFG --root --data
hatari --mmu on --tos build/bios_mste.bin
```

Wynik (zrzut `zrzuty/41-EmuTOS-MMU-ekran-uszkodzony.png`):
* BIOS **wstaje**, belka statusu pokazuje `32MHz/030(CED)/-/MMU` — czyli MMU jest
  włączone ✓,
* **ekran jest uszkodzony**: pasmo śmieci u góry, reszta czarna ✗
  → mapowanie MMU nie obejmuje poprawnie bufora ekranu / okna ET4000.

Wniosek: ścieżka MMU wymaga najpierw naprawy mapowań ekranu. To nie jest droga
na teraz.

## 3. LEPSZA DROGA — RAM-TOS bez MMU (znaleziona w źródłach EmuTOS)

EmuTOS sam opisuje ten wariant (`Makefile:766-770`):

> „Special variants of EmuTOS running in RAM instead of ROM. In this case,
>  **emutos.img needs to be loaded into RAM by some loader**."

I co najważniejsze — **nasz świeży obraz nadaje się do tego bez przeróbek**:

```
naglowek emutos.img: 602e 0206 00e00030 00e00000 …
                     BRA.S   v2.06  ENTRY=0x00E00030  base=0x00E00000
```

* `0x602e` = `BRA.S` o przesunięciu 0x2e → **adres wejścia jest w nagłówku**
  (`0x00E00030`), a nasz `loader/tos.c` **już czyta to pole** (`p[4..7]`),
* obraz linkowany jest pod `0x00E00000` = 14 MB → na TF536 z 64 MB TT-RAM to
  **pamięć zapisywalna**, więc można go tam **skopiować i skoczyć** — bez MMU,
  bez mapowań, bez kombinowania z ROM-em.

### Co trzeba zmienić (małe i konkretne)

`loader/tos.c:78` uznaje za „loadable" tylko obrazy z adresem
`0x800..0x80000` — to reguła pisana pod RAM-TOS w ST-RAM. Trzeba ją rozszerzyć:
obraz, którego adres linkowania leży w **zapisywalnym RAM-ie** (ST lub TT),
jest RAM-TOS-em i ładujemy go tam, gdzie wskazuje nagłówek, po czym skaczemy
pod `entry` z nagłówka. Bez tego EmuTOS w wersji ROM trafia na „ROM IMAGE NEEDS
MMU OR ROM BANK".

### Dlaczego to jest właściwy następny krok

* nie wymaga MMU (a więc nie dotyka uszkodzonego mapowania ekranu),
* korzysta z **naszego** EmuTOS zbudowanego **naszym** toolchainem,
* daje prawdziwy TOS pod ATOS — czyli fundament wariantu B,
* jest małą, sprawdzalną zmianą w jednym pliku + test na obrazie.

---

# RUNDA: RAM-TOS EmuTOS — zbudowany i podpięty; korekta dwóch moich błędów

## Osiągnięcia (potwierdzone komendami)

1. **EmuTOS buduje się jako RAM-TOS naszym toolchainem.** Warunkiem nie jest
   żadna zmienna podawana z linii poleceń, tylko **cel `TARGET_PRG`**
   (`include/config.h:34-40`):
   ```c
   #if defined(TARGET_PRG) || defined(TARGET_FLOPPY) || ...
   #  define EMUTOS_LIVES_IN_RAM 1
   ```
   Build: `make 256 ELF=1 DEF='-DTARGET_PRG' -j4` → **EXIT=0, 0 błędów**, a obraz
   linkuje się **do RAM-u**:
   ```
   naglowek: 602e 0206 00002170 00002140
             BRA.S  v2.06  ENTRY=0x00002170  base=0x00002140
   ```
   Adres `0x2140` mieści się w zakresie, który nasz loader już przyjmuje
   (`0x800..0x80000`) — czyli **bez MMU i bez ROM banku**.

2. **Loader nauczył się rozpoznawać nagłówek TOS** (`loader/tos.c`): pola
   `BRA.S / wersja / ENTRY / adres bazowy`. Jeśli adres bazowy leży w
   zapisywalnym ST-RAM-ie i entry jest w obrazie → obraz jest **RAM-TOS-em**:
   `payload_offset=0`, `payload_size=rozmiar`, `load_address=base`,
   `loadable=1`. Wtedy `tos_load_once()` kopiuje obraz pod `base`, a
   `ram_image_start()` (`boot/ramjump.S`) skacze pod **ENTRY z nagłówka**:

   ```asm
   ram_image_start:
        move.l  4(%sp),%a0     /* a0 = load_address */
        move.l  4(%a0),%a1     /* ENTRY z nagłówka obrazu */
        jmp     (%a1)
   ```

   To dokładnie mechanizm, o którym pisze EmuTOS: „emutos.img needs to be
   loaded into RAM by some loader" (`Makefile:766-770`).

3. Obraz testowy złożony: `work/ramtos.img` (czysty v10 + `\SYSTEM\EMUTOS.IMG`
   260 282 B + `BIOS.CFG --auto-tos 1`), BIOS przebudowany z nowym loaderem.

## Korekta dwóch moich wcześniejszych wniosków (były błędne)

* **„MMU psuje ekran" — NIEPRAWDA.** Ten sam uszkodzony obraz występuje przy
  `--mmu off` ✗. MMU nie ma z tym nic wspólnego.
* **„EmuTOS nie wstał / problem dotyczy EmuTOS" — NIEUSTALONE.** Obraz jest
  uszkodzony **tak samo bez żadnego TOS-u** (`BIOS.CFG --auto-tos 0`, zrzut
  `zrzuty/42-v10-clean-ekran-uszkodzony-bez-TOS.png`): belka statusu renderuje
  się poprawnie, a **obszar pulpitu to śmieci**.

## Nowy, konkretny błąd do naprawy (znaleziony przy okazji)

**Obraz v10-clean w tej konfiguracji renderuje uszkodzony obszar ekranu** —
niezależnie od MMU i niezależnie od TOS-u. To blokuje *odczytanie* wyniku
testu EmuTOS-a, bo nie da się odróżnić „EmuTOS wystartował i źle rysuje" od
„nic nie wystartowało, a obraz był zepsuty wcześniej".

Podejrzenie (do sprawdzenia, nie twierdzę): konfiguracja video w `BIOS.CFG`
(`--video-driver` / `--video-mode` to surowe bajty, nie podałem ich wcale)
nie zgadza się z `--graphics-card vofa --gfx-chip et4000 --mono`.

## Następny krok (dwa warianty, oba krótkie)

1. **Odseparować obraz od video**: uruchomić test TOS na bazie, która u nas
   renderuje się poprawnie (np. `work/kobold.img`, `work/desktop-icons.img`),
   dokładając `EMUTOS.IMG` + `BIOS.CFG --auto-tos 1`.
2. **Naprawić konfigurację video w v10-clean**: dobrać `--video-driver` /
   `--video-mode` do VOFA/ET4000 albo ustalić, dlaczego obszar pulpitu jest
   śmieciowy przy poprawnej belce statusu.

Dopiero po tym wynik testu RAM-TOS EmuTOS-a będzie miarodajny.

---

# RUNDA: ścieżka auto-TOS odblokowana konfiguracyjnie; zostało jedno ogniwo

## Co ustaliłem i naprawiłem

1. **Dlaczego auto-TOS nigdy się nie uruchamiał.** `main.c:118` wymaga
   `g_config.auto_tos && g_system.boot_selection_valid`, a `boot_selection_valid`
   ustawia się tylko przy `g_config.boot_valid` (`main.c:104-108`).
   `tools/make_bios_cfg.py:36` ustawia `boot_valid` **tylko gdy podano
   `--boot-id`**. Wcześniejsze wywołania (także to z `Makefile:1083`) podają
   jedynie `--auto-tos 1 --boot-device 1`, więc plik wychodzi z **`boot_valid 0`**
   i ścieżka EmuTOS-a **nigdy nie działa** — niezależnie od tego, co jest na dysku.

   Poprawne wywołanie:
   ```
   make_bios_cfg.py BIOS.CFG --auto-tos 1 --boot-device 1 --boot-id 128 --boot-part 0
   → auto_tos 1 boot_valid 1 boot_id 0x80
   ```
   `128 = 0x80` to **IDE master** (`include/block_device.h:14`:
   `BLOCK_IDE_ID(n) = 0x80 | n`), a nie 0/1 jak sugeruje `--boot-device`.

2. **Ścieżka w `main.c:118-126` jest dokładnie tą, którą przygotowałem:**
   ```c
   if(cat.image[ci].loadable){
       (void)tos_load_once(&cat.image[ci]);      /* RAM TOS */
   } else if(...rom_address && cpu>=68030){
       (void)mmu_boot_rom(&cat.image[ci]);       /* ROM przez MMU */
   }
   ```
   Nasz EmuTOS (RAM-TOS, baza `0x2140`) jest teraz `loadable=1`, więc wchodzi
   w **pierwszą** gałąź — bez MMU.

3. **Usterka ekranu jest ograniczona do przepływu `image-v10-clean`.**
   Ten sam `EMUTOS.IMG` + `BIOS.CFG` na bazie `work/kobold.img` daje
   **idealnie narysowany pulpit ATOS** (zrzut 11 419 B, ikony My Atari/TRASH/
   TERMINAL/COMMANDER/KOBOLD/LDW/TOSMONO). Czyli: to nie MMU, nie EmuTOS i nie
   wyświetlanie jako takie — tylko obraz v10-clean w tej konfiguracji.

## Stan: zostało jedno ogniwo

`tos_load_once()` **nie wchodzi** w EmuTOS-a — po nieudanej próbie BIOS bootuje
dalej kernel i widzimy pulpit. Wszystkie warunki wstępne wyglądają na spełnione
(`payload_offset=0`, `payload_size=rozmiar`, `load_address=0x2140`,
`ram_bytes` 8 MB, `entry` w obrazie), więc winowajcą jest jeden z pozostałych
strażników funkcji:

```c
if(!im->loadable||!im->payload_size||im->payload_offset+im->payload_size>im->size||!cluster_sectors)return 0;
if(g_system.ram_bytes<=im->payload_size+8192u)return 0;
if(!validate_chain(im))return 0;
load_end=im->load_address+im->payload_size;
if(load_end<im->load_address||load_end>g_system.ram_bytes-8192u)return 0;
```

Najbardziej podejrzany jest **`validate_chain(im)`** — plik 260 282 B to ~509
klastrów i łańcuch FAT musi być w całości przejść; jeśli walidacja ma limit albo
liczy rozmiar inaczej niż `payload_size`, funkcja cicho wraca.

### Następny krok (konkretny, jeden przebieg)

Dodać w loaderze BIOS-u **kod przyczyny odmowy** (np. `g_tos_load_reason`
z wartościami 1..5 odpowiadającymi kolejnym strażnikom) i wypisać go w konsoli
BIOS-u po nieudanej próbie. Wtedy jedno uruchomienie wskaże winowajcę bez
zgadywania. Dopiero potem warto wracać do menu F5 (mój test trafił w ekran
startowy ATOS 2.1 — zrzut `zrzuty/43-BIOS-start-po-F5.png` — czyli w złe okno
czasowe, nie w menu).

---

# RUNDA: bloker przesunięty i precyzyjnie zlokalizowany — ładowanie BIOS.CFG

## Wdrożone narzędzia diagnostyczne (zostają w systemie)

1. **Kod przyczyny odmowy loadera** — `g_tos_load_reason` w `loader/tos.c`,
   wartości 1..8 odpowiadają kolejnym strażnikom `tos_load_once()`
   (loadable / payload / cluster_sectors / ram_bytes / validate_chain /
   load_end ×2 / stage_address). Menu F5 pokazuje go na ekranie
   („LOAD FAILED (reason N)"), a debugger czyta go po symbolu.
2. **Diagnostyka ścieżki auto-TOS** — w `main.c`: `g_dbg_auto_tos`,
   `g_dbg_boot_sel`, `g_dbg_boot_id`, `g_dbg_boot_part`, `g_dbg_cat_count`
   oraz na obraz: `g_dbg_img_loadable/compat/size/name[0..3]`.
   Dzięki temu „cicha odmowa" przestaje być nieodróżnialna od „w ogóle nie
   próbowano".

## Zmierzony wynik (to jest sedno tej rundy)

```
auto_tos       = 0        <- konfiguracja NIE zostala wczytana
boot_sel_valid = 0
boot_acsi_id   = 0x00
katalog: obrazow = 0
reason loadera = 0        <- tos_load_once nie byl nawet wolany
```

Czyli **bloker nie leży w TOS-ie, loaderze ani obrazie** — BIOS w ogóle nie
wczytał naszego `BIOS.CFG`. Dopóki `auto_tos` jest 0, cała ścieżka EmuTOS-a
(razem z moją zmianą loadera) jest martwa, mimo poprawnej konfiguracji.

## Znaleziony błąd narzędzia (pułapka)

`tools/install_boot_sys.py` instaluje plik **pod nazwą pliku źródłowego**.
Wywołanie z `/tmp/BIOS4.CFG` dało na dysku `BIOS/BIOS4.CFG`, a BIOS szuka
dokładnie `BIOS.CFG` — więc plik był „zainstalowany", ale nie ten, którego
system szuka. `Makefile:1083` używa `/tmp/BIOS.CFG`, więc tam nazwa jest
przypadkiem poprawna; każda inna nazwa cicho nie działa.

## Następny krok (dokładnie wskazany)

Zdiagnozować **ładowanie konfiguracji** w `hw/config.c`:

1. czy plik jest znajdowany (`\BIOS\BIOS.CFG` na woluminie startowym,
   z fallbackiem `\BIOS.CFG` w katalogu głównym — mam teraz **oba** na dysku),
2. czy przechodzi walidację `cfg_crc` (`hw/config.c:334`) — narzędzie liczy CRC
   nad buforem z **zerowym** polem CRC (`make_bios_cfg.py:42`); jeśli BIOS liczy
   inaczej, cały plik jest odrzucany i wracają domyślne (`auto_tos=0`),
3. czy `g_config_loaded` (`hw/config.c:9`) jest ustawiane — to jednobajtowy
   wskaźnik, który od razu powie, czy problem jest w znalezieniu pliku, czy
   w jego treści.

Dopiero po tym ma sens ponowny test startu EmuTOS-a: cała reszta ścieżki
(RAM-TOS zbudowany naszym toolchainem, loader rozpoznający nagłówek TOS,
pętla `main.c:118` wybierająca `loadable`) jest już gotowa i zmierzona.

---

# RUNDA: plik konfiguracji jest poprawny — bloker jest w jego SZUKANIU

## Co ustaliłem twardo (mierzalne, powtarzalne)

**`work/BIOS.CFG` jest w pełni poprawny.** Weryfikacja bajt po bajcie:

```
magic   = 0x41534643   ('ASFC' — zgodne z CONFIG_MAGIC, include/config.h:5)
version = 2            (zgodne z CONFIG_VERSION)
size    = 128          (zgodne z sizeof(bios_config_t), _Static_assert 128)
CRC     = 0x83940C63   <- zapisane == wyliczone  (ZGODNE)
auto_tos    (@26) = 1
boot_valid  (@25) = 1
boot_id     (@23) = 128   (IDE master, 0x80)
```

**Algorytm CRC jest identyczny po obu stronach** — narzędzie
(`tools/make_bios_cfg.py:5-9`) i BIOS (`include/config.h:58-67`) pomijają bajty
8..11 i liczą `sum = rol(sum,1) ^ bajt`. Zero rozbieżności.

**Reguły walidacji** (`include/config.h:78-82`) to dokładnie: `magic`,
`version`, `size == sizeof`, `crc` — i **wszystkie cztery są spełnione**.

Prawdziwe offsety struktury (odczytane kompilatorem na hoście, bo nagłówek jest
host-testowalny): `magic 0, version 4, size 6, crc 8, boot_device 12,
video_driver 13, video_mode 14, safe_mode 15, debug_level 16, language 17,
blitter 21, boot_delay 22, boot_id 23, boot_part 24, boot_valid 25,
auto_tos 26, video_pending 27, cpu_clock 28, boot_order 29, boot_order_valid 33`
— narzędzie trafia we **wszystkie** te pola poprawnie.

**Wniosek:** skoro plik jest ważny, a pomiar pokazał `auto_tos = 0` i
`boot_sel_valid = 0`, to BIOS **nie dociera** do tego pliku. Bloker siedzi
w `cfg_load()` / `cfg_try_device()` (`hw/config.c:272-300`): kolejność
urządzeń, `acsi_inspect_device`, `cfg_mount`, `cfg_bios_dir_open`,
`cfg_read_valid` — albo fallback `\BIOS.CFG` z katalogu głównego.

## Pułapka narzędzia (znaleziona po drodze)

`tools/install_boot_sys.py` instaluje plik **pod nazwą pliku źródłowego**.
`/tmp/BIOS4.CFG` → `BIOS/BIOS4.CFG`, a BIOS szuka dokładnie `BIOS.CFG`.
`Makefile:1083` używa `/tmp/BIOS.CFG`, więc tam działa przypadkiem; każda inna
nazwa cicho nie działa i wygląda jak „konfiguracja zignorowana".

## Uczciwie o pomiarze, który się nie udał

Ostatni odczyt (`g_config_loaded`, `g_config_error`, pola `g_config`,
`cfg_id/cfg_part/cfg_mounted/cfg_bios_cluster`) wrócił **w całości `None`** —
debugger nie odczytał **nawet** `g_config_loaded`, więc to **awaria pomiaru**,
a nie dowód o konfiguracji. Odczyt `read_bytes` przez harness wracał pusto
w tej sesji wielokrotnie; nie wolno na takim wyniku opierać wniosku.

## Następny krok (niezależny od kapryśnych odczytów)

Wypisać diagnozę **na konsolę BIOS-u** (ona renderuje się poprawnie na dobrej
bazie) zamiast polegać na debuggerze:

* `g_dbg_cfg_device` — które id urządzenia było próbowane,
* `g_dbg_cfg_part` — która partycja,
* `g_dbg_cfg_mounted` — czy `cfg_mount()` się udało,
* `g_dbg_cfg_bios_dir` — czy znaleziono katalog `\BIOS`,
* `g_dbg_cfg_read_ok` — czy `cfg_read_valid()` przyjął plik.

Jedno uruchomienie wskaże, na którym z tych pięciu kroków szukanie się urywa —
bez zgadywania i bez zależności od odczytu po symbolach.

---

# RUNDA: konfiguracja ZNALEZIONA, auto-TOS AKTYWNY — panic w tej ścieżce

## Twardy pomiar (odczytany po panice, więc bez pościgu za ekranem)

```
g_dbg_cfg_device  = 128     <- probowane urzadzenie: IDE master (0x80)
g_dbg_cfg_probe   = 1       <- jedno urzadzenie przeszlo block_probe
g_dbg_cfg_inspect = 1       <- acsi_inspect_device: OK
g_dbg_cfg_part    = 0       <- partycja 0
g_dbg_cfg_mounted = 1       <- cfg_mount: OK
g_dbg_cfg_dir     = 1       <- katalog \BIOS znaleziony
g_dbg_cfg_read    = 1       <- cfg_read_valid PRZYJAL plik
g_dbg_cfg_error   = 0
g_config_loaded   = 1       <- KONFIGURACJA WCZYTANA
boot_valid (@25)  = 1
auto_tos   (@26)  = 1       <- SCIEZKA AUTO-TOS JEST WLACZONA
boot_id    (@23)  = 128
```

**To zamyka wątek konfiguracji.** Poprzednie niepowodzenia miały jedną
przyczynę: plik musi leżeć w **`\BIOS\BIOS.CFG`**. Kopia w katalogu głównym
(`\BIOS.CFG`), którą instalowałem wcześniej, **nie wystarczała** — mimo że
`hw/config.c:13` opisuje ją jako fallback. Dopiero instalacja przez
`install_boot_sys.py … --directory BIOS` (i **z nazwą `BIOS.CFG`** — narzędzie
bierze nazwę pliku źródłowego!) dała `g_config_loaded=1`.

## Co to odsłoniło

Skoro `auto_tos = 1`, pętla `main.c:118-126` **działa** — i przy naszej
konfiguracji kończy się **panic**:

```
Panic: Bus Error
addr=00e0012c  sr=2304  pc=0015fba
text=00000000 data=00000000 bss=00000000
```

Zrzut: `zrzuty/44-panic-w-sciezce-auto-TOS.png`. Adres `0x00e0012c` leży
w obszarze ROM-u (BIOS jest pod 0xE00000), a `pc=0x0015fba` w niskim RAM-ie.

To jest **nowy, konkretny cel**: awaria jest w ścieżce auto-TOS, czyli w
`tos_catalog_scan()` albo `tos_load_once()` na naszym obrazie EmuTOS-a —
dokładnie tam, gdzie wcześniej nie dało się dojść, bo konfiguracja była
odrzucana.

## Uwaga metodologiczna (na przyszłość)

Ekran paniki **czyści konsolę**, więc linia `CFG-DBG` wypisana wcześniej jest
kasowana. Dlatego diagnostykę należy zostawiać w **globalnych symbolach**
(czyta je debugger po wystąpieniu awarii) — dopiero to dało jednoznaczny wynik,
po kilku nieudanych próbach odczytu „na gorąco".

## Następny krok

1. Odczytać `g_tos_load_reason` po panice — powie, czy `tos_load_once()` w ogóle
   weszło (0 = nie weszło, więc winny jest `tos_catalog_scan`).
2. Zawęzić panic: `pc=0x0015fba` w BIOS-ie wskazuje funkcję — `addr2line` na
   `build/bios_mste.elf` da nazwę pliku i linii, bez zgadywania.

---

# WNIOSEK RUNDY: EmuTOS został ZAŁADOWANY i zaczął się wykonywać

## Analiza adresu awarii (wnioskowanie z adresów — nie obserwacja wprost)

```
Panic: Bus Error   pc=0x0015fba   addr=0x00e0012c   text=0 data=0 bss=0
```

* `__bss_end` BIOS-u leży pod `0x3a70` (mapa linkera), więc **pc nie jest w BIOS-ie**,
* nasz obraz EmuTOS-a jest ładowany pod `base = 0x00002140` i ma 260 282 B,
  czyli zajmuje `0x2140 .. 0x041BE` (≈ 262 KB),
* `0x0015fba` mieści się **w tym zakresie**: `0x15fba − 0x2140 = 0x13E7A`,
  czyli około 90 KB **w głąb obrazu EmuTOS-a**.

To znaczy — i traktuję to jako **mocne wnioskowanie, nie dowód wprost** — że
`tos_load_once()` **skopiował obraz pod 0x2140 i skoczył pod entry z nagłówka**,
a `ram_image_start` zrobił swoje. **EmuTOS zaczął się wykonywać** i po kilku
tysiącach bajtów własnego kodu trafił na Bus Error.

## Dlaczego to jest kamień milowy

Przez całą sesję ścieżka „prawdziwy TOS pod ATOS" była zablokowana kolejno:
1. toolchain (nieprawdziwy bloker — `ELF=1`),
2. brak obrazu linkowanego w RAM (`TARGET_PRG` → `EMUTOS_LIVES_IN_RAM`),
3. loader nie rozpoznawał nagłówka TOS (dodane),
4. **konfiguracja BIOS-u nie była wczytywana** (`boot_valid=0`, a potem plik
   w złym katalogu).

Teraz **wszystkie cztery są rozwiązane i zmierzone**: `g_config_loaded=1`,
`auto_tos=1`, obraz załadowany, CPU w jego kodzie.

## Następny krok

Awaria jest już wewnątrz EmuTOS-a, nie w naszym loaderze. Trzeba ustalić, czego
EmuTOS oczekuje, a czego nie dostaje — najbardziej prawdopodobne:
`addr=0x00e0012c` (obszar ROM-u, w którym siedzi **nasz BIOS**) sugeruje, że
EmuTOS albo próbuje **czytać ROM systemowy** w miejscu, gdzie oczekuje własnego
obrazu, albo pisze do rejestru sprzętowego, którego nie ma.

Praktyczne kroki:
1. odczytać `g_tos_load_reason` po panice (czy `tos_load_once` doszło do końca),
2. porównać `addr` z mapą naszego BIOS-u (`build/bios_mste.map`) — co dokładnie
   leży pod `0x00e0012c`,
3. rozważyć `-DTARGET_256 -DTARGET_PRG` bez `ROMSIZE` — czyli obraz bez paddingu
   ROM-u, który EmuTOS traktuje konsekwentnie jako „żyjący w RAM".
