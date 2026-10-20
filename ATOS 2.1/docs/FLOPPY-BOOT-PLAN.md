# Plan: boot z flopa (WD1772 + FAT12)

Cel: gdy brak bootowalnego dysku (lub wybrano floppy), wystartować ATOS z
dyskietki; wariant awaryjny: wczytać `emutos.prg` i uruchomić po resecie.

## Sprzęt (ST/STE) — protokół zweryfikowany na referencji EmuTOS

Ścieżka referencyjna: `files-pasted-by-the-user-projekt/work/emutos-reference/bios/{fdc.h,dma.h,floppy.c,dma.c}`.

Dostęp do FDC jest **pośredni przez DMA** (0xFFFF8604 dane, 0xFFFF8606 sterowanie):
`set_fdc_reg(reg,val)`: `REG16(0xffff8606)=reg; delay; REG16(0xffff8604)=val;`
`get_fdc_reg(reg)`: `REG16(0xffff8606)=reg; delay; return REG16(0xffff8604);`

Rejestry FDC (wartości control):
- `FDC_CS=0x0080`, `FDC_TR=0x0082`, `FDC_SR=0x0084`, `FDC_DR=0x0086`
- komendy: `RESTORE=0x00`, `SEEK=0x10`, `READ=0x80`, `WRITE=0xA0`, `FORCE_IRQ=0xD0`
- status: bit0 BUSY, bit1 DRQ, bit4 RNF, bit5 typ/spin-up, bit7 MOTORON

Selekcja stacji/boku — **PSG port A** (14):
`REG8(0xffff8800)=14; old=REG8(0xffff8800); REG8(0xffff8802)=(old&0xF8)|n;`
gdzie `n`: domyślnie 0x07; stacja A → `n&=~2`; stacja B → `n&=~4`;
bok 1 → `n&=~1`. Czyli A/bok0 = `0x05`, A/bok1 = `0x04`.

Adres DMA: `REG8(0xffff860d)=addr&0xff; REG8(0xffff860b)=(addr>>8)&0xff; REG8(0xffff8609)=(addr>>16)&0xff;`

Start transferu czytania (przełącz, by wyczyścić bufory DMA):
`REG16(0xffff8606)=0x0190; delay; REG16(0xffff8606)=0x0090; delay; REG16(0xffff8604)=count;`

Sekwencja odczytu sektora (z EmuTOS `floppy.c`):
1. select(drive,side) — PSG,
2. set_track: jeśli zmiana toru → `set_fdc_reg(FDC_DR,track)` + `FDC_SEEK`; czekaj na GPIP4,
3. `set_fdc_reg(FDC_SR,sector)`,
4. set_dma_addr(buffer),
5. `fdc_start_dma_read(1)`,
6. `set_fdc_reg(FDC_CS,FDC_READ)`,
7. czekaj na IRQ FDC = **MFP GPIP4** (`0xfffffa01` bit4 schodzi w dół), timeout ~1.5 s (motor on) / 3 s,
8. odczytaj `get_fdc_reg(FDC_CS)` i zdekoduj (RNF/CRC).

Motor: pierwszy odczyt po włączeniu wymaga spin-upu — powtórz/próbkuj, aż status ma MOTORON i odczyt się uda. DD vs HD: na MegaSTE/TT/Falcon dochodzi `0xffff860e` (bit0 freq, bit1 gęstość) — na ST/STE nie. **(ZROBIONE: `hw/fdc.c:fdc_clock_init()` ustawia 8 MHz + DD na MegaSTE/TT/Falcon przed pierwszym odczytem.)**

## Etapy
1. `include/fdc.h` + `hw/fdc.c`: `fdc_read_sector(drive,track,side,sector,buf)` (powyższe).
   Test: Hatari `--disk-a <obraz FAT12>`, odczyt toru 0/sektora 1, porównanie z bajtami obrazu.
2. `hw/fat12.c`: mount (BPB 512 B/sector), odczyt katalogu i pliku (`KERNEL.SYS`, `BOOT.SYS`).
   Uwaga: nasz `BOOT.SYS`/`KERNEL.SYS` są w `SYSTEM` na dysku twardym;
   na flop działa prościej wariant „płaski" (pliki w korzeniu) albo FAT12 z podkatalogiem.
3. Integracja w `loader/bootstage.c`: gdy skan nie znajdzie dysku (lub
   floppy ma priorytet), spróbuj stacji A; `NO BOOT DEVICE - RETRY?` zostaje.
4. Wariant `emutos.prg`: po nieudanym boocie wczytaj plik z flopa i
   przekaż do `tos_load_once` (już istnieje dla HDD).
5. `tools/mk_floppy.py`: zbuduj 720 KB FAT12 z `BOOT.SYS`, `KERNEL.SYS`,
   modułami i basic shell-em (~150 KB zmieści się).

## Stan wdrożenia (2026-09-26)

**Etap 1 — ZROBIONE.** `include/fdc.h` + `hw/fdc.c` czytają sektor przez
WD1772/DMA; podpięte do builda BIOS-u (`build/hw/fdc.o`), `tools/mk_floppy.py`
generuje obraz testowy, a F3 HARDWARE pokazuje `FLOPPY A  READ OK, SIG ...`.

Weryfikacja w Hatari (`--disk-a /tmp/atos-floppy.st`, boot-sektor `ATOSFLOPPY`):
`fdc_dbg_status=1`, `fdc_dbg_sig=0x41544f53` (`ATOS`), na ekranie
`FLOPPY A  READ OK, SIG ATOS`.

