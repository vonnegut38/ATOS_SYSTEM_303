# PACKAGE - wdrożenie na realny sprzęt

Konfiguracja docelowa: **Mega ST4 + TF536 @ 50 MHz (68030, 64 MB ALT-RAM, IDE)
+ karta ET4000 przez interfejs VOFA w MegaBus + monitor VGA.**

## Zawartość

    rom/bios_mste.bin        256 KB, firmware BIOS (linkowany pod 0xE00000)
    rom/bios_mste_hi.bin     128 KB, starsza połowa (dla 2x EPROM 128K)
    rom/bios_mste_lo.bin     128 KB, młodsza połowa
    SYSTEM/BOOT/BOOT.SYS     drugi etap loadera (handoff)
    SYSTEM/KERNEL/KERNEL.SYS jądro
    SYSTEM/KERNEL/*.SYS      moduły jądra (CORE, VFS, PROC, TIMER, EVENT, INPUT, SHELL, ACSI)
    SYSTEM/KERNEL/*.APP      usługi startowe (INIT, GFX, GUI)
    tools/                   skrypty do zbudowania obrazu dysku

Suma kontrolna ROM-u: SHA-256, pierwsze 16 znaków = 8d1fead3416d04d8

## Krok 1 - firmware (EPROM / flash na TF536)

1. Zapisz rom/bios_mste.bin (262144 B) do pamięci ROM karty TF536, mapowanej
   pod 0x00E00000.
2. Jeśli programujesz dwie kości 128 KB, użyj bios_mste_hi.bin (górna) i
   bios_mste_lo.bin (dolna).
3. Po włączeniu BIOS startuje od razu na ET4000 (VGA 640x480). Brak karty =
   start na Shifterze (fallback).

## Krok 2 - dysk bootujący (CF/SD na IDE TF536)

System leży na partycji bootującej w katalogu /SYSTEM.

Wariant A - obraz na PC (zalecane):

    cd PACKAGE
    python3 tools/make_fat_ahdi.py  asf_cf.img 512 ASF    # FAT16 512 MB
    # albo: python3 tools/make_fat32_ahdi.py asf_cf.img 512 ASF   (FAT32)
    # albo: python3 tools/make_xgm_ahdi.py   asf_cf.img 512 ASF   (łańcuch XGM)

    python3 tools/install_path_ahdi.py asf_cf.img SYSTEM/BOOT SYSTEM/BOOT/BOOT.SYS
    python3 tools/install_path_ahdi.py asf_cf.img SYSTEM/KERNEL SYSTEM/KERNEL/KERNEL.SYS
    python3 tools/install_path_ahdi.py asf_cf.img SYSTEM/KERNEL SYSTEM/KERNEL/CORE.SYS SYSTEM/KERNEL/VFS.SYS SYSTEM/KERNEL/PROC.SYS SYSTEM/KERNEL/TIMER.SYS SYSTEM/KERNEL/EVENT.SYS SYSTEM/KERNEL/INPUT.SYS SYSTEM/KERNEL/SHELL.SYS SYSTEM/KERNEL/ACSI.SYS SYSTEM/KERNEL/INIT.APP SYSTEM/KERNEL/GFX.APP SYSTEM/KERNEL/GUI.APP

Zapis obrazu 1:1 na kartę (macOS; X = numer dysku z diskutil list):

    diskutil unmountDisk /dev/diskX
    sudo dd if=asf_cf.img of=/dev/rdiskN bs=1m

Wariant B - karta partycjonowana na Atari (HDDRIVER/HDX): utwórz jedną
partycję GEM/BGM (FAT16) albo F32 (FAT32) i skopiuj na nią katalog SYSTEM
z podkatalogami BOOT i KERNEL.

Obsługiwane: partycje AHDI (GEM/BGM/F32) z łańcuchami XGM oraz FAT16 i FAT32
(32-bitowe klastry; przetestowane 511 MB / 130811 klastrów).

## Krok 3 - pierwszy start i konfiguracja

1. Menu BIOS: F1 Setup - data/godzina RTC, VIDEO DEVICE, BLITTER, BOOT DEVICE
   (AUTO / ACSI / IDE); ENTER zapisuje.
2. F4 Boot Device - wybór dysku (IDE/ACSI) i partycji; ENTER zapisuje.
3. Konfiguracja trafia do C:/BIOS/BIOS.CFG (katalog BIOS powstaje sam).
4. F3 Hardware - inwentaryzacja: CPU, RAM, ALT-RAM, VOFA/ET4000, IDE, ACSI,
   NetUSBee i ROM kartridża.
5. Start systemu: BIOS -> /SYSTEM/BOOT/BOOT.SYS -> /SYSTEM/KERNEL/KERNEL.SYS.

Oczekiwany ekran jądra (na ET4000):

    ATARI SYSTEM KERNEL 0.03
    AHDI VOLUMES: 1
      C: BGM  ... MB  BOOT
    FAT LAYER: FAT16 MOUNTED
    PROCESS MANAGER: 2 TASKS / FULL CTX RR OK
    KERNEL STATUS: RUNNING / PREEMPTIVE / SHELL READY
    STARTUP SERVICES: SYSTEM.INIT Ready / SYSTEM.GFX Ready / SYSTEM.GUI Ready
    C:>

## Rozwiązywanie problemów

| Objaw | Co sprawdzić |
|---|---|
| brak obrazu na VGA | czy karta ET4000/VOFA siedzi w MegaBus; czy ROM pod 0xE00000 |
| BOOT DISK NOT FOUND | czy dysk IDE jest widziany w F3 i wybrany w F1/F4 |
| BOOT.SYS NOT FOUND | czy na partycji bootującej jest katalog SYSTEM/BOOT z BOOT.SYS |
| FAT LAYER: BPB INVALID | partycja nie FAT16/FAT32 albo tabela AHDI uszkodzona |
| SHELL UNAVAILABLE | brakuje modułów w /SYSTEM/KERNEL |

## Czego jeszcze nie ma (świadomie)

- NetUSBee: detekcja działa, ale sterowniki sieci i dysku USB wymagają
  realnego sprzętu (emulator Hatari ich nie emuluje).
- LFN (długie nazwy) - na razie tylko 8.3.
- Ładowanie TOS/EmuTOS z katalogu ROMS - odłożone (PLAN-01 par. 8).