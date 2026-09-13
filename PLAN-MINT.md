# PLAN-MINT: MiNT na naszym BIOS-ie (droga B)

Cel: **wielozadaniowe środowisko graficzne TOS/GEM** bez pisania GEM-u od zera.
Nasz BIOS daje sprzęt (TF536, TT-RAM, ET4000/CRTC, IDE/ACSI, boot z FAT),
EmuTOS daje kompletne API TOS-u w RAM, MiNT daje wielozadaniowość i ochronę
pamięci, a NVDI/fVDI dają akcelerację ET4000/W32i.

## Architektura docelowa

    +-------------------------------------------+
    |  aplikacje TOS/GEM                        |
    +-------------------------------------------+
    |  AES/VDI  (EmuTOS)  + NVDI/fVDI (2D accel)|
    +-------------------------------------------+
    |  MiNT (scheduler, pamięć, MP na MMU 030)  |
    +-------------------------------------------+
    |  GEMDOS / BIOS / XBIOS  (EmuTOS w RAM)    |
    +-------------------------------------------+
    |  NASZ BIOS + loader  (TF536, ET4000, IDE) |
    +-------------------------------------------+

## Faza 0 - rozpoznanie (bez zmian w kodzie)
- [ ] jakie obrazy EmuTOS mamy (192k/256k) i czy sa bootowalne jako TOS,
- [ ] czy nasz loader/tos.c potrafi zaladowac obraz TOS do RAM i skoczyc,
- [ ] czego EmuTOS wymaga od BIOS-u (wektory, _sysbase, cookie jar),
- [ ] czy TF536/IDE jest widziane przez EmuTOS (najwieksze ryzyko).

## Faza 1 - EmuTOS przez nasz loader (rozstrzygajaca)
Cztery sprawdzenia po bootcie:
1. [ ] konsola/klawiatura,
2. [ ] ET4000 w GEM (tryb graficzny, mysz),
3. [ ] dysk C: (IDE/ACSI) - czy EmuTOS montuje,
4. [ ] mysz w GEM.
Wynik rozstrzyga dalej:
- dziala -> Faza 2,
- nie dziala dysk -> piszemy TYLKO sterownik IDE/XBIOS (nie caly GEM).

## Faza 2 - MiNT na EmuTOS
- [ ] dobrac wersje MiNT (1.x dla ST/030),
- [ ] MINT.PRG + konfiguracja (MINT.CNF) na obrazie,
- [ ] sprawdzic: uruchomienie dwoch procesow, przelaczanie, stabilnosc,
- [ ] sprawdzic ochrone pamieci (030 ma MMU; ROM zmapowany - nasz BIOS jest w ROM).

## Faza 3 - akceleracja 2D (ET4000/W32i)
- [ ] potwierdzic, ktory sterownik ma akceleracje W32(i): NVDI 4/5 albo fVDI,
- [ ] zainstalowac pod MiNT-em i zmierzyc (wypelnienie/blit) - porownac z 1:1 CPU,
- [ ] jesli brak: napisac tylko driver 2D dla W32i (BitBlt/fill przez rejestry),
      a nie caly GEM.

## Faza 4 - zbieznosc (docelowo)
Nasz kernel zostaje jako **warstwa sprzetowa + loader**; stopniowo przejmujemy
dolne warstwy EmuTOS-a (albo zostawiamy EmuTOS jako dostawce API).
Mozliwe warianty:
- B1: BIOS -> EmuTOS (RAM) -> MiNT            (najtanszy, najbezpieczniejszy),
- B2: BIOS -> nasz kernel -> AES/VDI z EmuTOS + MiNT  (wiecej kontroli),
- B3: wszystko wlasne (droga A)               (najwiekszy koszt, juz znamy).