Kluczowa poprawka: oczekiwanie na koniec komendy przez **bit BUSY rejestru
statusu FDC**, nie przez MFP GPIP4 — GPIP4 potrafi czytać 0 dla zastanego
przerwania i wtedy `fdc_wait` wracał przed transferem DMA (dane lądowały
później, więc sygnatura była pusta).

**Etap 2 — ZROBIONE.** `include/fat12.h` + `hw/fat12.c`: mount (BPB), szukanie
w katalogu głównym, łańcuch klastrów 12-bit, odczyt pliku. Probe w F3 pokazuje
`FAT12  TEST.TXT OK, SIG HELL`; `fat12_dbg_status=1`, `fat12_dbg_sig=0x48454c4c`.
`tools/mk_floppy.py` buduje teraz prawdziwy FAT12 z `TEST.TXT`.

**Etap B — FDD w systemie ZROBIONE (2026-09-26).** Handoff v10 z callbackami
`floppy_mount/dir/read_file`, syscall 63/64, komenda `fdd` i `fdd NAME` w
Commanderze (`View: FDD`). Shell: do zrobienia (API modułu).

**Etap API (shell+Commander) ZROBIONE (2026-09-26).** `fdd` w shellu i Commanderze,
API modułu v4 (offsety 176/180/184).

**Etap 3 — start systemu z FDD ZROBIONE (2026-09-26).** System bootuje w całości
z dyskietki 720 KB: BIOS czyta `BOOT.SYS` i `KERNEL.SYS`, a jądro montuje FAT na
flopie i ładuje z niego moduły. Jak to działa:

1. `loader/bootstage.c` skanuje ACSI 0..7 i IDE 0..1; gdy nic nie znajdzie,
   próbuje stacji A. Floppy ma własny id `BLOCK_FLOPPY_ID 0xfe`
   (`include/block_device.h`), a `hw/acsi.c` kieruje ten id do `fdc_read_sector`.
2. Dla flopa loader ustawia `acsi_part_id[0] = "FDD"`, `part = 0`, `start = 0`;
   dalsza ścieżka (`locate_system_file`/`load_image`/`load_module_service`)
   działa **bez zmian** — czyta przez `acsi_read_sector`.
3. `system/kernel_fat.c`: `kfat_mount` przyjmuje `start == 0` (wcześniej odrzucał
   wolumen systemowy) oraz małe liczby klastrów (`clusters >= 2`); jądro ma
   fallback `kfat_mount(0u, "FDD")`, gdy tabela AHDI jest niepoprawna.
4. **Szerokość FAT:** loader (`loader/bootstage.c`) i jądro
   (`system/kernel_fat.c`) wybierają 12 vs 16 bitów z liczby klastrów
   (`< 4085` = FAT12), z obsługą wpisu przechodzącego przez granicę sektora i
   progami końca łańcucha (FAT12 EOC = `0xFF8`, FAT16 = `0xFFF8`); zapis do
   FAT12 jest zablokowany. Dzięki temu dyskietka rozruchowa to **prawdziwy
   FAT12**, a ten sam obraz czyta ROM-owy `hw/fat12.c` (`fdd`).
5. Obraz: 720 KB, BPB spc=2/spf=3/root=112; `\SYSTEM\` trzyma BOOT.SYS,
   KERNEL.SYS, moduły (ACSI/CORE/VFS/PROC/TIMER/EVENT/INPUT/SHELL/VOFA),
   INIT.APP, GFX.APP, GUI.APP, a `\SYSTEM\APP\` — COMMAND.APP, SYSINFO.APP
   i HW.APP (zgodnie z PATH jądra). Razem ~166 z 714 klastrów.

**Dowód (Hatari, tylko `--disk-a`, bez ACSI/IDE):**
`docs/screenshots/boot/fdboot-shell.png` — `FLOPPY FAT: MOUNTED`,
`KERNEL STATUS: RUNNING / PREEMPTIVE / SHELL READY`, `VIDEO: VOFA (ROM INIT)`,
`STARTUP SERVICES: SYSTEM.INIT/GFX/GUI READY` i prompt `tts1 C:\>`.
Scenariusz: `atos2-tools/scenario_fdboot.py`; wcześniejsze etapy widać w
`fdboot-shell.png` (loader: `FOUND FLOPPY A:`, `PARTITION: FDD 1`,
`STARTING BOOT.SYS...`). **Uwaga:** boot z flopa jest wolny (~3 min w Hatari)
przez cykl-dokładną emulację WD1772; na realnym sprzęcie to kwestia sekund.

**F4 BOOT z FLOPPY ZROBIONE (2026-09-26).** `ui/boot.c` wykrywa stację A
(`floppy_bootable()`), pokazuje `DETECTED FDD` / `DEVICE FLOPPY` /
`BOOT PARTITION P0 FDD START 0` i zapisuje `boot_valid=1, boot_id=0xfe`,
`boot_part=0`; `loader/bootstage.c` obsługuje zapamiętany flop
(`REMEMBERED BOOT FLOPPY UNAVAILABLE`, gdy brak dyskietki). Dowód:
`docs/screenshots/boot/bootmenu-floppy.png`; z HDD+flopem zapis dał
`boot_valid 1 boot_id 0xfe` w `\BIOS\BIOS.CFG`.

## Ryzyka
- Timing/spin-up i gęstość (DD/HD), różnice MegaSTE/TT/Falcon (rejestr 0x860e, inny select).
- Współdzielenie DMA z ACSI/IDE — trzeba trzymać „flock" (żadnych operacji dyskowych w trakcie).
- Rozmiar: `SYSTEM` ~150 KB → 720 KB flopa wystarcza z zapasem.
