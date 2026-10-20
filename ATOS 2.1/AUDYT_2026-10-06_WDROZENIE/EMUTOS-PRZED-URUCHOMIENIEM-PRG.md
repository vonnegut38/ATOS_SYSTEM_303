# Co EmuTOS musi zainicjować, zanim uruchomi program TOS (.PRG)

**Analizowane drzewo źródłowe (tylko do odczytu):**
`files-pasted-by-the-user-projekt/work/emutos-reference` (kopia: `commander/work/emutos-reference`)

**Konwencja odnośników:** wszystkie odnośniki są ścieżkami względnymi wobec korzenia drzewa, w formie
`plik:linia`. Wszystkie cytaty kodu pochodzą z tego drzewa.

---

## 0. Ustalenia wstępne, bez których reszta raportu jest nieczytelna

### 0.1 Podwójne nazwy: `hz_200` i `_hz_200` to ten sam obiekt

EmuTOS buduje C z opcją `-fleading-underscore`:

```
Makefile:211: TOOLCHAIN_CFLAGS = -fleading-underscore -Wa,--register-prefix-optional -fno-reorder-functions --param=min-pagesize=0 -DELF_TOOLCHAIN
```

Dzięki temu symbol C `hz_200` i symbol linkera (i asemblera) `_hz_200` to **ten sam** adres. Dlatego
`tosvars.ld` definiuje `_hz_200`, asembler używa `_hz_200`, a C używa `hz_200`. Adresy są **absolutne,
zaszyte w skrypcie linkera**:

```
tosvars.ld:10:  * The TOS system variables have been officially documented by Atari.
tosvars.ld:11:  * They have fixed addresses in the 0x380-0x800 region, so they are only
tosvars.ld:12:  * accessible in supervisor mode.
```

### 0.2 Obszar sysvars: 0x380–0x800, zerowany przy pierwszym rozruchu

```
emutos.ld:69:     .first_stram :
emutos.ld:70:     {
emutos.ld:71:         /* System variables are located at fixed addresses */
emutos.ld:72: #include "tosvars.ld"
emutos.ld:73:         . = 0x800; /* Extend sysvars area upto end of supervisor-only RAM */
emutos.ld:74:         sysvars_end = .;
emutos.ld:75:     } >stram
```

```
bios/memory.S:566:         /* Clear the sysvars */
bios/memory.S:567:         move.l  #sysvars_start.w, a0
bios/memory.S:568:         move.l  #sysvars_end, a1
bios/memory.S:569:         jbsr    call_bzero_a0_a1
```

Wniosek praktyczny: **każda zmienna systemowa, której EmuTOS jawnie nie ustawi, ma wartość 0**
(na zimnym rozruchu), a na ciepłym rozruchu zachowuje wartość z poprzedniego uruchomienia.

### 0.3 Dwa różne momenty „uruchomienia .PRG" — to jest kluczowe dla całego raportu

Kolejność rozruchu (`bios/bios.c:808`):

```
bios/bios.c:813:     bios_init();                /* Initialize the BIOS */
...
bios/bios.c:852:     blkdev_boot();
bios/bios.c:854:     Dsetdrv(bootdev);           /* Set boot drive */
bios/bios.c:855:     init_default_environment(); /* Build default environment string */
...
bios/bios.c:873:     autoexec();                 /* autoexec PRGs from AUTO folder */
bios/bios.c:877:     if(cmdload != 0) {
bios/bios.c:882:         Pexec(PE_LOADGO, "COMMAND.PRG", "", NULL);
bios/bios.c:883:     } else if (exec_os) {
bios/bios.c:889:         pd = (PD *) Pexec(PE_BASEPAGEFLAGS, (char *)PF_STANDARD, "", default_env);
bios/bios.c:890:         pd->p_tbase = (UBYTE *) exec_os;
bios/bios.c:892:         Pexec(PE_GO, "", (char *)pd, default_env);
```