## Ryzyka i fallbacki
| Ryzyko | Fallback |
|---|---|
| EmuTOS nie widzi IDE TF536 | wlasny XBIOS-owy sterownik blokowy (maly) |
| EmuTOS nie obsluguje ET4000 | mamy wlasny driver ET4000 (hw/nova.c, video8.c) - podpiac jako VDI |
| MiNT wymaga ukladu pamieci TOS | oddac mu RAM w ukladzie TOS (faza 2, glowne ryzyko) |
| NVDI/fVDI bez akceleracji W32i | napisac tylko driver 2D (BitBlt) |

## Zasady pracy (z doswiadczenia tej sesji)
1. Zmiana -> test na Hatari -> wnioski. Nigdy "na raz".
2. "Raz dziala, raz nie" -> najpierw surowe bajty, potem zmiana.
3. Nigdy ciezka praca w przerwaniu (Timer C).
4. Nie robic zapisow szerszych niz szyna (MegaBus 16-bit) bez testu.
5. BSS rosnij oszczednie (duze tablice psuly zagniezdzony Pexec).
6. Identyczne zrzuty = zamrozony ekran, nie "program skonczyl rysowac".

## Faza 0 - WYNIK ROZPOZNANIA (ustalone z kodu)

1. **Obrazy sa**: atari-030-port/disk/emutos-pl-192k.img (196 608 B) i
   emutos-pl-256k.img (262 144 B).
2. **BIOS MA JUZ menu bootowania EmuTOS z ACSI** - ui/tos.c:
   console_puts("LOADING EMUTOS FROM ACSI..."); tos_load_once(...)
   Katalog (loader/tos.c:tos_catalog_scan) skanuje korzen partycji FAT.
3. **Loader zna adresy ROM**: 192K -> 0x00FC0000, 256K/512K -> 0x00E00000
   (loader/tos.c:85-86), ale dla obrazu ROM ustawia tylko `compatible`,
   NIE `loadable` -> menu pokazuje "ROM MAP" i ui/tos.c:27 odmawia:
   "ROM IMAGE NEEDS MMU OR ROM BANK".

### Wniosek: brakuje JEDNEJ rzeczy do fazy 1
Obraz TOS/EmuTOS trzeba **zmapowac pod 0xE00000**, a to wymaga albo:
- **(a) MMU 68030** - maly uklad stron: TC/TT + kilka deskryptorow mapujacych
  0x00E00000..0x00E3FFFF na blok RAM, potem skok pod 0xE00000.
  68030 ma MMU, wiec to wykonalne i dobrze udokumentowane. To jest **praca
  do zrobienia** (kilkadziesiat linii assemblera + tabela).
- **(b) ROM/flash TF536** - wgrac obraz EmuTOS do flasha karty (krok sprzetowy),
  wtedy EmuTOS startuje jako ROM bez MMU i bez loadera.

### Rekomendacja
Najpierw **(b)** jako proba "czy to w ogole wstaje na naszym sprzecie":
najtansza, rozstrzyga o calym kierunku. Jesli sprzet (IDE/ET4000) dziala pod
EmuTOS-em, wtedy **(a)** - MMU - zeby moc bootowac EmuTOS z dysku bez
przeprogramowywania karty.

### Kolejnosc prac fazy 1
1. [ ] wgrac/probowac EmuTOS (b) albo zrobic MMU map (a),
2. [ ] boot + konsola/klawiatura,
3. [ ] ET4000 w GEM,
4. [ ] dysk C: (IDE/ACSI) - NAJWIEKSZE RYZYKO,
5. [ ] mysz,
6. [ ] dopiero potem MiNT (faza 2) i NVDI/fVDI (faza 3).


## Faza 1 - SRODOWISKO TESTOWE URUCHOMIONE (wynik)

### Dziala (zweryfikowane zrzutami)
- **EmuTOS 1.4.0** startuje do pulpitu GEM na naszym emulowanym sprzecie:
  68030 @32 MHz, 4 MB ST-RAM + **64 MB TT-RAM (TF536)**, IDE, **ET4000/VOFA**
  (640x480 mono), mysz, menu PL, ikony.
- **C: dziala** - przez `--harddrive <katalog> --gemdos-drive C`:
  katalog hosta `atari-030-port/disk/easy/` jest widziany jako DYSK C:
  (bez partycjonowania i bez obrazu - najwygodniejsza droga instalacji).

Pelna komenda (dziala):
    hatari-tf536 --machine megast --cpulevel 3 --cpuclock 32 --memsize 4 --ttram 64
      --tf536 on --graphics-card vofa --gfx-chip et4000 --mono --zoom 2
      --acsi N=none (0..7)
      --harddrive atari-030-port/disk/easy --gemdos-drive C
      --tos atari-030-port/disk/emutos-pl-256k.img
      --cmd-fifo /tmp/demo_fifo

### Ustalenia o EasyMiNT (z oficjalnego IMPORTAN.TXT)
- EasyMiNT instaluje sie na partycji **ext2** (nie FAT!) + wymaga HDDriver
  z WYLACZONA ochrona root/boot sectora -> to powazna instalacja na obrazie
  dysku, nie kopiowanie plikow.
- Archiwa: EM_x_x.ZIP (instalator), EASYMINT.ZIP (kernel+drivery),
  ROOTFS.TGZ (root fs), BASIC.TAR (rudymentarny MiNT).
- Lustro: ftp.pigwa.net/stuff/mirror/www.atari.cuprum.com.pl/easymint/
  (curl z bash dziala - sprawdzone).

### Rekomendacja
Dwie drogi (do wyboru w nastepnej sesji):
- **(1) FreeMiNT bezposrednio** (kernel + XaAES) z dysku **GEMDOS/FAT**
  -> prostsze, bo mamy C: z katalogu hosta; brak ext2. Ryzyko: mniej UNIX-owych
  ficzerow, ale okna GEM dzialaja.
- **(2) EasyMiNT** -> pelna dystrybucja, ale wymaga obrazu z ext2 + HDDriver.

### Nastepny krok
1. [ ] wybrac (1) lub (2),
2. [ ] pobrac (curl) i rozpakowac do atari-030-port/disk/easy/ (dla 1)
       albo przygotowac obraz ext2 (dla 2),
3. [ ] AUTO/MINT.PRG + MINT.CNF (030, TT-RAM),
4. [ ] boot MiNT -> **okna GEM** (czesc 1 celu),
5. [ ] promocja ekranu: gry/dema w okienkach (czesc 2 celu).


## Faza 2 - MiNT: wynik i brakujacy element (Linux partition)

### Ustalone
- **FreeMiNT 1.19 startuje** na EmuTOS + TF536 (030@32, 4/64 MB) + ET4000:
  jadro, moduly, update daemon, idle process, awaryjny shell MiS.
- MiNT **wymaga partycji linuxowej** jako swojego UNIX-owego roota (u:).
  Ten build ma: **ext2.xfs** i minix.xfx -> oba sa obslugiwane.
- Bez tego: "parse_cnf: can't open u:/C/mint/1-19-4eb/mint.cnf" (u: to root
  UNIX, nie litera dysku) i brak shella -> awaryjny MiS.
- **mke2fs 1.47.2 dziala** (jest w /Users/xxx/Downloads/platform-tools/):
  utworzono testowy obraz ext2 64 MB:
    mke2fs -q -t ext2 -F -L minix -b 1024 -I 128 part.img 65536
  (parametry 1024B blok / 128B inode = stare, zgodne z MiNT).
- **Brakuje**: narzedzia do WPISANIA plikow do obrazu ext2.
  Nie ma debugfs/e2cp. Opcje:
    a) brew install e2fsprogs  -> daje debugfs -w / e2cp
    b) mount ext2 przez FUSE na macOS
    c) wlasny zapis ext2 w Pythonie

### Plan nastepnego kroku
1. [ ] zdobyc writer ext2 (a/b/c),
2. [ ] wgrac do partycji zawartosc sys-root + /mint z FreeMiNT,
3. [ ] zbudowac obraz AHDI: tablica partycji (format jak w install_files_ahdi.py)
       + partycja ext2,