- **Kontekst A (przed AES)** — programy z folderu `\AUTO\` (`bios/bios.c:741`) oraz `COMMAND.PRG`
  (`bios/bios.c:882`) uruchamiane są **zanim** wystartuje główny UI. Tu działa tylko BIOS + GEMDOS.
  `appl_init()` **nie ma prawa działać**, bo wektor TRAP #2 nie wskazuje jeszcze AES-u.
- **Kontekst B (po AES)** — programy uruchamiane z pulpitu (EmuDesk/EmuCON) przez `Pexec()` z
  `aes/gemshlib.c:645` działają przy w pełni zainicjowanym AES i VDI.

Interfejs główny wskazywany jest przez `os_header.os_magic->gm_init`:

```
bios/bios.c:558:     /* Set start of user interface.
bios/bios.c:559:      * No need to check if os_header.os_magic->gm_magic == GEM_MUPB_MAGIC,
bios/bios.c:560:      * as this is always true. */
bios/bios.c:561:     exec_os = os_header.os_magic->gm_init;
```

```
bios/endrom.c:26: const GEM_MUPB ui_mupb =
bios/endrom.c:27: {
bios/endrom.c:28:     GEM_MUPB_MAGIC, /* Magic value identifying this structure */
bios/endrom.c:29:     _end_os_stram,  /* End of the static ST-RAM used by the OS */
bios/endrom.c:30: #if WITH_AES
bios/endrom.c:31:     ui_start        /* AES entry point */
```

a `_ui_start` to `aes/gemstart.S:46`, skąd wołane jest `_gem_main` (`aes/gemstart.S:119`).

---

## 1. Zmienne systemowe w niskiej pamięci (0x0000–0x0800)

Pełna, autorytatywna mapa adresów: `tosvars.ld:22-110`. Poniżej **każda** zmienna istotna przed
startem programu, z podaniem co EmuTOS do niej wpisuje i kto ją czyta.

### 1.1 Strona awaryjna i wektory wyjątków (0x380–0x3CC)

| Adres | Symbol | Znaczenie | Kto pisze | Kto czyta |
|---|---|---|---|---|
| 0x380 | `_proc_lives` | znacznik `0x12345678` = ważny zrzut | `bios/vectors.S:274` | programy diagnostyczne |
| 0x384 | `_proc_dregs` | zrzut D0–D7 | `bios/vectors.S:234` | narzędzia crash |
| 0x3A4 | `_proc_aregs` | zrzut A0–A6/SP | `bios/vectors.S:241` | narzędzia crash |
| 0x3C4 | `_proc_enum` | numer wyjątku | `bios/vectors.S:246` | narzędzia crash |
| 0x3C8 | `_proc_usp` | USP w chwili wyjątku | `bios/vectors.S:237` | narzędzia crash |
| 0x3CC | `_proc_stk` | 16 słów stosu wyjątku | `bios/vectors.S:272` | narzędzia crash |

Nie są wymagane do poprawnego startu programu, ale EmuTOS **zakłada, że może je nadpisać** w każdej
chwili — dlatego `_init_user_vec` na ciepłym rozruchu inicjalizuje tylko pierwsze 64 wektory, żeby
nie zniszczyć zrzutu:

```
bios/vectors.S:120: /* Initialize the user vectors.
bios/vectors.S:121:  * On a cold boot, all 192 user vectors are initialized.
bios/vectors.S:122:  * On a warm boot, only the first 64 vectors are reinitialized.
bios/vectors.S:123:  * This avoids overwriting crash dumps starting at address 0x380.
```

### 1.2 Tabela zmiennych 0x400–0x5B0

Adresy z `tosvars.ld`. Kolumna „Wartość w EmuTOS" podaje wartość **po zakończeniu `bios_init()`
i `osinit_*`, a przed startem pierwszego programu z AUTO** — czyli w kontekście A. Różnice dla
kontekstu B podano w sekcji 5.

| Adres | Symbol | Znaczenie | Wartość / gdzie ustawiane | Czytane przez |
|---|---|---|---|---|
| 0x400 | `_etv_timer` | wektor timera GEM | `bios/bios.c:360` = `just_rts`, potem **`bdos/time.c:166` = `tikfrk`** | `bios/vectors.S:542` (`jsr (a0)`) |
| 0x404 | `_etv_critic` | handler błędu krytycznego GEM | `bios/bios.c:361` = `default_etv_critic` | `bios/vectors.S:54`; `bios/blkdev.c:520` |
| 0x408 | `_etv_term` | wektor zakończenia programu GEM | `bios/bios.c:362` = `just_rts` | `bdos/proc.c:606` (`Setexc(0x102,-1L)`) |
| 0x40C | `_etv_xtra` | dodatkowe wektory GEM (nieużywane) | zerowane | — |
| 0x420 | `_memvalid` | konfiguracja pamięci ważna | `bios/memory.S:584` = `0x752019f3` | `bios/memory.S:135` (wykrycie ciepłego startu) |
| 0x424 | `memctrl` | kopia 0xffff8001 | `bios/memory.S:572` | `bios/memory.S:146` |
| 0x426 | `resvalid` | ważność `resvector` | 0 (nie ustawiane) | `bios/startup.S:507` |
| 0x42A | `resvector` | wektor resetu | 0 (nie ustawiane) | `bios/startup.S:510` |
| **0x42E** | **`_phystop`** | fizyczny szczyt ST-RAM | `bios/memory.S:573` (`move.l d5,_phystop.w`) | `bios/biosmem.c:72`; boot-sector `util/bootsect.S` |
| **0x432** | **`_membot`** | początek TPA | `bios/biosmem.c:66-67` (`end_os = _end_os_stram; membot = end_os;`) | `bios/biosmem.c:145` (`getmpb`) → GEMDOS `pmd` |
| **0x436** | **`_memtop`** | koniec TPA | `bios/biosmem.c:72` = `phystop`, następnie **zmniejszane** przez `balloc_stram(size,TRUE)` przy alokacji ekranu (`bios/biosmem.c:115`) | `bios/biosmem.c:146` → GEMDOS `pmd` |
| 0x43A | `_memval2` | ważność memctrl/memconf | `bios/memory.S:585` = `0x237698aa` | `bios/memory.S:137` |
| 0x43E | `_flock` | blokada VBL dla FDD | 0 | `bios/acsi.c:369,377` |
| 0x440 | `_seekrate` | szybkość stepowania FDD | `bios/floppy.c:309` = `DD_STEPRATE_3MS` | `bios/floppy.c:294` |
| 0x442 | `_timer_ms` | okres timera w ms | `bios/mfp.c:196` = `20` | `bios/vectors.S:541` |
| 0x444 | `_fverify` | weryfikacja zapisu FDD | `bios/floppy.c:308` = `0xff` | `bios/floppy.c:1200` |
| **0x446** | **`_bootdev`** | dysk startowy | `bios/bios.c:826` (`DEFAULT_BOOTDEV`/`FLOPPY_BOOTDEV`), `bios/bios.c:837` (ekran powitalny), `bios/bios.c:638` (NatFeats) | `bios/bios.c:737`, `bios/blkdev.c:246,277,307`; `aes/geminit.c:859` |
| 0x448 | `_palmode` | 0=NTSC, ≠0=PAL | **EmuTOS nie zapisuje** (tylko czyta `get_default_palmode()` w `bios/screen.c:607`) | starsze programy/loader |
| 0x44A | `_defshiftmod` | domyślny tryb Shiftera | **EmuTOS nie zapisuje** — czytane w `bios/screen.c:1302` | `bios/screen.c:1302` |
| **0x44C** | **`_sshiftmod`** | kopia 0xffff8260 | `bios/screen.c:557,564,580`, `bios/screen.c:1007`, `bios/screen.c:1011`, `bios/screen.c:1327` | `bios/screen.c:791,852,1082` |
| **0x44E** | **`_v_bas_ad`** | adres bazowy ekranu (logiczny) | `bios/screen.c:656` (`screen_init_address`), `bios/screen.c:1107` i `:1139` (`setscreen`) | `bios/vt52.c:930`, `bios/conout.c:126,507,712`, `vdi/vdi_control.c:618`, `vdi/vdi_raster.c:927` |
| 0x452 | `_vblsem` | semafor VBL (>0 → VBL działa) | `bios/bios.c:580` = `1` | `bios/vectors.S:334,338,489` |
| 0x454 | `_nvbls` | liczba slotów VBL | `bios/bios.c:366` = 8 | `bios/vectors.S:440` |
| 0x456 | `_vblqueue` | wskaźnik na tablicę VBL | `bios/bios.c:367` = `vbl_list` | `bios/vectors.S:447`; `vdi/vdi_mouse.c:602,631` |
| 0x45A | `_colorptr` | paleta do załadowania | `bios/screen.c:1182` (`setpalette`) | `bios/vectors.S:367` (VBL, potem zeruje) |
| 0x45E | `_screenpt` | nowy adres ekranu na następny VBL | EmuTOS tylko czyta (`bios/vectors.S:394`); ustawia program | `bios/vectors.S:394-396` |
| 0x462 | `_vbclock` | licznik wykonanych VBL | `bios/vectors.S:348` | programy GEM (animacje) |
| 0x466 | `_frclock` | licznik odebranych VBL | `bios/vectors.S:330` | `bios/screen.c:1217-1218` (`vsync`) |
| 0x46A | `_hdv_init` | wektor init HDD | `bios/blkdev.c:98` = `flop_hdv_init` | `bios/blkdev.c:106` |
| 0x46E | `_swv_vec` | wektor zmiany rozdzielczości | `bios/bios.c:363` = `just_rts`, potem `bios/bios.c:579` = `os_header.reseth` | VBL → `bios/screen.c` (`_detect_monitor_change`) |
| 0x472 | `_hdv_bpb` | wektor getbpb HDD | `bios/blkdev.c:101` = `blkdev_getbpb` | BIOS 7 / GEMDOS |
| 0x476 | `_hdv_rw` | wektor read/write HDD | `bios/blkdev.c:102` = `blkdev_rwabs` | BIOS 4 / GEMDOS |
| 0x47A | `_hdv_boot` | wektor boot HDD | `bios/blkdev.c:95` = `blkdev_hdv_boot` | `bios/blkdev.c:280` |
| 0x47E | `_hdv_mediach` | wektor zmiany nośnika | `bios/blkdev.c:103` = `blkdev_mediach` | BIOS 9 / GEMDOS |
| **0x482** | **`_cmdload`** | ≠0 → załaduj `COMMAND.PRG` | **EmuTOS nigdy nie zapisuje** (zerowane) | `bios/bios.c:877` |
| **0x484** | **`_conterm`** | atrybuty konsoli | `bios/ikbd.c:1100` = `7` (klik, powtórzenia, dzwonek) | `bios/ikbd.c:279,475,495,801,845,894`, `bios/vt52.c:215` |
| 0x486 | `trp14ret` | adres powrotu TRAP #14 | nieużywane | — |
| 0x48A | `criticret` | adres powrotu błędu krytycznego | nieużywane | — |
| 0x48E | `_themd` | pierwszy deskryptor pamięci | `bios/biosmem.c:144-147` | `bdos/umem.c:555` (`Getmpb` → `pmd`) |
| 0x4A2 | `_savptr` | wskaźnik bloku zapisu BIOS | `bios/bios.c:359` = `trap_save_area` | `bios/vectors.S:1283,1317,1335,1356`, `bios/kprint.c:587` |
| 0x4A6 | `_nflops` | liczba stacji FDD | `bios/floppy.c:319` = 0, `:386` `nflops++` | `cli/cmdmain.c:83` (przez `Supexec`), programy |
| 0x4A8 | `_con_state` | stan maszyny VT52 | `bios/vt52.c:190,244,253` | `bios/vt52.c:153` — **sprawdza `!con_state`**, więc musi być poprawnym wskaźnikiem albo 0 |
| 0x4AC | `_save_row` | wiersz dla ESC Y | `bios/vt52.c:263` | `bios/vt52.c:279` |
| 0x4AE | `sav_context` | obszar zapisu wyjątków | nieużywane w C | — |
| **0x4B2** | **`_bufl`** | 2 wskaźniki list buforów GEMDOS | `bdos/fsbuf.c:67` (`bufl[BI_FAT]`), `:71` (`bufl[BI_DATA]`) | `bdos/fsbuf.c:121` |
| **0x4BA** | **`_hz_200`** | licznik 200 Hz | `bios/vectors.S:503` (`addq.l #1,_hz_200.w`) | `bios/acsi.c:122,363`, `bios/mfp.c:175`, `bios/parport.c:147`, `bios/dsp.c:711`, `bios/blkdev.c:535`, `bios/xbios.c:424`, `bios/sd.c:585`; typowe programy (opóźnienia) |
| 0x4BE | `the_env` | wskaźnik domyślnego środowiska | **EmuTOS nie zapisuje** | — |
| **0x4C2** | **`_drvbits`** | bitmapa dysków | `bios/blkdev.c:175` = 0, `:392` `drvbits \|= (1L << logical)` | `bios/blkdev.c:835` (`Drvmap`), `:848`; `cli/cmdint.c:1110` |
| **0x4C6** | **`_dskbufp`** | bufor sektorowy | `bios/blkdev.c:91` = `dskbuf` (bufor w `bios/lowstram.c:40`) | `bios/acsi.c:156,214,308,640`, `bios/blkdev.c:266,309,626`, `bdos/fsbuf.c` |
| 0x4CA | `_autopath` | ścieżka auto-execute | **EmuTOS nie zapisuje** | programy |
| **0x4CE** | **`_vbl_list`** | 8 wskaźników procedur VBL | wpisy zerowane: `bios/bios.c:370-372`; slot 0 zajmuje VDI: `vdi/vdi_mouse.c:602` = `vb_draw` | `bios/vectors.S:448-463` |
| 0x4EE | `_dumpflg` | flaga zrzutu ekranu | `bios/bios.c:357` = `-1`; VBL ją zeruje i ustawia z powrotem: `bios/vectors.S:467,472` | `bios/vectors.S:467` |
| 0x4F0 | `_prtabt` | flaga przerwania drukarki | nieużywane | — |
| **0x4F2** | **`_sysbase`** | wskaźnik początku systemu | `bios/bios.c:358` = `&os_header` | programy sprawdzające wersję TOS |
| 0x4F6 | `_shell_p` | wskaźnik do shella | **EmuTOS nie zapisuje** (0) | starsze programy/shell |
| **0x4FA** | **`_end_os`** | koniec statycznych danych OS | `bios/biosmem.c:66` = `_end_os_stram` | programy alokujące pamięć |
| **0x4FE** | **`_exec_os`** | punkt wejścia UI | `bios/bios.c:561` = `ui_start` | `bios/bios.c:883` |
| 0x502 | `_dump_vec` | procedura zrzutu ekranu | `bios/chardev.c:84` = `just_rts` | `bios/vectors.S:469` |
| 0x506 | `_prt_stat` | status drukarki | `bios/chardev.c:82` = `just_rts` | BIOS 8 |
| 0x50A | `_prt_vec` | wyjście drukarki | `bios/chardev.c:83` = `just_rts` | BIOS 3 |
| 0x50E | `_aux_stat` | status RS232 | `bios/chardev.c:78` = `just_rts` | BIOS 8 |
| 0x512 | `_aux_vec` | wyjście RS232 | `bios/chardev.c:79` = `just_rts` | BIOS 3 |
| 0x516 | `_pun_ptr` | struktura PUN_INFO (AHDI) | `bios/blkdev.c:161` = `&pun_info` | `bdos/fsbuf.c:61`, `bios/xhdi.c:214` |
| 0x51A | `_memval3` | ważność konfiguracji | `bios/memory.S:586` = `0x5555aaaa` | `bios/memory.S:139` |
| 0x51E | `_bconstat_vec` | 8 wskaźników statusu wejścia | `bios/chardev.c:66-67` | BIOS 1 |
| 0x53E | `_bconin_vec` | 8 wskaźników wejścia | `bios/chardev.c:68-69` | BIOS 2 |
| 0x55E | `_bcostat_vec` | 8 wskaźników statusu wyjścia | `bios/chardev.c:70-71` | BIOS 8 |
| 0x57E | `_bconout_vec` | 8 wskaźników wyjścia | `bios/chardev.c:72-73` | BIOS 3 |
| **0x59E** | **`_longframe`** | ≠0 → CPU ≥ 68010 (długie ramki) | `bios/processor.S:262` | `bdos/rwa.S:369,388`, `bios/vectors.S`, `bios/amiga2.S:210` |
| **0x5A0** | **`_p_cookies`** | wskaźnik na słoik cookies | `util/cookie.c:31` = `dflt_jar` | `bdos/umem.c:575`, `bios/machine.c`, `cli/cmdutil.c:317`; programy (Getcookie) |
| 0x5A4 | `_ramtop` | szczyt TT-RAM lub NULL | `bios/memory2.c:119,121,128` | `bdos/umem.c` (alt-RAM) |
| 0x5A8 | `_ramvalid` | `0x1357BD13` = ramtop ważny | `bios/memory2.c:124,129` | `bios/memory2.c:94` |
| 0x5AC | `_bell_hook` | procedura dzwonka | `bios/sound.c:90` = `do_bell` | `bios/sound.c:253` |
| 0x5B0 | `_kcl_hook` | procedura kliku klawisza | `bios/sound.c:91` = `do_keyclick` | `bios/vectors.S:1434` |

### 1.3 Zmienne, o które pyta zadanie, a których w EmuTOS **nie ma**

To nie jest brak w analizie — te symbole nie występują w drzewie:

| Symbol | Status w EmuTOS | Odpowiednik |
|---|---|---|
| `_mch` (machine id, 0x5A0 w części dokumentacji) | **brak** — adres 0x5A0 zajmuje `_p_cookies` (`tosvars.ld:106`) | cookie `_MCH`: `bios/machine.c:694` `cookie_add(COOKIE_MCH, cookie_mch);`, wartości `MCH_ST`/`MCH_STE` w `include/cookie.h:51-52`, wyznaczane w `bios/machine.c:382-404` |
| `_sndbits` | **brak** — `grep sndbits` nie zwraca nic | XBIOS `Soundcmd`/`Setcolor`; PSG inicjowany w `bios/sound.c:69` (`snd_init`), `bell_hook`/`kcl_hook` jw. |
| `_pdb` (PDB TOS 4) | **brak** | aktualny proces to `run` (`bdos/proc.c:52`), dostępny przez `os_header.os_run` (`bios/startup.S:102`) |
| aliasy `_hz_200` | **brak** — istnieje tylko jeden adres 0x4BA (`tosvars.ld:81`) | — |
| `vmousevec` | **brak** — patrz sekcja 3: struktura nazywa się `mousevec` | — |

### 1.4 Rzeczy, na które warto uważać

1. **Komentarz w `bios/biosmem.c:49` jest nieaktualny**: mówi „`phystop` TOS variable in 0x43a",
   podczas gdy `tosvars.ld:42` definiuje `_phystop = 0x42e`, a 0x43A to `_memval2`
   (`tosvars.ld:45`). Autorytatywny jest `tosvars.ld`.
2. `bios/lowstram.c` umieszcza w **pierwszych 32 KiB** ST-RAM obiekty, które muszą być widoczne
   z trybu użytkownika: `phys_work` (`bios/lowstram.c:26`), `dskbuf` (`:40`) i `shifty` (`:53`):

```
bios/lowstram.c:42: /* Workaround for bug in GFA Basic 3.51 startup.
bios/lowstram.c:43:  * Theoretically, we are allowed to put the shifty variable anywhere,
...
bios/lowstram.c:52: UBYTE dskbuf_alignment[3]; /* FIXME: Unsafe hack to align dskbuf on 4 bytes */
bios/lowstram.c:53: UBYTE shifty; /* reflects the status up/down of mode keys */
```

   a `emutos.ld:105-113` gwarantuje `_shifty < 0x8000` oraz `dskbuf` wyrównany do 4.
3. `shifty` jest eksponowane programom przez `os_header.os_kbshift` (`bios/startup.S:100`,
   `include/biosdefs.h:59`), a nie przez stały adres.

---

## 2. Tablice wektorów

### 2.1 Wektory wyjątków (0x008–0x0FC)

Instalacja domyślna — `bios/bios.c:143-144`:

```
bios/bios.c:143:     init_exc_vec();
bios/bios.c:144:     init_user_vec(FIRST_BOOT);
```

`init_exc_vec` wypełnia **61 wektorów** (nr 2–62) adresami z `deflt_vec_table`, każdy prowadzi do
`any_vec`, który zrzuca stan do 0x380 i woła `dopanic()`:

```
bios/vectors.S:102: /* basically initialize the 62 exception vectors. */
bios/vectors.S:103: _init_exc_vec:
bios/vectors.S:104:         lea     deflt_vec_table, a0
bios/vectors.S:105:         lea     8, a1
bios/vectors.S:106:         move.l  #61, d0
bios/vectors.S:107: set_vec:
bios/vectors.S:108:         move.l  a0, (a1)+
bios/vectors.S:109:         add.l   #2, a0
```

`init_user_vec` wypełnia wektory użytkownika (od 0x100) wspólnym `user_vec`:

```
bios/vectors.S:125: _init_user_vec:
bios/vectors.S:126:         lea     user_vec, a0
bios/vectors.S:127:         lea     256, a1
bios/vectors.S:128:         moveq   #63, d0
bios/vectors.S:129:         tst.w   4(sp)           // check FIRST_BOOT
bios/vectors.S:130:         jeq     set_uvec
bios/vectors.S:131:         move.w  #191, d0
```

Następnie `vecs_init` ustawia wektory, które muszą być poprawne przed startem czegokolwiek
(`bios/bios.c:151-176`):

```
bios/bios.c:152:     VEC_LEVEL1 = just_rte;
bios/bios.c:158:     VEC_LEVEL7 = just_rte;
bios/bios.c:169:     VEC_DIVNULL = just_rte;
bios/bios.c:173:     VEC_GEM = vditrap;
bios/bios.c:174:     VEC_BIOS = biostrap;
bios/bios.c:175:     VEC_XBIOS = xbiostrap;
bios/bios.c:176:     VEC_LINEA = int_linea;
```

Komentarz wyjaśnia, dlaczego VBL/HBL nie są tu ustawiane (robi to `machine_init`):

```
bios/bios.c:146:     /* Some user drivers may install interrupt handlers and call the previous
bios/bios.c:147:      * ones. For example, ARAnyM's network driver for MiNT (nfeth.xif) and fVDI
bios/bios.c:148:      * driver (aranym.sys) install custom handlers on INT3, and call the
bios/bios.c:149:      * previous one. This panics with "Exception number 27" if VEC_LEVEL3 is
bios/bios.c:150:      * not initialized with a valid default handler.
```

Emulacja nietypowych instrukcji (potrzebna wielu programom TOS):

```
bios/bios.c:182:     if (longframe) {
bios/bios.c:187:         VEC_PRIVLGE = int_priv;
bios/bios.c:192:         VEC_ILLEGAL = int_illegal;
bios/bios.c:202:     VEC_UNIMPINT = int_unimpint;
```

Adresy wektorów są zdefiniowane w `bios/vectors.h:53-68`:

```
bios/vectors.h:53: #define VEC_ILLEGAL (*(volatile PFVOID*)0x10) /* illegal instruction vector */
bios/vectors.h:54: #define VEC_DIVNULL (*(volatile PFVOID*)0x14) /* division by zero exception vector */
bios/vectors.h:55: #define VEC_PRIVLGE (*(volatile PFVOID*)0x20) /* privilege exception vector */
bios/vectors.h:56: #define VEC_LINEA   (*(volatile PFVOID*)0x28) /* LineA exception vector */
bios/vectors.h:64: #define VEC_TRAP1   (*(volatile PFVOID*)0x84) /* TRAP #1 exception vector */
bios/vectors.h:65: #define VEC_TRAP2   (*(volatile PFVOID*)0x88) /* TRAP #2 exception vector */
bios/vectors.h:66: #define VEC_TRAP13  (*(volatile PFVOID*)0xb4) /* TRAP #13 exception vector */
bios/vectors.h:67: #define VEC_TRAP14  (*(volatile PFVOID*)0xb8) /* TRAP #14 exception vector */
bios/vectors.h:92: #define VEC_GEM     VEC_TRAP2                 /* GEM trap exception vector */
bios/vectors.h:93: #define VEC_BIOS    VEC_TRAP13                /* BIOS trap exception vector */
bios/vectors.h:94: #define VEC_XBIOS   VEC_TRAP14                /* XBIOS trap exception vector */
```

### 2.2 TRAP #1 (GEMDOS) — 0x84

Instalowany przez GEMDOS w `osinit_before_xmaddalt`:

```
bdos/bdosmain.c:317: void osinit_before_xmaddalt(void)
bdos/bdosmain.c:318: {
bdos/bdosmain.c:319:     /* take over the handling of TRAP #1 */
bdos/bdosmain.c:320:     Setexc(0x21, (long)enter);
```

Punkt wejścia `_enter` (`bdos/rwa.S:159`) robi `fix_trap` (normalizacja ramki dla 68010+),
zapisuje D0/A3–A6 do basepage’a bieżącego procesu (`P_REGSAVE`, `bdos/rwa.S:189`), przenosi
resztę rejestrów na stos użytkownika i przełącza się na prywatny stos GEMDOS:

```
bdos/rwa.S:207: // now switch to fs stack
bdos/rwa.S:208:         move.l  #fstrt,sp
bdos/rwa.S:209:         lea     50(a5),a0
bdos/rwa.S:210:         jra     callos
...
bdos/rwa.S:226: callos: move.l  a0,-(sp)        // push parm pointer
bdos/rwa.S:227:         jsr     _osif
```

`_osif` (`bdos/bdosmain.c:431`) wykonuje funkcję z tablicy `funcs` (`bdos/bdosmain.c:280-291`).

### 2.3 TRAP #2 (GEM / VDI / AES) — 0x88, łańcuch trzech instalacji

Kolejność jest kluczowa:

1. **BIOS** ustawia `VEC_GEM = vditrap` (`bios/bios.c:173`). `_vditrap` (`vdi/vdi_asm.S:34`)
   obsługuje tylko `d0 = 0x73` (VDI) i `d0 = 0xffff` (zapytanie o adres dyspozytora):

```
vdi/vdi_asm.S:34: _vditrap:
vdi/vdi_asm.S:35:         cmp.w   #0x73,d0                // VDI call?
vdi/vdi_asm.S:36:         jne     no_vdi
vdi/vdi_asm.S:37:         jsr     _GSX_ENTRY
vdi/vdi_asm.S:38:         rte
```

2. **GEMDOS** przejmuje TRAP #2, **zachowując stary wektor** (czyli `vditrap`) i łańcuchując:

```
bdos/bdosmain.c:322:     /*
bdos/bdosmain.c:323:      * intercept TRAP #2 only for xterm(), keeping the old value
bdos/bdosmain.c:324:      * so that our trap handler can call the old one
bdos/bdosmain.c:325:      */
bdos/bdosmain.c:326:     old_trap2 = (PFVOID) Setexc(0x22, (long)bdos_trap2);
```

```
bdos/rwa.S:145: _bdos_trap2:
bdos/rwa.S:146:         tst.w   d0
bdos/rwa.S:147:         jeq     oterm
bdos/rwa.S:148:
bdos/rwa.S:149:         move.l  _old_trap2,-(sp)
bdos/rwa.S:150:         rts
```

Uwaga: `d0.w = 0` na TRAP #2 jest równoważne `Pterm0()` — `bdos/rwa.S:152`.

3. **AES** przejmuje TRAP #2 na czas swojego działania, zapamiętując poprzedni wektor
   (czyli `bdos_trap2`) w `savetrap2`, i przywraca go po zakończeniu:

```
aes/geminit.c:879:     disable_interrupts();
aes/geminit.c:880:     set_aestrap();                  /* set trap#2 -> aestrap */
...
aes/geminit.c:954:     /* restore previous trap#2 address */
aes/geminit.c:955:     disable_interrupts();
aes/geminit.c:956:     unset_aestrap();
```

```
aes/gemdosif.S:295: _set_aestrap:
aes/gemdosif.S:301:         move.l  0x88,savetrap2
aes/gemdosif.S:302:         move.l  #_aestrap,0x88
aes/gemdosif.S:287: _unset_aestrap:
aes/gemdosif.S:292:         move.l  savetrap2,0x88
```

`_aestrap` (`aes/gemdosif.S:222`) rozpoznaje tylko `d0 = 0xC8`/`0xC9`, resztę przekazuje dalej:

```
aes/gemdosif.S:222: _aestrap:
aes/gemdosif.S:223:         cmpi.w  #0xC8,d0
aes/gemdosif.S:224:         jeq     trapaes
aes/gemdosif.S:225:         cmpi.w  #0xC9,d0
aes/gemdosif.S:226:         jeq     trapaes
aes/gemdosif.S:227:         move.l  savetrap2,-(sp)
aes/gemdosif.S:228:         rts
```

**Pełny łańcuch w kontekście B:**
`TRAP#2 → _aestrap → (d0≠C8/C9) → savetrap2 = bdos_trap2 → (d0≠0) → old_trap2 = vditrap →
(d0=0x73) → GSX_ENTRY / (d0=0xffff) → adres dyspozytora`.
Dodatkowo AES instaluje własny handler błędu krytycznego (`aes/geminit.c:727` `takeerr()`),
a VDI woła `_GSX_ENTRY` bezpośrednio z `vdi/vdi_asm.S:37`.

### 2.4 TRAP #13 (BIOS) i TRAP #14 (XBIOS) — 0xB4 / 0xB8

```
bios/vectors.S:1266: // ==== Trap 13 - BIOS entry point ==========================================
bios/vectors.S:1268: _biostrap:
bios/vectors.S:1269:         move.w  _bios_ent,d1
bios/vectors.S:1270:         lea     _bios_vecs,a0
bios/vectors.S:1271:         jra     biosxbios
bios/vectors.S:1275: _xbiostrap:
bios/vectors.S:1276:         move.w  _xbios_ent,d1
bios/vectors.S:1277:         lea     _xbios_vecs,a0
```

Tablice funkcji: `bios/bios.c:1279-1294` (12 funkcji: `getmpb`, `bconstat`, `bconin`, `bconout`,
`lrwabs`, `setexc`, `tickcal`, `getbpb`, `bcostat`, `mediach`, `drvmap`, `kbshift`)
oraz `bios/xbios.c:1285-1517`.

**Zapis/rekonstrukcja rejestrów przez BIOS jest zależna od `_savptr`** — to jest jedyny
„bufor zapisu" używany przez te trampoliny i musi wskazywać na `trap_save_area`, bo inaczej
każde wywołanie BIOS/XBIOS zniszczy stos:

```
bios/vectors.S:1282: biosxbios:
bios/vectors.S:1283:         move.l  _savptr.w, a1
...
bios/vectors.S:1317:         move.l  a1, _savptr.w
```

```
bios/vectors.S:1448: /* save area for trap13/14. */
bios/vectors.S:1449: save_beg:       .ds.w   24*3    // d3-d7/a3-a7, sr, pc, vo = 24 words,
bios/vectors.S:1450:                                 // multiplied by 3 ("3 level deep reentrant")
bios/vectors.S:1451: _trap_save_area:                     // End of save storage
```

### 2.5 TRAP #2 jako `Setexc(0x22,...)` — kontrakt `Setexc`

`Setexc` operuje wprost na wektorach i przyjmuje `-1` jako „tylko odczyt":

```
bios/bios.c:1097: LONG setexc(WORD num, LONG vector)
bios/bios.c:1098: {
bios/bios.c:1099:     LONG oldvector;
bios/bios.c:1100:     LONG *addr = (LONG *) (4L * num);
bios/bios.c:1101:     oldvector = *addr;
bios/bios.c:1102:
bios/bios.c:1103:     if(vector != -1) {
bios/bios.c:1104:         *addr = vector;
bios/bios.c:1105:     }
bios/bios.c:1106:     return oldvector;
bios/bios.c:1107: }
```

### 2.6 „Zapis/odtworzenie wektorów per proces" — jak to naprawdę działa

**W EmuTOS nie ma zapisu/odtworzenia tablicy wektorów na proces** (tak jak w MiNT). Basepage nie
zawiera kopii wektorów. Mechanizmy, które istnieją:

1. **Reentrancja TRAP #13/#14** przez `_savptr` + `trap_save_area` — 3 poziomy zagłębienia
   (`bios/vectors.S:1449-1451`, `bios/vectors.S:1283,1317,1335,1356`), plus `bios/kprint.c:587`
   przywraca `savptr`, „in case running program has altered it".
2. **Zapis stanu procesu to rejestry, nie wektory**: `P_REGSAVE` = 0x68 (D0, A3–A7) w basepage
   (`include/asmpd.h:25-29`), wypełniane przy wejściu do TRAP #1 (`bdos/rwa.S:189`), odtwarzane
   w `gouser` (`bdos/rwa.S:253`).
3. **`etv_term`** (0x408) — jedyny „wektor per proces": `xterm` pobiera go i wywołuje przed
   powrotem do rodzica:

```
bdos/proc.c:601: void xterm(UWORD rc)
bdos/proc.c:602: {
bdos/proc.c:603:     PFVOID userterm;
bdos/proc.c:604:     PD *p = run;
bdos/proc.c:605:
bdos/proc.c:606:     userterm = (PFVOID)Setexc(0x102, (long)-1L);  /* get user term handler address */
bdos/proc.c:607:     protect_v((PFLONG)userterm);    /* call it, protecting d2/a2 from modification */
```

   (uwaga: `0x102` to numer **zmiennej systemowej**, nie wektora procesora — `4*0x102 = 0x408`.)
4. **Globalny zapis/odtworzenie TRAP #2** przez AES omówiony w 2.3.
5. **Sprzątanie po programie, który nie wywołał `appl_exit()`** — robi to shell:

```
aes/gemshlib.c:647:         /* if the user did an appl_init() without an appl_exit(),
aes/gemshlib.c:648:          * do the important parts for him
aes/gemshlib.c:649:          */
aes/gemshlib.c:650:         if (rlr->p_flags&AP_OPEN)
```

---

## 3. Struktury wejścia: klawiatura i mysz

### 3.1 KBDVECS — definicja w C (cytowana dosłownie)

```
include/biosdefs.h:102: /*
include/biosdefs.h:103:  * Struct returned by Kbdvbase()
include/biosdefs.h:104:  */
include/biosdefs.h:105: struct kbdvecs
include/biosdefs.h:106: {
include/biosdefs.h:107:     PFVOID midivec;     /* MIDI Input */
include/biosdefs.h:108:     PFVOID vkbderr;     /* IKBD Error */
include/biosdefs.h:109:     PFVOID vmiderr;     /* MIDI Error */
include/biosdefs.h:110:     PFVOID statvec;     /* IKBD Status */
include/biosdefs.h:111:     PFVOID mousevec;    /* IKBD Mouse */
include/biosdefs.h:112:     PFVOID clockvec;    /* IKBD Clock */
include/biosdefs.h:113:     PFVOID joyvec;      /* IKBD Joystick */
include/biosdefs.h:114:     PFVOID midisys;     /* Main MIDI Vector */
include/biosdefs.h:115:     PFVOID ikbdsys;     /* Main IKBD Vector */
include/biosdefs.h:116: };
```

Zwracana adresem przez `Kbdvbase()` (XBIOS 0x22):

```
bios/xbios.c:712:  * xbios_22 - (kbdvbase) Returns pointer to a kbdvecs structure
bios/xbios.c:718:     return (LONG) &kbdvecs;
```

**Uwaga do treści zadania:** w `KBDVECS` **nie ma** pola `vmousevec`. Pola to kolejno
`midivec, vkbderr, vmiderr, statvec, mousevec, clockvec, joyvec, midisys, ikbdsys`.

### 3.2 KBDVECS — układ faktyczny w pamięci (z offsetami)

Struktura jest tworzona w asemblerze razem z polami **niedokumentowanymi** przed i po niej
(`bios/aciavecs.S`). Komentarz objaśniający:

```
bios/aciavecs.S:41: // In addition to the documented features, it was deemed necessary to add
bios/aciavecs.S:42: // the following undocumented features, located in memory immediately before
bios/aciavecs.S:43: // and after the documented part of the KBDVECS structure:
bios/aciavecs.S:44: //
bios/aciavecs.S:45: // struct UNDOCUMENTED {
bios/aciavecs.S:46: //   void (*kbdvec)( UBYTE data );   /* KBD Input, TOS >= 2.0 */
bios/aciavecs.S:47: //   KBDVECS kbdvecs;
bios/aciavecs.S:48: //   char ikbdstate;
bios/aciavecs.S:49: //   char kbdlength;
bios/aciavecs.S:50: // };
bios/aciavecs.S:52: //- kbdvec (undocumented feature of TOS >= 2.0) is called with the
bios/aciavecs.S:53: //  received data byte in d0, and a pointer to the ikbd iorec in a0.
```

Układ (kolejne `.ds.l 1` = 4 bajty):

```
bios/aciavecs.S:126: #if CONF_WITH_EXTENDED_MOUSE
bios/aciavecs.S:127: _mousexvec:     .ds.l   1       // mouse-routine for additional buttons
bios/aciavecs.S:128: #endif
bios/aciavecs.S:129: kbdvec:         .ds.l   1       // must be right before _kbdvecs
bios/aciavecs.S:130: _kbdvecs:
bios/aciavecs.S:131: midivec:        .ds.l   1       // MIDI input
bios/aciavecs.S:132: vkbderr:        .ds.l   1       // keyboard error
bios/aciavecs.S:133: vmiderr:        .ds.l   1       // MIDI-Error
bios/aciavecs.S:134: statvec:        .ds.l   1       // IKBD-Status
bios/aciavecs.S:135: mousevec:       .ds.l   1       // mouse-routine
bios/aciavecs.S:136: clockvec:       .ds.l   1       // time-routine
bios/aciavecs.S:137: joyvec:         .ds.l   1       // joystick-routinee
bios/aciavecs.S:138: midisys:        .ds.l   1       // MIDI-systemvector
bios/aciavecs.S:139: ikbdsys:        .ds.l   1       // IKBD-systemvector
bios/aciavecs.S:140: ikbdstate:      .ds.b   1       // action to take upon packet completion
bios/aciavecs.S:141: kbdlength:      .ds.b   1       // number of bytes remaining in current packet
bios/aciavecs.S:142:         .even
```

| Offset od `Kbdvbase()` | Pole | Typ |
|---|---|---|
| −0x04 | `kbdvec` | `void (*)(UBYTE)` |
| (−0x08) | `_mousexvec` | tylko gdy `CONF_WITH_EXTENDED_MOUSE` |
| **+0x00** | **`midivec`** | `void (*)(UBYTE)` |
| **+0x04** | **`vkbderr`** | `void (*)(UBYTE)` |
| +0x08 | `vmiderr` | `void (*)(UBYTE)` |
| +0x0C | `statvec` | `void (*)(char *buf)` |
| **+0x10** | **`mousevec`** | `void (*)(char *buf)` |
| +0x14 | `clockvec` | `void (*)(char *buf)` |
| +0x18 | `joyvec` | `void (*)(char *buf)` |
| +0x1C | `midisys` | `void (*)(void)` |
| **+0x20** | **`ikbdsys`** | `void (*)(void)` |
| +0x24 | `ikbdstate` | `char` |
| +0x25 | `kbdlength` | `char` |

Semantyka wywołań (z `bios/aciavecs.S:31-39`): `midivec`/`vkbderr`/`vmiderr` dostają bajt w `d0`;
`statvec`/`mousevec`/`clockvec`/`joyvec` — adres pakietu w `a0`; `midisys`/`ikbdsys` są wołane
z handlera przerwania ACIA (MFP, wektor 6).

### 3.3 Inicjalizacja KBDVECS

`_init_acia_vecs` (`bios/aciavecs.S:146-228`), wołane z `bios/bios.c:458`
(`init_acia_vecs()`). Wszystkie wektory dostają `just_rts`, z wyjątkiem tych skonfigurowanych:

```
bios/aciavecs.S:146: _init_acia_vecs:
bios/aciavecs.S:147:         // initialize the kbdvecs structure
bios/aciavecs.S:148:         lea     just_rts,a0             // dummy vector
bios/aciavecs.S:149:         lea     _kbdvecs,a1
bios/aciavecs.S:151:         move.l  a0,_mousexvec-_kbdvecs(a1)
...
bios/aciavecs.S:157:         move.l  #_kbdvec,kbdvec-_kbdvecs(a1)
bios/aciavecs.S:163:         move.l  #_midivec,midivec-_kbdvecs(a1)
bios/aciavecs.S:165:         move.l  a0,vkbderr-_kbdvecs(a1)
bios/aciavecs.S:166:         move.l  a0,vmiderr-_kbdvecs(a1)
bios/aciavecs.S:167:         move.l  a0,statvec-_kbdvecs(a1)
bios/aciavecs.S:168:         move.l  a0,mousevec-_kbdvecs(a1)
bios/aciavecs.S:174:         move.l  #_clockvec,clockvec-_kbdvecs(a1)     // in bios/clock.c
bios/aciavecs.S:179:         move.l  a0,joyvec-_kbdvecs(a1)
bios/aciavecs.S:185:         move.l  #_midisys,midisys-_kbdvecs(a1)
bios/aciavecs.S:193:         move.l  #_ikbdsys,ikbdsys-_kbdvecs(a1)
bios/aciavecs.S:204:         // initialize IKBD-related variables
bios/aciavecs.S:205:         moveq.l #0,d0
bios/aciavecs.S:206:         move.b  d0,ikbdstate
```

`mousevec` zmienia potem VDI, instalując własny handler (i przywracając przy zamknięciu):

```
vdi/vdi_mouse.c:605:     Initmous(1, (LONG)&mouse_params, (LONG)mouse_int);
```

```
bios/mouse.c:116:         if (newvec != NULL)
bios/mouse.c:117:             kbdvecs.mousevec = newvec;  /* set mouse vector */
bios/mouse.c:119:     } else {                    /* if error */
bios/mouse.c:120:         kbdvecs.mousevec = just_rts;    /* set dummy vector */
```

`statvec` jest przechwytywany przy rozszerzonej myszy (`vdi/vdi_mouse.c:609-611`).

### 3.4 IOREC — definicja w C (dosłownie) i układ

```
bios/iorec.h:18: typedef struct iorec IOREC;
bios/iorec.h:19:
bios/iorec.h:20: struct iorec {
bios/iorec.h:21:     UBYTE *buf;         /* input buffer */
bios/iorec.h:22:     WORD size;          /* buffer size */
bios/iorec.h:23:     volatile WORD head; /* head index */
bios/iorec.h:24:     volatile WORD tail; /* tail index */
bios/iorec.h:25:     WORD low;           /* low water mark */
bios/iorec.h:26:     WORD high;          /* high water mark */
bios/iorec.h:27: };
bios/iorec.h:29: extern IOREC ikbdiorec, midiiorec;
```

Układ w pamięci (zgodny z powyższą strukturą; adres `_ikbdiorec` = `&ikbdibuf`):

```
bios/aciavecs.S:107: _ikbdiorec:
bios/aciavecs.S:108: ikbdibuf:       .ds.l   1
bios/aciavecs.S:109: ikbdibufsz:     .ds.w   1
bios/aciavecs.S:110: ikbdibufhd:     .ds.w   1
bios/aciavecs.S:111: ikbdibuftl:     .ds.w   1
bios/aciavecs.S:112: ikbdibuflo:     .ds.w   1
bios/aciavecs.S:113: ikbdibufhi:     .ds.w   1
bios/aciavecs.S:114: _midiiorec:
bios/aciavecs.S:115: midiibuf:       .ds.l   1
bios/aciavecs.S:116: midiibufsz:     .ds.w   1
bios/aciavecs.S:117: midiibufhd:     .ds.w   1
bios/aciavecs.S:118: midiibuftl:     .ds.w   1
bios/aciavecs.S:119: midiibuflo:     .ds.w   1
bios/aciavecs.S:120: midiibufhi:     .ds.w   1
```

| Offset | Pole IOREC | Wartość początkowa (IKBD) | Wartość początkowa (MIDI) |
|---|---|---|---|
| +0x00 | `buf` | `ikbdibufbuf` (256 B) | `midiibufbuf` (128 B) |
| +0x04 | `size` | 256 | 128 |
| +0x06 | `head` | 0 | 0 |
| +0x08 | `tail` | 0 | 0 |
| +0x0A | `low` | 64 (`size/4`) | 32 |
| +0x0C | `high` | 192 (`3*size/4`) | 96 |

Zapisywane przez kopiowanie tabeli z ROM:

```
bios/aciavecs.S:197:         // initialize the iorecs
bios/aciavecs.S:198:         pea     iorec_table_end-iorec_table
bios/aciavecs.S:199:         pea     iorec_table
bios/aciavecs.S:200:         pea     _ikbdiorec
bios/aciavecs.S:201:         jsr     _memmove
```

```
bios/aciavecs.S:235: iorec_table:
bios/aciavecs.S:236:         //          sz,       hd, tl,       lo,             hi
bios/aciavecs.S:237:         .dc.l   ikbdibufbuf
bios/aciavecs.S:238:         .dc.w   ikbd_bufsize,  0,  0, ikbd_bufsize/4, 3*ikbd_bufsize/4
bios/aciavecs.S:239:         .dc.l   midiibufbuf
bios/aciavecs.S:240:         .dc.w   midi_bufsize,  0,  0, midi_bufsize/4, 3*midi_bufsize/4
```

### 3.5 Format 4-bajtowego rekordu klawisza

Rekord jest unią w `bios/ikbd.c`:

```
bios/ikbd.c:428: static union {
bios/ikbd.c:429:     ULONG key;                  /* combined value */
bios/ikbd.c:430:     struct {
bios/ikbd.c:431:         UBYTE shifty;           /* state of 'shifty' */
bios/ikbd.c:432:         UBYTE scancode;         /* possibly-converted scancode */
bios/ikbd.c:433:         UWORD ascii;            /* derived ascii value */
bios/ikbd.c:434:     } k;
bios/ikbd.c:435: } kb_last;
```

Wstawianie do IOREC (zawsze z krokiem 4, więc rekordy są 4-bajtowo wyrównane):

```
bios/ikbd.c:157: static void push_ikbdiorec(ULONG value)
bios/ikbd.c:158: {
bios/ikbd.c:159:     short tail;
bios/ikbd.c:160:
bios/ikbd.c:161:     KDEBUG(("KBD iorec: Pushing value 0x%08lx\n", value));
bios/ikbd.c:162:
bios/ikbd.c:163:     tail = ikbdiorec.tail + 4;
bios/ikbd.c:164:     if (tail >= ikbdiorec.size) {
bios/ikbd.c:165:         tail = 0;
bios/ikbd.c:166:     }
bios/ikbd.c:167:     if (tail == ikbdiorec.head) {
bios/ikbd.c:168:         /* iorec full */
bios/ikbd.c:169:         return;
bios/ikbd.c:170:     }
bios/ikbd.c:171:     *(ULONG_ALIAS *) (ikbdiorec.buf + tail) = value;
bios/ikbd.c:172:     ikbdiorec.tail = tail;
bios/ikbd.c:173: }
```

Wypełnienie pól i wysłanie:

```
bios/ikbd.c:900:     kb_last.k.shifty = shifty;
bios/ikbd.c:901:     kb_last.k.scancode = scancode;
bios/ikbd.c:902:     kb_last.k.ascii = ascii;
bios/ikbd.c:903:     kb_last_ikbdsys = kbdvecs.ikbdsys;
bios/ikbd.c:904:
bios/ikbd.c:905:     /*
bios/ikbd.c:906:      * if we're not sending mouse packets, send a real key
bios/ikbd.c:907:      */
bios/ikbd.c:908:     if (!mouse_packet[0])
bios/ikbd.c:909:         push_ikbdiorec(kb_last.key);
```

Odczyt (BDOS/BIOS `Bconin(2)`) potwierdza 4-bajtowy krok:

```
bios/ikbd.c:269:     ikbdiorec.head += 4;
bios/ikbd.c:270:     if (ikbdiorec.head >= ikbdiorec.size) {
bios/ikbd.c:271:         ikbdiorec.head = 0;
bios/ikbd.c:272:     }
bios/ikbd.c:273:     value = *(ULONG_ALIAS *) (ikbdiorec.buf + ikbdiorec.head);
```

**Format rekordu:** bajt 0 = `shifty` (stan modyfikatorów), bajt 1 = scancode (po konwersji),
słowo 2 = kod ASCII. Potwierdza to także generator rekordów dla konsoli szeregowej:

```
bios/ikbd.c:218:     value = MAKE_ULONG(scancode, ascii);
bios/ikbd.c:219:     value |= (ULONG)mode << 24;
```

oraz `include/portab.h:139: #define MAKE_ULONG(hi,lo) (((ULONG)(UWORD)(hi) << 16) | (UWORD)(lo))`.

Bity `shifty` (zwracane przez `Kbshift()`): `include/biosdefs.h:90-100`
(`MODE_RSHIFT 0x01`, `MODE_LSHIFT 0x02`, `MODE_CTRL 0x04`, `MODE_ALT 0x08`, `MODE_CAPS 0x10`,
`MODE_HOME 0x20`, `MODE_INSERT 0x40`). `shifty` jest zmienną w niskiej pamięci
(`bios/lowstram.c:53`) i jest inicjowane na 0 w `bios/ikbd.c:1102`.

### 3.6 Pakiety IKBD i format pakietu myszy

Tabela długości i akcji (`bios/aciavecs.S:349-361`, dane `:433-436`):

```
bios/aciavecs.S:352: //  1     F6 a1 a2 a3 a4 a5 a6 a7 (miscellaneous, 7 bytes)
bios/aciavecs.S:353: //  2     F7 0b xh xl yh yl       (absolute mouse)
bios/aciavecs.S:354: //  3     F8 dx dy                (relative mouse, no button)
bios/aciavecs.S:355: //  3     F9 dx dy                (relative mouse, button 1)
bios/aciavecs.S:356: //  3     FA dx dy                (relative mouse, button 2)
bios/aciavecs.S:357: //  3     FB dx dy                (relative mouse, both buttons)
bios/aciavecs.S:358: //  4     FC yy MM dd hh mm ss    (date and time)
bios/aciavecs.S:359: //  5     FD j0 j1                (both joysticks)
bios/aciavecs.S:360: //  6     FE bj                   (joystick 0)
bios/aciavecs.S:361: //  7     FF bj                   (joystick 1)
```

```
bios/aciavecs.S:433: ikbdstate_table:
bios/aciavecs.S:434:         .dc.b   1, 2, 3, 3, 3, 3, 4, 5, 6, 7
bios/aciavecs.S:435: kbdlength_table:
bios/aciavecs.S:436:         .dc.b   7, 5, 2, 2, 2, 2, 6, 2, 1, 1
```

Przekazanie do `mousevec` — nagłówek jest częścią bufora dla pakietów F8–FB, a pomijany dla F7:

```
bios/aciavecs.S:488: kbd_abs_mouse:
bios/aciavecs.S:489:         addq.l  #1,a0
bios/aciavecs.S:490: kbd_rel_mouse:
bios/aciavecs.S:491:         move.l  mousevec,a1
bios/aciavecs.S:492:         jra     kbd_jump_vec
```

Interpretacja bajtów przez VDI (potwierdza układ `[nagłówek][dx][dy]`):

```
vdi/vdi_asm.S:239:         move.b  (a0), d0        // see if we have a mouse packet
vdi/vdi_asm.S:240:         move.b  d0, d1          // save packet
vdi/vdi_asm.S:246:         cmp.b   #0xf8, d1       // relative mouse packet header?
...
vdi/vdi_asm.S:309:         move.b  1(a0), d0       // get new relative x pos
vdi/vdi_asm.S:310:         or.b    2(a0), d0       // get new relative y pos
```

Bity przycisków:

```
bios/ikbd.c:101: #define MOUSE_REL_POS_REPORT    0xf8    /* values for mouse_packet[0] */
bios/ikbd.c:102: #define RIGHT_BUTTON_DOWN       0x01    /* these values are OR'ed in */
bios/ikbd.c:103: #define LEFT_BUTTON_DOWN        0x02
```

Emulacja myszy klawiaturą (strzałki +Insert/Help) buduje **dokładnie ten sam** 3-bajtowy pakiet:

```
bios/ikbd.c:110: SBYTE mouse_packet[3];                  /* passed to mousevec() */
...
bios/ikbd.c:388:         call_mousevec(mouse_packet);
```

`mouse_packet[0] == 0` oznacza „nie w trybie emulacji myszy" i jest tak ustawiane w `kbd_init`:

```
bios/ikbd.c:1104:     mouse_packet[0] = 0;    /* not doing mouse emulation */
```

### 3.7 Inicjalizacja klawiatury — `kbd_init` (pełny cytat końcówki)

```
bios/ikbd.c:1062: void kbd_init(void)
...
bios/ikbd.c:1087:     /* initialize the IKBD */
bios/ikbd.c:1088:     ikbd_reset();
bios/ikbd.c:1089:
bios/ikbd.c:1090:     /* initialize the key repeat stuff */
bios/ikbd.c:1091:     kb_ticks = 0;
bios/ikbd.c:1092:     kb_initial = KB_INITIAL;
bios/ikbd.c:1093:     kb_repeat = KB_REPEAT;
bios/ikbd.c:1094:
bios/ikbd.c:1095:     kb_arrowkeys = 0;  /* no arrow keys pressed initially */
bios/ikbd.c:1096:     kb_dead = -1;      /* not in a dead key sequence */
bios/ikbd.c:1097:     kb_altnum = -1;    /* not in an alt-numeric sequence */
bios/ikbd.c:1098:     kb_switched = 0;   /* not switched initially */
bios/ikbd.c:1099:
bios/ikbd.c:1100:     conterm = 7;       /* keyclick, key repeat & system bell on by default */
bios/ikbd.c:1101:
bios/ikbd.c:1102:     shifty = 0;        /* initial state of modifiers */
bios/ikbd.c:1103:
bios/ikbd.c:1104:     mouse_packet[0] = 0;    /* not doing mouse emulation */
bios/ikbd.c:1105:
bios/ikbd.c:1106:     bioskeys();
bios/ikbd.c:1107: }
```

Kolejność w `bios_init`: `kbd_init()` (`bios/bios.c:454`) → `midi_init()` (`:456`) →
`init_acia_vecs()` (`:458`). **Uwaga:** `_init_acia_vecs` jest wołane **po** `kbd_init`, więc
`kbdvecs` jest poprawne dopiero od `bios/bios.c:458`.

Mapa klawiszy: `struct keytbl` — `bios/ikbd.h:57-86`; `bioskeys()` (`bios/ikbd.c:136-140`)
wypełnia `current_keytbl` z tabeli kraju (`get_keytbl()`), a `keytbl()` (`bios/ikbd.c:122`)
pozwala ją podmienić (XBIOS 0x19).

---

## 4. Basepage i konfiguracja procesu

### 4.1 PD = basepage (definicja dosłowna)

```
include/bdosdefs.h:57: /*
include/bdosdefs.h:58:  *  PD - Process Descriptor (a.k.a. BASEPAGE)
include/bdosdefs.h:59:  */
include/bdosdefs.h:60:
include/bdosdefs.h:61: #define NUMSTD      6       /* number of standard files */
include/bdosdefs.h:62: #define NUMCURDIR   BLKDEVNUM   /* number of entries in curdir array */
include/bdosdefs.h:63: #define PDCLSIZE    0x80    /*  size of command line in bytes  */
include/bdosdefs.h:64:
include/bdosdefs.h:65: typedef struct _pd PD;
include/bdosdefs.h:66: struct _pd
include/bdosdefs.h:67: {
include/bdosdefs.h:68: /* 0x00 */
include/bdosdefs.h:69:     UBYTE   *p_lowtpa;      /* pointer to start of TPA */
include/bdosdefs.h:70:     UBYTE   *p_hitpa;       /* pointer to end of TPA+1 */
include/bdosdefs.h:71:     UBYTE   *p_tbase;       /* pointer to base of text segment */
include/bdosdefs.h:72:     LONG    p_tlen;         /* length of text segment */
include/bdosdefs.h:73: /* 0x10 */
include/bdosdefs.h:74:     UBYTE   *p_dbase;       /* pointer to base of data segment */
include/bdosdefs.h:75:     LONG    p_dlen;         /* length of data segment */
include/bdosdefs.h:76:     UBYTE   *p_bbase;       /* pointer to base of bss segment */
include/bdosdefs.h:77:     LONG    p_blen;         /* length of bss segment */
include/bdosdefs.h:78: /* 0x20 */
include/bdosdefs.h:79:     DTA     *p_xdta;
include/bdosdefs.h:80:     PD      *p_parent;      /* parent PD */
include/bdosdefs.h:81:     ULONG   p_flags;        /* see below */
include/bdosdefs.h:82:     char    *p_env;         /* pointer to environment string */
include/bdosdefs.h:83: /* 0x30 */
include/bdosdefs.h:84:     SBYTE   p_uft[NUMSTD];  /* index into sys file table for std files */
include/bdosdefs.h:85:     char    p_lddrv;
include/bdosdefs.h:86:     UBYTE   p_curdrv;
include/bdosdefs.h:87:     LONG    p_1fill[2];
include/bdosdefs.h:88: /* 0x40 */
include/bdosdefs.h:89:     UBYTE   p_curdir[NUMCURDIR];    /* index into sys dir table */
include/bdosdefs.h:90:     char    p_2fill[32-NUMCURDIR];
include/bdosdefs.h:91: /* 0x60 */
include/bdosdefs.h:92:     LONG    p_3fill[2];
include/bdosdefs.h:93:     LONG    p_dreg[1];      /* dreg[0] */
include/bdosdefs.h:94:     LONG    p_areg[5];      /* areg[3..7] */
include/bdosdefs.h:95: /* 0x80 */
include/bdosdefs.h:96:     char    p_cmdlin[PDCLSIZE];     /* command line image */
include/bdosdefs.h:97: };
```

Offsety asemblerowe (muszą być zgodne z powyższym) — `include/asmpd.h:20-29`:

```
include/asmpd.h:21: #define P_TBASE         0x08        /* address of TEXT segment */
include/asmpd.h:22: #define P_BBASE         0x18        /* address of BSS segment */
include/asmpd.h:23: #define P_BLEN          0x1C        /* length of BSS */
include/asmpd.h:24: #define P_XDTA          0x20        /* pointer to DTA area */
include/asmpd.h:25: #define P_REGSAVE       0x68        /* registers d0,a3-a7 */
include/asmpd.h:26:                                 /* redefines of above area: */
include/asmpd.h:27: #define P_D0SAVE        0x68        /* register d0 */
include/asmpd.h:28: #define P_A6SAVE        0x78        /* register a6 */
include/asmpd.h:29: #define P_A7SAVE        0x7c        /* register a7 */
```

Wskaźnik aktualnego basepage’a: `bdos/proc.c:52` `PD *run;`, eksponowany jako
`os_header.os_run` (`bios/startup.S:102`, `include/biosdefs.h:60`).

### 4.2 Inicjalizacja pól PD

```
bdos/proc.c:363: /* initialize the structure fields */
bdos/proc.c:364: static void init_pd_fields(PD *p, char *tail, long max, char *envptr)
bdos/proc.c:365: {
bdos/proc.c:366:     int i;
bdos/proc.c:367:     char *b;
bdos/proc.c:368:
bdos/proc.c:369:     /* first, zero it out */
bdos/proc.c:370:     bzero(p, sizeof(PD)) ;
bdos/proc.c:371:
bdos/proc.c:372:     /* memory values */
bdos/proc.c:373:     p->p_lowtpa = (UBYTE *)p;              /*  M01.01.06   */
bdos/proc.c:374:     p->p_hitpa  = (UBYTE *)p  +  max;      /*  M01.01.06   */
bdos/proc.c:375:     p->p_xdta = (DTA *) p->p_cmdlin;       /* default p_xdta is p_cmdlin */
bdos/proc.c:376:     p->p_env = envptr;
bdos/proc.c:377:
bdos/proc.c:378:     /* copy tail */
bdos/proc.c:379:     b = &p->p_cmdlin[0];
bdos/proc.c:380:     for (i = 0; (i < PDCLSIZE) && (*tail); i++)
bdos/proc.c:381:         *b++ = *tail++;
bdos/proc.c:382:
bdos/proc.c:383:     *b++ = 0;
bdos/proc.c:384: }
```

Ważne: `p_cmdlin` (basepage+0x80) dostaje **kopię łańcucha „tail"** bez modyfikacji, a `tail`
w konwencji TOS ma w bajcie 0 **długość**, nie pierwszy znak. Potwierdza to EmuCON, który
buduje ten łańcuch jawnie:

```
cli/cmdexec.c:  (koniec funkcji build_cmdline)
    cmdline[0] = (char) len;
```

oraz AES, który adresuje `basepage+0x80` wprost:

```
aes/gemstart.S:54:         // ad_stail is the address to the cmdline buffer in our basepage
aes/gemstart.S:55:         move.l  a5,d0
aes/gemstart.S:56:         add.l   #0x80,d0
aes/gemstart.S:57:         move.l  d0,_ad_stail
```

Pola segmentów (`p_tbase/p_tlen/...`) wypełnia loader `kpgmld` (`bdos/kpgmld.c`, patrz
`bdos/proc.c:336`), a nagłówek programu to `PGMHDR01` (`bdos/pghdr.h:14-24`):

```
bdos/pghdr.h:15: {
bdos/pghdr.h:16:         /*  magic number is already read  */
bdos/pghdr.h:17:         LONG    h01_tlen ;      /*  length of text segment              */
bdos/pghdr.h:18:         LONG    h01_dlen ;      /*  length of data segment              */
bdos/pghdr.h:19:         LONG    h01_blen ;      /*  length of bss  segment              */
bdos/pghdr.h:20:         LONG    h01_slen ;      /*  length of symbol table              */
bdos/pghdr.h:21:         LONG    h01_res1 ;      /*  reserved - always zero              */
bdos/pghdr.h:22:         ULONG   h01_flags ;     /*  flags                               */
bdos/pghdr.h:23:         WORD    h01_abs ;       /*  not zero if no relocation           */
```

### 4.3 Deskryptor plików i katalogów

```
bdos/proc.c:387: static void init_pd_files(PD *p)
bdos/proc.c:388: {
bdos/proc.c:389:     int i;
bdos/proc.c:390:
bdos/proc.c:391:     /* inherit standard files from me */
bdos/proc.c:392:     for (i = 0; i < NUMSTD; i++) {
bdos/proc.c:393:         WORD h = run->p_uft[i];
bdos/proc.c:394:         if (h > 0)
bdos/proc.c:395:             ixforce(i, h, p);
bdos/proc.c:396:         else
bdos/proc.c:397:             p->p_uft[i] = h;
bdos/proc.c:398:     }
```

Standardowe uchwyty ustawia `stdhdl_init` (`bdos/console.c:107-126`), wołane w
`osinit_after_xmaddalt` (`bdos/bdosmain.c:347`). Bez tego program nie miałby otwartej konsoli.

### 4.4 Środowisko

```
bdos/proc.c:144: /*
bdos/proc.c:145:  * envsize - determine size of env area
bdos/proc.c:146:  *
bdos/proc.c:147:  * counts bytes starting at 'env' up to and including the terminating
bdos/proc.c:148:  * double null.
bdos/proc.c:149:  */
bdos/proc.c:150: static WORD envsize(char *env)
bdos/proc.c:151: {
bdos/proc.c:152:     char *e;
bdos/proc.c:153:     WORD cnt;
bdos/proc.c:154:
bdos/proc.c:155:     for (e = env, cnt = 0; !(*e == '\0' && *(e+1) == '\0'); ++e, ++cnt)
bdos/proc.c:156:         ;
bdos/proc.c:157:
bdos/proc.c:158:     return cnt + 2;         /*  count terminating double null  */
bdos/proc.c:159: }
```

Środowisko jest **kopiowane** do świeżo zaalokowanego bloku (`alloc_env`, `bdos/proc.c:413-431`),
a jego rozmiar musi być parzysty (`bdos/proc.c:421`). `p_env` wskazuje na kopię.

Domyślne środowisko buduje BIOS jako `"PATH=<litera>:\"` + podwójny NUL:

```
bios/bios.c:77: #define ENV_SIZE    12          /* sufficient for standard PATH=^X:\^^ (^=nul byte) */
bios/bios.c:78: #define DEF_PATH    "A:\\"      /* default value for path */
bios/bios.c:80: static char default_env[ENV_SIZE];  /* default environment area */
```

```
bios/bios.c:650: /*
bios/bios.c:651:  * Build the default environment string: "PATH=^X:\^^" [where ^=nul]
bios/bios.c:652:  */
bios/bios.c:653: static void init_default_environment(void)
bios/bios.c:654: {
bios/bios.c:655:     char *p;
bios/bios.c:656:
bios/bios.c:657:     strcpy(default_env,PATH_ENV);
bios/bios.c:658:     p = default_env + sizeof(PATH_ENV); /* point to first byte of path string */
bios/bios.c:659:     strcpy(p,DEF_PATH);
bios/bios.c:660:     *p += bootdev;                      /* fix up drive letter */
bios/bios.c:661:     p += sizeof(DEF_PATH);
bios/bios.c:662:     *p = '\0';                          /* terminate with double nul */
bios/bios.c:663: }
```

z `include/bdosdefs.h:128: #define PATH_ENV "PATH="    /* PATH environment variable */`.
Kolejność: `bios/bios.c:855` (`init_default_environment()`) — **po** ustawieniu `bootdev`,
**przed** `autoexec()` (`:873`).

### 4.5 DTA (bufor transferu katalogowego)

Domyślnie DTA = `p_cmdlin` (basepage+0x80):

```
bdos/proc.c:375:     p->p_xdta = (DTA *) p->p_cmdlin;       /* default p_xdta is p_cmdlin */
```

Zmiana przez `Fsetdta`/`Setdta`:

```
bdos/fsmain.c:401:  *  xsetdta - Function 0x1A     f_setdta
bdos/fsmain.c:403: void xsetdta(DTAINFO *addr)     /* set transfer address to addr */
bdos/fsmain.c:404: {
bdos/fsmain.c:405:     run->p_xdta = (DTA *)addr;
```

Odczyt w `Fsfirst`/`Fsnext`: `bdos/fsdir.c:528, 659, 673`. Układ DTA (`include/bdosdefs.h:47-55`)
zaczyna się od 21 bajtów **wewnętrznych** EmuTOS, których program nie powinien ruszać:

```
include/bdosdefs.h:48: {
include/bdosdefs.h:49:     char    d_reserved[21];     /* internal EmuTOS usage */
include/bdosdefs.h:50:     char    d_attrib;           /* attributes */
include/bdosdefs.h:51:     UWORD   d_time;             /* packed time */
include/bdosdefs.h:52:     UWORD   d_date;             /* packed date */
include/bdosdefs.h:53:     LONG    d_length;           /* size */
include/bdosdefs.h:54:     char    d_fname[14];        /* name */
```

### 4.6 Stos przy wejściu do programu — `proc_go` i `gouser`

```
bdos/proc.c:504: /* proc_go launches the new process by creating the right data
bdos/proc.c:505:  * structure in memory, then pretending resuming from an ordinary
bdos/proc.c:506:  * BDOS call by calling gouser().
```

Struktura stosu (cytat dosłowny wraz z komentarzem — to jest odpowiedź na pytanie „co jest
w 0(sp) i 4(sp)”):

```
bdos/proc.c:528: struct gouser_stack {
bdos/proc.c:529:   LONG other_sp;   /* a4, the other stack pointer */
bdos/proc.c:530:   WORD sr;         /* d0, the status register */
bdos/proc.c:531:   LONG retaddr;    /* a3, the return address */
bdos/proc.c:532:   LONG fill[11];   /* 10 registers d1-d7/a0-a2 and one dummy so that ... */
bdos/proc.c:533:   PD * basepage;   /* ... upon startup the basepage is in 4(sp) */
bdos/proc.c:534: };
```

```
bdos/proc.c:536: static void proc_go(PD *p)
bdos/proc.c:537: {
bdos/proc.c:538:     struct gouser_stack *sp;
bdos/proc.c:539:
bdos/proc.c:540:     KDEBUG(("BDOS xexec: trying to load (and execute) a process on %p ...\n",p->p_tbase));
bdos/proc.c:541:     p->p_parent = run;
bdos/proc.c:542:
bdos/proc.c:543:     /* create a stack at the end of the TPA */
bdos/proc.c:544:     sp = (struct gouser_stack *) (p->p_hitpa - sizeof(struct gouser_stack));
bdos/proc.c:545:
bdos/proc.c:546:     sp->basepage = p;      /* the stack contains the basepage */
bdos/proc.c:547:
bdos/proc.c:548:     sp->retaddr = (long)p->p_tbase; /* return address a3 is text start */
bdos/proc.c:549:     sp->sr = get_sr() & 0x0700;  /* the process will start in user mode, same IPL */
bdos/proc.c:550:
bdos/proc.c:551:     /* the other stack is the supervisor stack */
bdos/proc.c:552:     sp->other_sp = (long) &supstk[SUPSIZ];
bdos/proc.c:553:
bdos/proc.c:554:     /* store this new stack in the saved a7 field of the PD */
bdos/proc.c:555:     p->p_areg[7-3] = (long) sp;
```

i dalej rejestry startowe:

```
bdos/proc.c:557: #if 1
bdos/proc.c:558:     /* the following settings are not documented, and hence theoretically
bdos/proc.c:559:      * the assignments below are not necessary.
bdos/proc.c:560:      * However, many programs test if A0 = 0 to check if they are running
bdos/proc.c:561:      * as a normal program or as an accessory, so we need to clear at least
bdos/proc.c:562:      * this register!
bdos/proc.c:563:      */
bdos/proc.c:564:     {   /* d1-d7/a0-a2 and dummy return address set to zero */
bdos/proc.c:565:         int i;
bdos/proc.c:566:         for(i = 0; i < 11 ; i++)
bdos/proc.c:567:             sp->fill[i] = 0;
bdos/proc.c:568:     }
bdos/proc.c:569:     p->p_areg[6-3] = (long) sp;    /* a6 to hold a copy of the stack */
bdos/proc.c:570:     p->p_areg[5-3] = (long)p->p_dbase;  /* a5 to point to the DATA segt */
bdos/proc.c:571:     p->p_areg[4-3] = (long)p->p_bbase;  /* a4 to point to the BSS segt */
```

przełączenie i skok do kodu użytkownika:

```
bdos/proc.c:574:     /* the new process is the one to run */
bdos/proc.c:575:     run = (PD *)p;
bdos/proc.c:576:
bdos/proc.c:577:     gouser();
bdos/proc.c:578: }
```

Rzeczywista maszyneria przełączenia trybu — `bdos/rwa.S:234-259`:

```
bdos/rwa.S:234: _gouser:
bdos/rwa.S:235:         move.l  _run,a5         // a5 -> current basepage
bdos/rwa.S:236:         move.l  d0,P_D0SAVE(a5)
bdos/rwa.S:237: ggusr:  move.l  P_A7SAVE(a5),a6 // stack pointer (maybe usp, maybe ssp)
bdos/rwa.S:238:         move.l  (a6)+,a4        // other stack pointer
bdos/rwa.S:239:         move.w  (a6)+,d0
bdos/rwa.S:240:         move.l  (a6)+,a3        // retadd
bdos/rwa.S:245:         movem.l (a6)+,d1-d7/a0-a2
bdos/rwa.S:247:         btst    #13,d0
bdos/rwa.S:248:         jne     retsys          // a6 is (user-supplied) system stack
bdos/rwa.S:249:         move.l  a4,sp
bdos/rwa.S:250:         move.l  a6,usp
bdos/rwa.S:251: gousr:  move.l  a3,-(sp)
bdos/rwa.S:252:         move    d0,-(sp)
bdos/rwa.S:253:         movem.l P_REGSAVE(a5),d0/a3-a6
bdos/rwa.S:255:         jra     fix_rte
```

**Wyprowadzenie wartości startowych (arytmetyka wynikająca z kodu):**

- `sizeof(struct gouser_stack)` = 4 (`other_sp`) + 2 (`sr`) + 4 (`retaddr`) + 44 (`fill[11]`)
  + 4 (`basepage`) = **58**.
- `sp = p_hitpa − 58`.
- Po `movem.l (a6)+,d1-d7/a0-a2` rejestr `a6` = `sp + 10 + 40 = sp + 50`.
- `a6 → usp`, następnie `RTE` przełącza bank A7, więc **USP w chwili wejścia do programu
  = sp + 50 = p_hitpa − 8**.
- Zatem:
  - **0(sp)** = `fill[10]` = „dummy" wyzerowany w `bdos/proc.c:566-567` → **0**,
  - **4(sp)** = `basepage` = wskaźnik `PD *` (dokładnie tak, jak mówi komentarz
    `bdos/proc.c:532-533`),
  - **8(sp)** = `p_hitpa` (pierwszy bajt po TPA).
- Rejestry przy wejściu: `d0` = 0 (lub kod powrotu dla wznawianego procesu — `bdos/rwa.S:236`,
  `bdos/proc.c:616`), `d1-d7` = 0, `a0-a2` = 0, `a3` = 0, `a4` = `p_bbase`,
  `a5` = `p_dbase`, `a6` = `sp+50` (kopia stosu), `a7`(USP) = `sp+50`,
  `PC` = `p_tbase`, `SR` = tylko bity IPL zachowane, **S = 0 (tryb użytkownika)**.
- `SSP` po przełączeniu = dawny `USP` = `sp+50`, czyli stos nadzorcy startuje na szczycie TPA
  (dlatego program w trybie użytkownika nie może bezkarnie przepełnić stosu — nad nim leży TPA).

Ramy tego stosu są przygotowane raz, w `proc_go`, i „odgrywane” przez `gouser` przy każdym
powrocie do procesu (także po `Pterm` — `bdos/proc.c:609-617`, `bdos/rwa.S:265-267`).

---

## 5. Globalne struktury AES/VDI i ich start

### 5.1 Blok parametrów AES — **uwaga: nie ma stałego adresu**

Klasyczna dokumentacja TOS mówi o bloku pod 0x5AC. **EmuTOS tego nie robi** — AES dostaje
**wskaźnik** do bloku w `d1`, a numer funkcji w `d0`:

```
aes/gemsuper.h:13: typedef struct aespb {
aes/gemsuper.h:14:     WORD *control;
aes/gemsuper.h:15:     WORD *global;
aes/gemsuper.h:16:     WORD *intin;
aes/gemsuper.h:17:     WORD *intout;
aes/gemsuper.h:18:     LONG *addrin;
aes/gemsuper.h:19:     LONG *addrout;
aes/gemsuper.h:20: } AESPB;
```

```
aes/gemdosif.S:222: _aestrap:
aes/gemdosif.S:223:         cmpi.w  #0xC8,d0
```

`0xC8` = AES, `0xC9` = AES + `dsptch()`. Blok jest kopiowany do lokalnych buforów o stałych
rozmiarach, więc program może trzymać go u siebie:

```
aes/gemsuper.c:428: static void xif(AESPB *pcrys_blk)
aes/gemsuper.c:429: {
aes/gemsuper.c:430:     WORD    control[C_SIZE];
aes/gemsuper.c:431:     WORD    int_in[I_SIZE];
aes/gemsuper.c:432:     WORD    int_out[O_SIZE];
aes/gemsuper.c:433:     LONG    addr_in[AI_SIZE];
aes/gemsuper.c:434:
aes/gemsuper.c:435:     memcpy(control, pcrys_blk->control, C_SIZE*sizeof(WORD));
```

**Wniosek:** dla AES nie trzeba nic „ustawiać pod adresem 0x5AC”. Trzeba natomiast, żeby pod
0x88 siedział `_aestrap`, a w `d1` był poprawny wskaźnik `AESPB`.

Blok parametrów **VDI** również jest przekazywany wskaźnikiem (`d1`), ale VDI utrzymuje własną
kopię wskaźników w niskiej pamięci (`bios/lineavars.S:271-276`):

```
aes/gsx2.h:16: /* VDI parameter block */
aes/gsx2.h:17: typedef struct
aes/gsx2.h:18: {
aes/gsx2.h:19:     WORD *contrl;
aes/gsx2.h:20:     WORD *intin;
aes/gsx2.h:21:     WORD *ptsin;
aes/gsx2.h:22:     WORD *intout;
aes/gsx2.h:23:     WORD *ptsout;
aes/gsx2.h:24: } VDIPB;
```

Kolejność pól jest istotna, bo `_GSX_ENTRY` czyta je „po kolei” z pominięciem `ptsin`:

```
vdi/vdi_asm.S:74:         movea.l d1,a2           // get address of user's parameter block
vdi/vdi_asm.S:75:         lea     _local_pb, a1    // get address of local storage area
vdi/vdi_asm.S:76:         movea.l (a2)+,a0        // copy address of user's CONTRL aray
vdi/vdi_asm.S:77:         move.l  a0,(a1)+        // save address of user's CONTRL array
vdi/vdi_asm.S:78:         move.l  (a2)+,(a1)+     // save address of user's INTIN  array
vdi/vdi_asm.S:79:         lea     lcl_ptsin,a3    // shared area used for local copy of PTSIN array
vdi/vdi_asm.S:80:         move.l  a3,(a1)+        // save address of local PTSIN array
vdi/vdi_asm.S:81:         movea.l (a2)+,a4        // get address of user's PTSIN array
vdi/vdi_asm.S:82:         move.l  (a2)+,(a1)+     // save address of user's INTOUT array
vdi/vdi_asm.S:83:         move.l  (a2),(a1)       // save address of user's PTSOUT array
```

### 5.2 Globalne zmienne AES (`gl_*` / `GLOBAL`)

Tablica parametrów AES-a dla aplikacji jest **dostarczana przez aplikację** (pole `global`
w `AESPB`), a AES wypełnia je w `APPL_INIT`:

```
aes/gemsuper.c:89:     /* Application Manager */
aes/gemsuper.c:90:     case APPL_INIT:
aes/gemsuper.c:91:         aestrace("appl_init()");
aes/gemsuper.c:92:         pglobal->ap_version = AES_VERSION;  /* version number     */
aes/gemsuper.c:93:         pglobal->ap_count = 1;              /* # concurrent procs */
aes/gemsuper.c:94:         pglobal->ap_id = rlr->p_pid;
aes/gemsuper.c:95:         tree = rs_trees[DESKTOP];
aes/gemsuper.c:96:         pglobal->ap_private = tree[ROOT].ob_spec;
aes/gemsuper.c:97:         pglobal->ap_planes = gl_nplanes;
aes/gemsuper.c:98:         pglobal->ap_3resv = (LONG)&D;
aes/gemsuper.c:99:         ret = ap_init();
aes/gemsuper.c:100:         break;
```

Układ tej struktury (`aes/gemrslib.h:15-27`) — to jest „global array", o którą pyta zadanie:

```
aes/gemrslib.h:15: typedef struct aesglobal {
aes/gemrslib.h:16:     WORD ap_version;                /* AES version */
aes/gemrslib.h:17:     WORD ap_count;                  /* max # of concurrent applications */
aes/gemrslib.h:18:     WORD ap_id;                     /* application id */
aes/gemrslib.h:19:     LONG ap_private;                /* for use by application */
aes/gemrslib.h:20:     OBJECT **ap_ptree;              /* pointer to array of tree addresses */
aes/gemrslib.h:21:                                 /* the following are not advertised to applications */
aes/gemrslib.h:22:     RSHDR *ap_rscmem;               /* address of rsc file in memory */
aes/gemrslib.h:23:     UWORD ap_rsclen;                /* length of rsc file */
aes/gemrslib.h:24:     WORD ap_planes;                 /* # of colour planes on screen */
aes/gemrslib.h:25:     LONG ap_3resv;                  /* ptr to AES global area D (struct THEGLO) */
aes/gemrslib.h:26:     LONG ap_4resv;                  /* reserved for use in AES 4.00 */
aes/gemrslib.h:27: } AESGLOBAL;
```

`ap_init()` tylko oznacza proces jako „otwarty":

```
aes/gemaplib.c:51: /*
aes/gemaplib.c:52:  *  Routine to initialize the application
aes/gemaplib.c:53:  */
aes/gemaplib.c:54: WORD ap_init(void)
aes/gemaplib.c:55: {
aes/gemaplib.c:56:     WORD    pid;
aes/gemaplib.c:57:
aes/gemaplib.c:58:     pid = rlr->p_pid;
aes/gemaplib.c:59:
aes/gemaplib.c:60:     rlr->p_flags |= AP_OPEN;        /* appl_init() done */
aes/gemaplib.c:61:
aes/gemaplib.c:62:     return pid;
aes/gemaplib.c:63: }
```

**`appl_init` nie może się nie udać** — nie ma limitu „kontekstów AES" ani sprawdzania
zajętości: zwraca po prostu `rlr->p_pid`. Ryzyko polega wyłącznie na tym, czy `rlr` (bieżący
proces AES) i `D` (obszar THEGLO) są już zainicjalizowane.

Zbiór zmiennych AES używanych przez aplikację (bufor `gl_ws`, `contrl`, `intin`, `ptsin`,
`intout`, `ptsout`, `gl_handle`, `gl_nplanes`) — patrz `aes/gemgraf.c:37-66`.

### 5.3 Start AES — `gem_main` i `run_accs_and_desktop`

Kolejność (skrót z `aes/geminit.c:783-961`):

```
aes/geminit.c:862:     ml_ocnt = 0;
aes/geminit.c:866:     mn_init();                      /* initialise variables for menu_register() */
aes/geminit.c:868:     num_accs = count_accs();        /* puts ACC names in acc[].name */
...
aes/geminit.c:877:     totpds = num_accs + 2;
aes/geminit.c:879:     disable_interrupts();
aes/geminit.c:880:     set_aestrap();                  /* set trap#2 -> aestrap */
...
aes/geminit.c:888:     eul = NULL;
aes/geminit.c:894:     /* initialize sync blocks */
aes/geminit.c:904:     nrl = drl = NULL;
aes/geminit.c:906:     fph = fpt = fpcnt = 0;
...
aes/geminit.c:929:         rlr->p_pid = i;
aes/geminit.c:933:     curpid = 0;
aes/geminit.c:934:     rlr->p_pid = curpid++;
aes/geminit.c:935:     rlr->p_link = NULL;
aes/geminit.c:939:     /* restart the tick     */
aes/geminit.c:940:     enable_interrupts();
aes/geminit.c:946:     gl_mowner = ctl_pd = iprocess("SCRENMGR", ctlmgr);
aes/geminit.c:952:     aes_run_rom_program(accdesk_start);
```

Obszar AES (`THEGLO`) jest zerowany i mieści się w segmencie AES:

```
aes/gemstart.S:99:         // clear the 'global memory' zone whose size is determined by
aes/gemstart.S:100:         // routine size_theglo :-)
aes/gemstart.S:101:         jsr     _size_theglo
aes/gemstart.S:102:         move.l  d0,-(sp)        // size in bytes
aes/gemstart.S:103:         movea.l #_D,a0
aes/gemstart.S:104:         move.l  a0,-(sp)        // address
aes/gemstart.S:105:         jsr     _bzero_nobuiltin    // clear it
```

```
aes/geminit.c:156: LONG size_theglo(void)
aes/geminit.c:157: {
aes/geminit.c:158:     return sizeof(THEGLO);
aes/geminit.c:159: }
```

Stos nadzorcy procesu 0 (pid 0 = AES) jest ustawiany na końcu `u_supstk`:

```
aes/geminit.c:168: LONG init_p0_stkptr(void)
aes/geminit.c:169: {
aes/geminit.c:170:     UDA *u = &D.g_int[0].a_uda;
aes/geminit.c:171:
aes/geminit.c:172:     u->u_spsuper = &u->u_supstk + 1;
aes/geminit.c:173:
aes/geminit.c:174:     return (char *)u->u_spsuper - (char *)u;
aes/geminit.c:175: }
```

Zamiana kontekstu przy wejściu do AES zależy od `rlr` i `p_uda->u_spsuper`:

```
aes/gemdosif.S:249:         movea.l _rlr,a6
aes/gemdosif.S:250:         movea.l PD_UDA(a6),a6
aes/gemdosif.S:251:         move.l  a0,UDA_SPUSER(a6)
aes/gemdosif.S:252:         move.l  sp,UDA_OLDSPSUPER(a6)
aes/gemdosif.S:253:         movea.l UDA_SPSUPER(a6),sp
```

Stąd pierwszy warunek poprawności `appl_init`: **`rlr` musi być nie-NULL i mieć poprawne
`p_uda`** (ustawiane w `aes/geminit.c:911-921` i przez `pstart`/`pd_index`).

### 5.4 VDI — stan wymagany przed pierwszym wywołaniem

Globalne wskaźniki buforów VDI (niskie adresy, w sekcji `.low_stram`):

```
bios/lineavars.S:264: // ==== Global GSX Variables =================================================
bios/lineavars.S:266: line_a_vars:                            // This is the base line-a pointer
bios/lineavars.S:267:
bios/lineavars.S:268: _v_planes:      .ds.w   1               // +0   number of video planes.
bios/lineavars.S:269: _v_lin_wr:      .ds.w   1               // +2   number of bytes/video line.
bios/lineavars.S:270:
bios/lineavars.S:271: _local_pb:
bios/lineavars.S:272: _CONTRL:        .ds.l   1               // +4   ptr to the CONTRL array.
bios/lineavars.S:273: _INTIN:         .ds.l   1               // +8   ptr to the INTIN array.
bios/lineavars.S:274: _PTSIN:         .ds.l   1               // +12  ptr to the PTSIN array.
bios/lineavars.S:275: _INTOUT:        .ds.l   1               // +16  ptr to the INTOUT array.
bios/lineavars.S:276: _PTSOUT:        .ds.l   1               // +20  ptr to the PTSOUT array.
```

Baza zmiennych Line-A dla programów (`include/lineavars.h:47`):

```
include/lineavars.h:47: #define line_a_vars (void *)&v_planes   /* start of linea variables */
```

Dyspozytor VDI wymaga tablicy `vwk_ptr` oraz ważnego `CONTRL`:

```
vdi/vdi_main.c:121: void screen(void)
vdi/vdi_main.c:122: {
vdi/vdi_main.c:123:     WORD opcode, handle;
vdi/vdi_main.c:124:     WORD *contrl = CONTRL;
vdi/vdi_main.c:125:     const struct vdi_jmptab *jmptab;
vdi/vdi_main.c:126:     Vwk *vwk = NULL;
vdi/vdi_main.c:127:
vdi/vdi_main.c:128:     /* get workstation handle */
vdi/vdi_main.c:129:     handle = CONTRL[6];
```

Tabela uchwytów i granice (64 lub 128 stanowisk, zależnie od konfiguracji):

```
vdi/vdi_control.c:25: #define FIRST_VDI_HANDLE    1
vdi/vdi_control.c:26: #define LAST_VDI_HANDLE     (FIRST_VDI_HANDLE+NUM_VDI_HANDLES-1)
vdi/vdi_control.c:27: #define VDI_PHYS_HANDLE     FIRST_VDI_HANDLE
```

```
vdi/vdi_control.c:44: /*
vdi/vdi_control.c:45:  * entry n in the following array points to the Vwk corresponding to
vdi/vdi_control.c:46:  * VDI handle n.  entry 0 is unused.
vdi/vdi_control.c:47:  */
vdi/vdi_control.c:48: static Vwk *vwk_ptr[NUM_VDI_HANDLES+1];
vdi/vdi_control.c:49: extern Vwk phys_work;       /* attribute area for physical workstation */
```

```
vdi/vdi_control.c:178: Vwk * get_vwk_by_handle(WORD handle)
vdi/vdi_control.c:179: {
vdi/vdi_control.c:180:     if ((handle < FIRST_VDI_HANDLE) || (handle > LAST_VDI_HANDLE))
vdi/vdi_control.c:181:         return NULL;
vdi/vdi_control.c:182:
vdi/vdi_control.c:183:     return vwk_ptr[handle];
vdi/vdi_control.c:184: }
```

Fizyczna stacja robocza jest tworzona przez `v_opnwk` — kopiuje `SIZ_TAB`, `DEV_TAB`, `INQ_TAB`,
inicjalizuje kolory, czcionki, mysz i wejście w tryb graficzny:

```
vdi/vdi_control.c:528:     /* We need to copy some initial table data from the ROM */
vdi/vdi_control.c:529:     for (i = 0; i < 12; i++) {
vdi/vdi_control.c:530:         SIZ_TAB[i] = SIZ_TAB_rom[i];
vdi/vdi_control.c:531:     }
vdi/vdi_control.c:532:
vdi/vdi_control.c:533:     for (i = 0; i < 45; i++) {
vdi/vdi_control.c:534:         DEV_TAB[i] = DEV_TAB_rom[i];
vdi/vdi_control.c:535:         INQ_TAB[i] = INQ_TAB_rom[i];
vdi/vdi_control.c:536:     }
vdi/vdi_control.c:537:
vdi/vdi_control.c:538:     /* update resolution-dependent values */
vdi/vdi_control.c:539:     update_rez_dependent();
vdi/vdi_control.c:540:
vdi/vdi_control.c:541:     /* initialize the vwk pointer array */
vdi/vdi_control.c:542:     vwk = &phys_work;
vdi/vdi_control.c:543:     vwk_ptr[VDI_PHYS_HANDLE] = vwk;
vdi/vdi_control.c:544:     CONTRL[6] = vwk->handle = VDI_PHYS_HANDLE;
vdi/vdi_control.c:545:     for (i = VDI_PHYS_HANDLE+1, p = vwk_ptr+i; i <= LAST_VDI_HANDLE; i++)
vdi/vdi_control.c:546:         *p++ = NULL;
...
vdi/vdi_control.c:550:     init_colors();              /* Initialize palette etc. */
vdi/vdi_control.c:551:
vdi/vdi_control.c:552:     text_init();                /* initialize the SIZ_TAB info */
...
vdi/vdi_control.c:562:     timer_init();
vdi/vdi_control.c:563:     vdimouse_init();            /* initialize mouse */
vdi/vdi_control.c:564:     esc_init(vwk);              /* enter graphics mode */
vdi/vdi_control.c:565:
vdi/vdi_control.c:566:     /* Just like TOS 2.06, make the physical workstation the current workstation for Line-A. */
vdi/vdi_control.c:567:     CUR_WORK = vwk;
```

`v_opnvwk` (dla programów) **zakłada**, że `v_opnwk` już był — m.in. kopiuje do `INTOUT`
gotowe `DEV_TAB`, które `v_opnvwk` sam wypełnia tylko wtedy, gdy zrobił to wcześniej `v_opnwk`:

```
vdi/vdi_control.c:344:     pointer = INTOUT;
vdi/vdi_control.c:345:     src_ptr = DEV_TAB;
vdi/vdi_control.c:346:     for (l = 0; l < 45; l++)
vdi/vdi_control.c:347:         *pointer++ = *src_ptr++;
```

Dlatego program, który wywoła `v_opnvwk` bez wcześniejszego otwarcia stacji fizycznej przez
AES, dostanie `work_out[]` zbudowane z **wyzerowanego** `DEV_TAB` (same zera).
`v_opnvwk` poprawnie obsługuje natomiast brak wolnego uchwytu i brak pamięci:

```
vdi/vdi_control.c:425:     if (handle > LAST_VDI_HANDLE) { /* No handle available, exit */
vdi/vdi_control.c:426:         CONTRL[6] = 0;
vdi/vdi_control.c:427:         return;
vdi/vdi_control.c:428:     }
...
vdi/vdi_control.c:443:     vwk = (Vwk *)Mxalloc(size, MX_SUPER);
vdi/vdi_control.c:444:     if (vwk == NULL) {
vdi/vdi_control.c:445:         CONTRL[6] = 0;  /* No memory available, exit */
vdi/vdi_control.c:446:         return;
vdi/vdi_control.c:447:     }
```

Struktura `Vwk` (`vdi/vdi_defs.h:171-221`) — pełna definicja jest w pliku; kluczowa uwaga
o kompatybilności z Line-A (offset `fill_color` = 0x1E) jest w `vdi/vdi_defs.h:163-169`.

### 5.5 Czcionki

`font_init()` (wołane z `bios/bios.c:282`) przygotowuje czcionki systemowe i łańcuch nagłówków:

```
bios/font.c:31: /*
bios/font.c:32:  * font_init - set default font to linea, font ring initialization
bios/font.c:33:  */
bios/font.c:34:
bios/font.c:35: void font_init(void)
bios/font.c:36: {
bios/font.c:37:     const Fonthead *f6x6, *f8x8, *f8x16;
bios/font.c:38:
bios/font.c:39:     /* ask country.c for the right fonts */
bios/font.c:40:
bios/font.c:41:     get_fonts(&f6x6, &f8x8, &f8x16);
...
bios/font.c:54:     fon8x8.next_font = &fon8x16;
bios/font.c:55:     font_count = 1;     /* number of different font ids in font_ring[] */
bios/font.c:56:
bios/font.c:57:     /* Initialize the system font array for linea */
bios/font.c:58:
bios/font.c:59:     sysfonts[0] = f6x6;
bios/font.c:60:     sysfonts[1] = f8x8;
bios/font.c:61:     sysfonts[2] = f8x16;
bios/font.c:62:     sysfonts[3] = NULL;
bios/font.c:63: }
```

Uwaga: `font_init` jest wołane **przed** `linea_init` (`bios/bios.c:282` vs `:331`), a
`vdi_vst_load_fonts` (VDI 119) jest no-opem bez GDOS (`vdi/vdi_text.c:1292-1294`).

### 5.6 Co się dzieje, gdy program zawoła `appl_init` → `wind_open` → `v_opnvwk` zaraz po starcie

| Wywołanie | Co musi istnieć | Skutek gdy brakuje |
|---|---|---|
| `appl_init` | `0x88 → _aestrap` (`aes/geminit.c:880`), nie-NULL `rlr` z `p_uda` (`aes/geminit.c:911-921`), `D` wyzerowane i zainicjowane (`aes/gemstart.S:105`), `gl_nplanes` (`aes/gemgraf.c:395`) | wektor TRAP #2 prowadzi do `bdos_trap2`/`vditrap` → `d0=0xC8` nie jest rozpoznane, program dostaje śmieciowy kod powrotu albo `RTS` do `old_trap2`; w kontekście A (AUTO) AES w ogóle jeszcze nie istnieje |
| `wind_open` | dodatkowo: `wm_start()` (`aes/geminit.c:761`), `gem_rsc_init()` (`:693`), `gem_rsc_fixit()` (`:741`), otwarta stacja fizyczna (`gl_handle`, `:705`) | brak `gl_handle`/zasobów → okno bez poprawnego drzewa obiektów, wyjątki w `gemwmlib.c`; `tree[ROOT].ob_height` ustawiane z `gl_rscreen.g_h` (`aes/geminit.c:747`) |
| `v_opnvwk` | dodatkowo: `0x88 → _aestrap` (bo `gsx2()` idzie przez TRAP #2, `aes/gsx2.c:24-32`), `_local_pb`/`CONTRL` (`bios/lineavars.S:271-276`), `vwk_ptr` z uchwytem fizycznym (`vdi/vdi_control.c:543`), `DEV_TAB` skopiowane przez `v_opnwk` (`vdi/vdi_control.c:533-536`) | `CONTRL[6] = 0` (brak uchwytu) i `work_out[]` z zerowego `DEV_TAB` |

Dodatkowo `gsx2()` **wymaga**, by VDI było osiągalne przez TRAP #2 w chwili wywołania:

```
aes/gsx2.c:20: void gsx2(void)
aes/gsx2.c:21: {
aes/gsx2.c:22:     vdipb.contrl = contrl;
aes/gsx2.c:23:
aes/gsx2.c:24:     __asm__ volatile
aes/gsx2.c:25:     (
aes/gsx2.c:26:         "move.l  %0,d1\n\t"
aes/gsx2.c:27:         "moveq   #0x73,d0\n\t"
aes/gsx2.c:28:         "trap    #2"
```

---

## 6. Ekran — co musi zawierać `_v_bas_ad`, `physbase`, `logbase` i zmienne rozdzielczości

### 6.1 Podział pamięci i alokacja ekranu

```
bios/biosmem.c:31:  * when TOS is in ROM, the ST RAM is used thus:
bios/biosmem.c:32:  *   0        400       800   end                         v_bas_ad   phystop
bios/biosmem.c:33:  *   +---------+---------+-----+------------------------------+--------+
bios/biosmem.c:34:  *   | vectors | sysvars | BSS | Transient Program Area (TPA) | screen |
bios/biosmem.c:35:  *   +---------+---------+-----+------------------------------+--------+
```

```
bios/biosmem.c:65:     /* Start of available ST-RAM */
bios/biosmem.c:66:     end_os = _end_os_stram;
bios/biosmem.c:67:     membot = end_os;
bios/biosmem.c:70:     /* End of available ST-RAM */
bios/biosmem.c:71:     /* The screen buffer will be allocated later */
bios/biosmem.c:72:     memtop = phystop;
```

Ekran jest alokowany **od góry** ST-RAM, co automatycznie zmniejsza `_memtop` (TPA):

```
bios/screen.c:635: /* Initialize the video address (mode is already set) */
bios/screen.c:636: void screen_init_address(void)
bios/screen.c:637: {
bios/screen.c:638:     LONG vram_size;
bios/screen.c:639:     UBYTE *screen_start;
...
bios/screen.c:645:     vram_size = calc_vram_size();
bios/screen.c:646:     /* videoram is placed just below the phystop */
bios/screen.c:647:     screen_start = balloc_stram(vram_size, TRUE);
...
bios/screen.c:655:     /* set new v_bas_ad */
bios/screen.c:656:     v_bas_ad = screen_start;
bios/screen.c:657:     KDEBUG(("v_bas_ad = %p, vram_size = %lu\n", v_bas_ad, vram_size));
bios/screen.c:658:     /* correct physical address */
bios/screen.c:659:     setphys(screen_start);
bios/screen.c:660: }
```

```
bios/biosmem.c:112:     if (top)
bios/biosmem.c:113:     {
bios/biosmem.c:114:         /* Allocate the new buffer at the top of the ST-RAM */
bios/biosmem.c:115:         memtop -= size;
bios/biosmem.c:116:         ret = memtop;
bios/biosmem.c:117:     }
```

Kolejność w `bios_init`: `mfp_init` → `screen_init_mode` (`bios/bios.c:327`) →
`linea_init` (`:331`) → `screen_init_address` (`:338`) → `vt52_init` (`:341`) → `bmem_init`
był **wcześniej** (`:254`), a `getmpb`/`pmd` dopiero w `umem_init` (`bdos/bdosmain.c:331`),
czyli **po** odjęciu ekranu. To gwarantuje, że TPA nie nachodzi na ekran.

### 6.2 `physbase` / `logbase`

Trzy różne rzeczy o podobnej nazwie:

1. **`_v_bas_ad` (0x44E)** — logiczny adres bazowy, używany przez BIOS/VT52/VDI do rysowania
   (`bios/conout.c:126`, `vdi/vdi_control.c:618`).
2. **`Logbase()` (XBIOS 3)** — zwraca dokładnie `_v_bas_ad`:

```
bios/screen.c:1071: UBYTE *logbase(void)
bios/screen.c:1072: {
bios/screen.c:1073:     return v_bas_ad;
bios/screen.c:1074: }
```

3. **`Physbase()` (XBIOS 2)** — zwraca adres faktycznie wyświetlany przez sprzęt:

```
bios/screen.c:1058: static void setphys(const UBYTE *addr)
...
bios/screen.c:1066: #elif CONF_WITH_ATARI_VIDEO
bios/screen.c:1067:     atari_setphys(addr);
```

```
bios/xbios.c:80:     return physbase();
bios/xbios.c:96:     return logbase();
```

`Setscreen` aktualizuje jedno i drugie oraz (dla Videl) realokuje pamięć:

```
bios/screen.c:1102: WORD setscreen(UBYTE *logLoc, const UBYTE *physLoc, WORD rez, WORD videlmode)
bios/screen.c:1103: {
bios/screen.c:1104:     WORD oldmode = 0;
bios/screen.c:1105:
bios/screen.c:1106:     if ((LONG)logLoc > 0) {
bios/screen.c:1107:         v_bas_ad = logLoc;
bios/screen.c:1108:         KDEBUG(("v_bas_ad = %p\n", v_bas_ad));
bios/screen.c:1109:     }
bios/screen.c:1110:     if ((LONG)physLoc > 0) {
bios/screen.c:1111:         setphys(physLoc);
bios/screen.c:1112:     }
```

Zmienna `_screenpt` (0x45E) jest mechanizmem odroczenia: VBL podstawia ją do `_v_bas_ad`
i do rejestrów sprzętowych:

```
bios/vectors.S:393:         // set new video address
bios/vectors.S:394:         move.l  _screenpt.w,d0
bios/vectors.S:395:         jeq     vbl_no_screenpt
bios/vectors.S:396:         move.l  d0,_v_bas_ad.w
```

Paleta: `Setpalette` (XBIOS 6) tylko **zapisuje wskaźnik**, a przeładowanie sprzętu robi VBL:

```
bios/screen.c:1182:     colorptr = palettePtr;
```

```
bios/vectors.S:367:         move.l  _colorptr.w,d0
bios/vectors.S:368:         jeq     vbl_no_palette
bios/vectors.S:369:         // load new color palette
bios/vectors.S:370:         clr.l   _colorptr.w
```

### 6.3 Rozdzielczość: `_sshiftmod` / `_defshiftmod` i zmienne Line-A

`_sshiftmod` (0x44C) to kopia rejestru 0xffff8260 (ST/STe) lub podobnego dla TT/Videl:

```
bios/screen.c:998:             videl_setrez(rez, videlmode);   /* sets 'sshiftmod' */
bios/screen.c:1007:             *(volatile UBYTE *)TT_SHIFTER = sshiftmod = rez;
bios/screen.c:1011:         *(volatile UBYTE *)ST_SHIFTER = sshiftmod = rez;
```

```
bios/screen.c:1076: WORD getrez(void)
bios/screen.c:1077: {
bios/screen.c:1078: #if CONF_WITH_ATARI_VIDEO
bios/screen.c:1079:     return atari_getrez();
bios/screen.c:1080: #else
bios/screen.c:1081:     /* No video hardware, return the logical video mode */
bios/screen.c:1082:     return sshiftmod;
bios/screen.c:1083: #endif
```

`_defshiftmod` (0x44A) jest tylko **czytane** przy zmianie rozdzielczości:

```
bios/screen.c:1300:         /* change to a valid resolution */
bios/screen.c:1302:         newres = defshiftmod;   /* use default shifter mode */
...
bios/screen.c:1327:     sshiftmod = newres;
```

Zmienne Line-A rozdzielczości są wyliczane w `linea_init`/`update_rez_dependent`:

```
bios/lineainit.c:50: void linea_init(void)
bios/lineainit.c:51: {
bios/lineainit.c:52:     screen_get_current_mode_info(&v_planes, &V_REZ_HZ, &V_REZ_VT);
bios/lineainit.c:53:
bios/lineainit.c:54:     /* precalculate shift value to optimize pixel address calculations */
bios/lineainit.c:55:     set_screen_shift();
bios/lineainit.c:56:
bios/lineainit.c:57:     /* update resolution-dependent values */
bios/lineainit.c:58:     update_rez_dependent();
```

```
vdi/vdi_control.c:188: /*
vdi/vdi_control.c:189:  * update resolution-dependent VDI/lineA variables
vdi/vdi_control.c:190:  *
vdi/vdi_control.c:191:  * this function assumes that v_planes, V_REZ_HZ, V_REZ_VT are already set
vdi/vdi_control.c:192:  */
vdi/vdi_control.c:193: void update_rez_dependent(void)
vdi/vdi_control.c:194: {
vdi/vdi_control.c:195:     BYTES_LIN = v_lin_wr = V_REZ_HZ / 8 * v_planes;
...
vdi/vdi_control.c:203:     DEV_TAB[0] = V_REZ_HZ - 1;
vdi/vdi_control.c:204:     DEV_TAB[1] = V_REZ_VT - 1;
vdi/vdi_control.c:205:     get_pixel_size(&DEV_TAB[3],&DEV_TAB[4]);
vdi/vdi_control.c:206:     DEV_TAB[13] = (v_planes<8) ? (1 << v_planes) : 256;
```

Pełna lista zmiennych Line-A (z offsetami względem `line_a_vars`, w tym **ujemnymi**):
`bios/lineavars.S:9-197` (blok ujemny) i `:264-332` (blok dodatni, `+0` = `v_planes`,
`+2` = `v_lin_wr`, `+4..+20` = wskaźniki `local_pb`).

### 6.4 Co widzi program otwierający okno GEM

Stan „ekran gotowy dla GEM" powstaje dopiero w `run_accs_and_desktop` (`aes/geminit.c:684-780`):

1. `gsx_init()` (`:705`) → `gsx_wsopen()` → `v_opnwk` → `init_colors()`, `text_init()`,
   `timer_init()`, `vdimouse_init()`, `esc_init()` (`vdi/vdi_control.c:550-564`).
2. `gsx_init` ustawia też procedury myszy i pobiera `xrat`/`yrat` z `ptsout`:

```
aes/gemgsxif.c:267: void gsx_init(void)
aes/gemgsxif.c:268: {
aes/gemgsxif.c:269:     gsx_wsopen();
aes/gemgsxif.c:270:     gsx_start();
aes/gemgsxif.c:271:     gsx_setmb(far_bcha, far_mcha, &drwaddr);
aes/gemgsxif.c:272:     gsx_0code(MOUSE_STATE);
aes/gemgsxif.c:273:     xrat = ptsout[0];
aes/gemgsxif.c:274:     yrat = ptsout[1];
```

3. `gl_width/gl_height/gl_nplanes/gl_clip/gl_rscreen`:

```
aes/gemgraf.c:393:     gl_width = gl_clip.g_w = gl_ws.ws_xres + 1;
aes/gemgraf.c:394:     gl_height = gl_clip.g_h = gl_ws.ws_yres + 1;
aes/gemgraf.c:395:     gl_nplanes = gsx_nplanes();
```

```
aes/gemgraf.c:423:     r_set(&gl_rscreen, 0, 0, gl_width, gl_height);
```

4. Kurzor myszy rysowany jest z VBL-a, więc **musi istnieć slot 0 `_vbl_list`**:

```
vdi/vdi_mouse.c:602:     vblqueue[0] = vb_draw;      /* set GEM VBL-routine to the first VBL slot */
vdi/vdi_mouse.c:605:     Initmous(1, (LONG)&mouse_params, (LONG)mouse_int);
```

5. VBL musi być włączony: `vblsem = 1` (`bios/bios.c:580`) — bez tego `_int_vbl` kończy
   natychmiast (`bios/vectors.S:338-340`) i nie ma ani kursora, ani `vbclock`.
6. `sh_tographic()` (`aes/geminit.c:730`) przełącza VDI w tryb graficzny i przejmuje wektor
   błędu krytycznego (`takeerr()`, `:727`).

Program, który wywoła `wind_open` bezpośrednio po starcie z pulpitu, zastanie zatem: otwartą
stację fizyczną, wypełnione `DEV_TAB/SIZ_TAB/INQ_TAB`, działający VBL z `vb_draw`,
zainstalowane procedury myszy i poprawny `_v_bas_ad`/`physbase` ustawione przy starcie ekranu.

---

## 7. Checklista uruchomieniowa

Tabela dotyczy **minimalnego** stanu przed skokiem do punktu wejścia `.PRG` (`p_tbase`).
Kolumna „Konsekwencja braku" opisuje typowy objaw.

| # | Element | Gdzie EmuTOS to ustawia | Konsekwencja braku |
|---|---|---|---|
| 1 | Wektory wyjątków 2–62 (`deflt_vec_table`) | `bios/bios.c:143` → `bios/vectors.S:103-118` | bus error/illegal → `any_vec` bez paniki lub dziki skok; brak zrzutu 0x380 |
| 2 | `VEC_LEVEL1..7` = `just_rte` | `bios/bios.c:152-158` | nieoczekiwane przerwanie → panic (np. „Exception number 27" przy sterownikach MiNT) |
| 3 | `VEC_DIVNULL` = `just_rte` | `bios/bios.c:169` | dzielenie przez zero w programie zabija system (TOS to ignoruje) |
| 4 | `VEC_ILLEGAL` / `VEC_PRIVLGE` (emulacja `move sr`/`move ccr`) | `bios/bios.c:182-192` | programy 68000 robiące `move sr` padają |
| 5 | `VEC_LINEA` = `int_linea` | `bios/bios.c:176` → `bios/linea.S:51` | każdy program graficzny używający Line-A dostaje panic |
| 6 | `VEC_BIOS` = `biostrap`, `VEC_XBIOS` = `xbiostrap` | `bios/bios.c:174-175` → `bios/vectors.S:1268-1277` | `trap #13`/`trap #14` nie działa |
| 7 | `_savptr` = `trap_save_area` | `bios/bios.c:359` | BIOS/XBIOS niszczy stos (zapis rejestrów pod złym adresem) |
| 8 | TRAP #1 → `_enter` | `bdos/bdosmain.c:320` | brak GEMDOS: `Pterm`, `Fopen`, `Malloc` itd. nie działają |
| 9 | TRAP #2 → `_vditrap` | `bios/bios.c:173` | brak VDI |
| 10 | TRAP #2 → `bdos_trap2` (łańcuch do `vditrap`) | `bdos/bdosmain.c:326` | brak `Pterm0` przez TRAP #2 (d0=0) i brak `xterm` z TRAP #2 |
| 11 | TRAP #2 → `_aestrap` (łańcuch do `bdos_trap2`) | `aes/geminit.c:880` + `aes/gemdosif.S:295-303` | `appl_init`/`wind_open` nie działają (tylko kontekst B) |
| 12 | `_longframe` (typ CPU) | `bios/processor.S:262` | błędna interpretacja ramek wyjątków → krach GEMDOS/BIOS |
| 13 | `_phystop`, `_memvalid`, `_memval2`, `_memval3`, `memctrl` | `bios/memory.S:572-586` | kolejny rozruch nie rozpozna RAM; `_phystop` = 0 → `Malloc`/ekran bez pamięci |
| 14 | `_membot`/`_memtop` + `_themd` (getmpb → `pmd`) | `bios/biosmem.c:66-72,144-147`; `bdos/umem.c:555` | brak sterty GEMDOS; `Malloc` zwraca 0; `Pexec` nie ma gdzie załadować programu |
| 15 | `_v_bas_ad` + `physbase` + `_sshiftmod` + `V_REZ_HZ/VT` + `v_lin_wr` | `bios/screen.c:656,659`; `bios/lineainit.c:52`; `vdi/vdi_control.c:195` | biały/śmieciowy ekran, konsola VT52 pisze w losowe miejsce |
| 16 | `_vblsem` = 1, `_nvbls`, `_vblqueue` = `_vbl_list`, wpisy NULL | `bios/bios.c:366-372`, `bios/bios.c:580` | brak VBL: brak `_vbclock`, brak kursora myszy, brak obsługi palety/`screenpt` |
| 17 | `_hz_200`, `_timer_ms`, `_frclock`, `_vbclock`, `_swv_vec` | `bios/vectors.S:503,330,348`; `bios/mfp.c:196`; `bios/bios.c:579` | opóźnienia (`Delay`, pętle `while(hz_200<...)`) zawieszają się; `vsync()` nigdy nie wraca (`bios/screen.c:1217`) |
| 18 | `_bufl[2]` (listy buforów GEMDOS) | `bdos/fsbuf.c:56-71` | awaria przy pierwszym `Fread`/`Fwrite` |
| 19 | `_dskbufp` = `dskbuf` | `bios/blkdev.c:91` | brak sektorowego bufora rozruchu i AHDI/ASCI |
| 20 | `_drvbits`, `_nflops`, `_bootdev` | `bios/blkdev.c:175,392`; `bios/floppy.c:319,386`; `bios/bios.c:826` | `Dgetdrv`, `Fsfirst` na innym dysku, `Drvmap` zwraca 0 |
| 21 | `_pun_ptr` = `&pun_info` | `bios/blkdev.c:161` | AHDI/XHDI i sterowniki dyskowe nie widzą tablicy urządzeń |
| 22 | `hdv_rw/hdv_bpb/hdv_boot/hdv_mediach/hdv_init` | `bios/blkdev.c:95-103` | brak dostępu do dysku przez BIOS/GEMDOS |
| 23 | `_conterm` = 7, `shifty` = 0, `con_state` poprawny | `bios/ikbd.c:1100,1102`; `bios/vt52.c:190,244,253` | brak kliku/powtórzeń/dzwonka; `conout` gubi stan ESC |
| 24 | `ikbdiorec` + bufory + `head/tail` + `low/high` | `bios/aciavecs.S:197-201,235-241` | `Bconin(2)`/`Crawio` czytają śmieci albo wiszą |
| 25 | `kbdvecs` (wszystkie 9 wektorów) + `kbdvec`/`ikbdstate`/`kbdlength` | `bios/aciavecs.S:146-208` | brak klawiatury, MIDI, zegara RTC z IKBD, joysticka |
| 26 | `conterm`/`shifty` widoczne dla użytkownika (sekcja `.low_stram`) | `bios/lowstram.c:53`, `emutos.ld:105-113` | programy czytające `shifty` przez `os_kbshift` dostają śmieci |
| 27 | `_p_cookies` = `dflt_jar` (+ cookies `_MCH`, `_IDT`, `_FRB`…) | `util/cookie.c:31`; `bios/machine.c:694` | `Getcookie` zwraca 0; programy sprawdzające maszynę/FRB zachowują się jak na obcej maszynie |
| 28 | `_etv_timer` = `tikfrk`, `_etv_critic`, `_etv_term` | `bdos/time.c:166`; `bios/bios.c:361-362` | brak tykania zegara GEMDOS; `Pterm` nie woła handlera użytkownika |
| 29 | `_bell_hook`/`_kcl_hook` | `bios/sound.c:90-91` | `Bconout(1,7)`/klik powoduje skok pod adres 0 |
| 30 | `_sysbase`, `_end_os`, `_exec_os` | `bios/bios.c:358`; `bios/biosmem.c:66`; `bios/bios.c:561` | programy weryfikujące wersję TOS / kończące się na `end_os` błędnie liczą pamięć |
| 31 | `_dumpflg` = −1, `_dump_vec` | `bios/bios.c:357`; `bios/chardev.c:84` | natychmiastowy zrzut ekranu w pierwszym VBL (`bios/vectors.S:467`) |
| 32 | Basepage: `p_lowtpa`, `p_hitpa`, `p_tbase`, `p_dbase`, `p_bbase`, `p_dlen`, `p_blen`, `p_flags`, `p_env` | `bdos/proc.c:364-384`; `bdos/kpgmld.c` | program nie wie, gdzie kończy się jego pamięć → `Mshrink`/`Malloc` psują system |
| 33 | `p_xdta` (domyślnie `p_cmdlin`) | `bdos/proc.c:375` | `Fsfirst`/`Fsnext` piszą pod adres 0 |
| 34 | `p_cmdlin` @ basepage+0x80 (bajt 0 = długość) | `bdos/proc.c:378-383`; `cli/cmdexec.c` (`cmdline[0] = len`) | program widzi puste/śmieciowe argumenty |
| 35 | `p_uft[0..5]` (standardowe uchwyty) | `bdos/proc.c:387-398`; `bdos/console.c:107-126` | brak konsoli (`Cconws`/`Crawio` nie działa) |
| 36 | `p_curdir[]`, `p_curdrv` (odziedziczone) | `bdos/proc.c:400-409` | `Fopen("PLIK")` szuka w złym katalogu |
| 37 | Stos: USP = `p_hitpa−8`, 4(sp) = basepage, d1–d7/a0–a3 = 0, a4/a5 = BSS/DATA, tryb użytkownika | `bdos/proc.c:536-577`; `bdos/rwa.S:234-259` | program nie znajdzie basepage’a (konwencja `4(sp)`), a test „a0==0" (akcesorium vs program) da zły wynik |
| 38 | VDI: `vwk_ptr[1]` = `&phys_work`, `DEV_TAB/SIZ_TAB/INQ_TAB`, `LINEA` (`v_planes`, `v_lin_wr`), `CUR_WORK` | `vdi/vdi_control.c:528-567` | `v_opnvwk` zwraca `handle=0` lub `work_out[]` pełne zer |
| 39 | VDI: `_vbl_list[0]` = `vb_draw`, `mousevec` = `mouse_int` | `vdi/vdi_mouse.c:602,605` | brak kursora myszy i brak zdarzeń ruchu |
| 40 | AES: `0x88` → `_aestrap` z zachowaniem `savetrap2`, `rlr`/UDA/stos procesu 0, `D` (THEGLO) wyzerowane | `aes/geminit.c:880,911-921`; `aes/gemstart.S:99-106`; `aes/geminit.c:168-175` | `appl_init` zwraca bezsensowny pid albo system pada w `trapaes` |
| 41 | AES: `gl_handle`/`gl_nplanes`/`gl_rscreen`/`gl_clip` (z `v_opnwk`) | `aes/gemgsxif.c:185-186`; `aes/gemgraf.c:393-395,423` | `wind_open`/`wind_create` operują na zerowym ekranie |
| 42 | AES: zasoby GEM (`gem_rsc_init`, `gem_rsc_fixit`), `wm_start`, `fs_start`, `mn_init` | `aes/geminit.c:693,741,761,762,866` | okna/dialogi/menu nie mają drzew obiektów |
| 43 | Środowisko: `PATH=` + litera dysku + podwójny NUL, skopiowane do `p_env` | `bios/bios.c:653-663`; `bdos/proc.c:413-431` | program uruchamiający inne programy nie znajdzie plików |

### 7.1 Kolejność, w jakiej EmuTOS faktycznie to robi (skrót)

```
bios/startup.S:535   → _biosmain
bios/bios.c:813      → bios_init()
bios/bios.c:242      →   vecs_init()            (pkt 1-7)
bios/bios.c:254      →   bmem_init()            (_end_os, _membot, _memtop, _themd)
bios/bios.c:278-282  →   cookie_init(), cookie jar, font_init()
bios/bios.c:327-341  →   screen_init_mode(), linea_init(), screen_init_address(), vt52_init()  (pkt 15)
bios/bios.c:357-373  →   _dumpflg, _sysbase, _savptr, etv_*, swv_vec, _nvbls, _vblqueue  (pkt 7,16,28,30,31)
bios/bios.c:384      →   init_system_timer()   (_timer_ms, pkt 17)
bios/bios.c:429-458  →   chardev_init(), kbd_init(), midi_init(), init_acia_vecs()  (pkt 23-26,29)
bios/bios.c:504      →   blkdev_init()         (_dskbufp, hdv_*, _drvbits, _nflops, _pun_ptr)  (pkt 19-22)
bios/bios.c:564-574  →   osinit_before/after_xmaddalt()  (TRAP#1, TRAP#2, _bufl, pmd, _etv_timer, uchwyty)  (pkt 8,10,14,18,28,35)
bios/bios.c:579-580  →   swv_vec = os_header.reseth; vblsem = 1  (pkt 16,17)
bios/bios.c:818-855  →   Drvmap(), _bootdev, Dsetdrv(), środowisko  (pkt 20,43)
bios/bios.c:873      →   autoexec()            ← KONTEKST A: pierwszy możliwy start .PRG
bios/bios.c:877-892  →   COMMAND.PRG lub exec_os (= _ui_start → AES)
aes/geminit.c:783    →   gem_main() → ... → aes/geminit.c:705 gsx_init()  (pkt 38,39,41)
aes/geminit.c:761-766→   wm_start(), fs_start(), dsptch()  (pkt 42)
                     →   shell uruchamia .PRG z Pexec PE_LOADGO  ← KONTEKST B
```

---

## 8. Podsumowanie różnic wobec „klasycznego" opisu TOS (żeby nie powielać błędów)

1. **Nie ma bloku AES/VDI pod stałym adresem 0x5AC.** AES i VDI dostają wskaźnik do bloku
   parametrów w `d1` (`aes/gemsuper.h:13-20`, `aes/gsx2.h:17-24`, `vdi/vdi_asm.S:74-83`).
   Adresy 0x5A0–0x5B0 w EmuTOS zajmują `_p_cookies`/`_ramtop`/`_ramvalid`/`_bell_hook`/`_kcl_hook`
   (`tosvars.ld:106-110`).
2. **Nie ma `_mch`, `_sndbits`, `_pdb`** — patrz 1.3. Identyfikator maszyny jest w cookie `_MCH`.
3. **`KBDVECS` nie ma pola `vmousevec`** — jest `mousevec` (offset +0x10), a przed całą strukturą
   leży `kbdvec` (offset −0x04) (`bios/aciavecs.S:127-141`).
4. **Nie ma zapisu/odtworzenia wektorów per proces** (jak w MiNT). Per proces zapisywane są
   wyłącznie rejestry w basepage (`P_REGSAVE` = 0x68) plus `etv_term`; TRAP #2 jest zapisywany
   i odtwarzany **globalnie** przez AES (`aes/gemdosif.S:287-304`).
5. **Komentarz `bios/biosmem.c:49`** podaje adres `phystop` = 0x43a, co jest nieprawdą
   (poprawnie: `_phystop` = 0x42E, 0x43A = `_memval2`).
6. **Programy z `\AUTO\` startują przed AES-em** (`bios/bios.c:873` przed `:883`) — dla nich
   obowiązują wyłącznie punkty 1–37 checklisty; punkty 38–42 są nieosiągalne.