4. [ ] MiNT: mount ext2 jako u: (mint.cnf) i start,
5. [ ] **GEM=XaAES -> okna GEM** (czesc 1 celu),
6. [ ] potem promocja ekranu: gry/dema w okienkach (czesc 2 celu).

### Stan dysku testowego
atari-030-port/disk/easy/ = C: dla EmuTOS (GEMDOS drive) z FreeMiNT,
auto/mint-4eb.prg + auto/mint030.prg, mint/1-19-4eb/{mint.cnf, ext2.xfs, minix.xfx, xaaes/...}


## Faza 2 - ustalenia z oficjalnego doc/INSTALL FreeMiNT (release-1_15_12)

- Instalacja: **jadro MiNT do katalogu AUTO** (mamy: auto/mint030.prg) - OK.
- **"NVDI must always be started before MiNT"** -> dla fazy 3 (akceleracja
  ET4000/W32i) NVDI/fVDI musi byc ladowany PRZED jadrem MiNT.
- WDIALOG: ladowany PO MiNT (najlepiej po AES); mozna pominac (N.DIALOG).
- Freedom: nie uzywac (niestabilny).
- Naming: mint000.prx (ST), mint030.prx (030), mint040/060; mintnp = bez MP.

### Otwarte: root 'u:' MiNT-a
- MiNT wymaga roota UNIX jako 'u:'. Sciezka z bledu: u:/C/mint/1-19-4eb/mint.cnf
- Podlaczenie partycji Minix jako ACSI 0 NIE zmienia roota: root = urzadzenie,
  z ktorego zaladowano jadro (u nas GEMDOS C:), a nie dowolna partycja.
- Utworzenie katalogu C/... w korzeniu C: nie pomoglo -> mapowanie 'u:' nie jest
  rownowazne <root>/C.
- Wniosek: trzeba znalezc poprawny model roota dla tej wersji (UNIXMODE /
  boot menu w nowszych jadrach / kopiowanie AUTO na partycje Minix).
  Najpewniej: nowsze jadro MiNT (te ma boot menu i pozwala wybrac root/urzadzenie).

### Narzedzie dodane
tools/make_minix_ahdi.py - tworzy obraz AHDI + partycje Minix v1 (magic 0x138F,
30-znakowe nazwy) i wypelnia ja drzewem. Zweryfikowane: checksum AHDI 0x1234.


## Faza 2 - DWA PARTYCJE + boot: wynik

### Zbudowane i dzialajace
- **tools/make_fat_ahdi.py** - AHDI + partycja FAT16 (BPB little-endian, tablica
  big-endian, checksum 0x1234). **Zgodny z install_files_ahdi.py** (wgral pliki).
- **tools/make_minix_ahdi.py** - AHDI + partycja Minix v1 (magic 0x138F).
- ID partycji zmienione na standardowe: FAT -> **"GEM"**, Minix -> **"MIX"**
  (wczesniej "FAT"/"MIN" - EmuTOS nie widzial dysku).
- Po poprawce ID EmuTOS startuje z **naszej partycji FAT** i uruchamia AUTO
  (loader MiNT + mint030.prg).

### Blad do rozwiazania
- Podczas startu MiNT: **Bus Error reading at address $400000** (pierwszy bajt
  za 4 MB ST-RAM) + reset trybu graficznego (VOFA-MODE w=0 h=0), ekran czarny.
- Podejrzenie: inicjalizacja pamieci w MiNT z TF536 (4 MB ST + 64 MB TT-RAM) -
  detekcja ALT-RAM / KERBPOOL / uklad pamieci TOS.
- Nastepne proby: warianty jadra (mint030/mint000), ustawienia pamieci w
  MINT.CNF, porownanie z dzialajacym EasyMiNT na TF536 (uzytkownik ma).

### Uklad testowy
    --acsi 0 = atari-030-port/disk/boot_fat.img   (GEM/FAT16: AUTO + mint.cnf)
    --acsi 1 = atari-030-port/disk/mint_minix.img (MIX/Minix v1: root)
    --harddrive NIE uzywac (MiNT nie widzi dysku GEMDOS z Hatari)
    --tos atari-030-port/disk/emutos-pl-256k.img


## Faza 2 - diff z dzialajaca referencja (bardzo wazne)

Porownalem moj sektor root z `outputs/atari_system_clean_768.img` (dziala):
- struktura AHDI **identyczna**: entry flag=1, id=BGM, start=1, checksum=0x1234
- rozne sa tylko: (a) referencja ma DRUGA partycje (normalne - 768 MB),
  (b) inne slowo checksum (oba sumuja sie do 0x1234 - oba poprawne)
=> **moja tablica AHDI jest poprawna.** Poprawilem tez wczesniej:
   id GEM->BGM, start 2->1, total16=0/total32=size, spc=8 (jak referencja).

### Ale: nadal brak ikony C: i 8x "Bus Error at $400000"
- $400000 = pierwszy bajt za 4 MB ST-RAM, PC w ROM-ie EmuTOS.
- Wyglada na to, ze AHDI/EmuTOS uzywa bufora DMA **powyzej** dostepnej pamieci
  -> bus error -> skan partycji sie nie konczy -> brak C:.
- Test BEZ --tf536: bus errory nadal sa -> to NIE wina TT-RAM/TF536.

### Nastepne proby (konkret)
1. sprawdzic, czy EmuTOS z Hatari poprawnie raportuje rozmiar ST-RAM
   (i czy AHDI nie bierze bufora z _memtop+),
2. sprobowac inny rozmiar pamieci (--memsize 4 vs inne) i porownac,
3. najpewniejsze: uzyc **gotowego, dzialajacego obrazu** z repo
   (outputs/atari_system_clean_768.img ma 2x BGM i AHDI) jako nosnika
   boot/AUTO, zamiast wlasnego obrazu - wtedy sprawdzimy sam MiNT.


## Faza 2 - gotowy obraz z sieci (proba)

Znalazlem **gotowa dystrybucje FreeMiNT** dla 32-bitowych Atari:
  github.com/rootadm79/freemint_os_distro  ("FreeMiNT OS Distro 26.2")
  - "Ready-to-boot", wspiera **Hatari (experimental)**, min. 68030 + 14 MB,
  - obraz na https://ns.mpsoftware.cz/filehosting/download.php
  - README: instalacja "Download the image" + dd na karte SD/CF.

### Wynik proby
- curl z download.php pobral **232 783 872 B** (222 MB) - ale to **NIE jest
  obraz dysku AHDI**:
  - brak tablicy AHDI (checksum 0x6DB7, nie 0x1234; wpisy to smieci),
  - brak jakichkolwiek stringow "MiNT"/"xaaes"/"HDDriver",
  - zawartosc wyglada na zakodowana/obfuskowana (bajty przypominajace base64),
  - strona "filehosting" ma najpewniej warstwe JS/anty-bot.
- Wniosek: ten kanal nie da sie uzyc automatycznie.

### Nastepne kandydaci na gotowy obraz (do sprawdzenia)
1. **MiSTer "Ready-To-Run VHDs"** (mail.misterfpga.org) - obrazy HDD dla
   rdzenia Atari ST (maja sterowniki + partycje),
2. watki atari-home.de o obrazach dyskow z EmuTOS (np. "9,3 MB Diskettenimages
   fuer den Hatari mit Emutos 1.3"),
3. skopiowac dzialajaca karte/obraz uzytkownika (EasyMiNT na TF536) - najpewniejsze.

### Wazne ustalenie z testow (do zapamietania)
- Bus errory przy starcie (8x, $400000) to **normalna sonda pamieci** EmuTOS -
  NIE sa przyczyna problemow z dyskiem (sa tez bez dyskow i bez TF536).
- Ani ACSI, ani IDE **nie montuja** partycji w tym EmuTOS - brak sterownika HDD
  w sektorze bootujacym. C: widziane wczesniej pochodzilo z emulacji GEMDOS
  w Hatari (--harddrive), a nie z prawdziwego sterownika.

