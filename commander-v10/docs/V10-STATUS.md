# ATOS v10 — status wykonania

**Zasada:** boot musi być deterministyczny przed kolejnymi fazami. Fazy 1 i 2 są
zrealizowane i zweryfikowane w Hatari. Reszta planu (3-25) — patrz „Do zrobienia".

## Zrobione i zweryfikowane

### §2 Zabezpieczenie
- Snapshot: commit `cb33ce9` + tag **`commander-v9-snapshot`** (git, źródła; obrazy .img poza repo).
- `commander-v10` = klon v9 (APFS clone, bit-identyczny na starcie); v9 nietknięte.
- Naprawiony błąd snapshotu: `work.img.bak-recon` (768 MB) nie był objęty `*.img`; dodano regułę i usunięto z historii (`.git` ~3 MB).

### §3 Phase 1 — naprawa bootu
- Adres ujednolicony na **0x00080000** (linker, nagłówek, loader).
- Jedno źródło prawdy: `KERNEL_IMAGE_LOAD_ADDRESS` w `include/boot_protocol.h`.
- `loader/bootstage.c` używa makra; czytelna diagnostyka:
  `KERNEL.SYS / LOAD / ENTRY / SIZE / STATUS: OK`, a przy błędzie
  `EXPECTED LOAD ADDRESS / ACTUAL LOAD ADDRESS / STATUS: KERNEL LOAD ADDRESS MISMATCH`.
- Test-strażnik: `tests/test_kernel_load_addr.py` (4 asercje: macro, linker, kernel_entry, loader).
- **Dowód Hatari:** `docs/screenshots/v10-phase1-kernel-load.png`
  (`LOAD: 00080000  ENTRY: 00080020  SIZE: 122778  STATUS: OK`).

### §4 Phase 2 — detekcja maszyny
- `probe_write8()` (bezpieczny zapis przez wektor bus error) w `boot/exception.S`.
- STE: test zapis/odczyt `0xffff820d` wg EmuTOS (samo czytanie dawało STE na ST/Mega ST).
- Mega ST: **ważny** odczyt RTC (`rtc_mega_present()`), nie sama czytelność adresu
  (Hatari ST dekoduje adres, ale bez zegara — dawał fałszywe Mega ST).
- Czysta decyzja `machine_classify()` w `include/machine_classify.h`.
- Host-test `tests/test_machine_detect.c` (`make check-machine` → `MACHINE_DETECT: ALL PASS`).
- POST pokazuje MACHINE / CPU / CLOCK / CACHE / SHIFTER / RAM / ALT-RAM / BLITTER.
- **Dowody Hatari:** `docs/screenshots/v10-phase2-machine-{st,megast,ste,megaste}.png`
  → ST→ATARI ST, MegaST→ATARI MEGA ST, STE→ATARI STE, MegaSTE→ATARI MEGA STE (8 MHz).

### §15 Zamrożenie AES/VDI
- `system/tos_vdi.c` i sekcja AES w `system/tos_prg.c` oznaczone **LEGACY / EXPERIMENTAL**
  (zakaz nowych opkodów/menu/dialogów/fontów).

### §16/§17 Szkielet testów i zachowanie referencyjne
- `tests/{bios,xbios,gemdos,vdi,aes,prg,video,memory}/` + README z planem testów.
- `docs/REFERENCE-BEHAVIOUR.md` (TOS 2.06 / EmuTOS / ATOS, zasady).

## Komendy weryfikacji

    cd commander-v10
    gmake -j4 all          # EXIT 0, KERNEL.SYS = 122778 B
    gmake check-machine    # MACHINE_DETECT: ALL PASS
    python3 -m unittest discover -s tests -p "test_kernel_load_addr.py" -v

Test w Hatari (obraz `work/v10-boot-test.img` = kopia `work.img` z nowym BOOT/KERNEL):

    SDL_VIDEODRIVER=dummy hatari --machine megaste --monitor mono --cpulevel 0 \
      --cpuclock 16 --memsize 4 --tos build/bios_mste.bin --patch-tos false \
      --acsi 0=work/v10-boot-test.img --cmd-fifo /tmp/fifo

## Znane problemy (wcześniejsze, nie wprowadzone przez v10)
- `tests/test_sysinfo_api.py` i `tests/test_user_sleep_wakeup.py` padają również w v9
  (dryf testów hostowych: brak pola `terminal` w `user_process_t`, API sysinfo).


## Phase 4 — storage (stan: kod gotowy, weryfikacja IDE do dokończenia)

- `include/block_device.h` + `hw/block_device.c`: wspólna warstwa ACSI/IDE
  (block_probe/read/write/flush/get_info), id: 0x00-0x07 ACSI, 0x80-0x81 IDE.
- `hw/ide.c` + `include/ide.h`: przeniesiony, zaudytowany sterownik ATA LBA28
  (detekcja, model, pojemność, read/write) — z wcześniejszej bazy (nie przepisany).
- `hw/acsi.c`: `acsi_read_sector`/`acsi_write_sector` kierują id z bitem 7 do IDE,
  dzięki czemu loader/FAT/jądro nie znają transportu; `acsi_inspect_device` obsługuje IDE.
- `loader/bootstage.c`: skan bootowania rozszerzony z ACSI 0-7 na IDE 0-1 (`block_probe`).
- `hw/expansion.c`: `ide_detect()`; `system.h`: `ide_units`; F3 pokazuje IDE.
- Build zielony; **ACSI boot nadal daje nowy KERNEL.SYS (LOAD 00080000, STATUS OK) — zweryfikowane.**

**Otwarte:** test „tylko IDE" w Hatari jest **nierozstrzygający** — mimo `--ide-master`
(bez `--acsi`, z `--configfile`) BIOS raportuje `BOOT DEVICE: ACSI 0` i czyta starszy
KERNEL.SYS (0x40000, 62189 B), a ten sam obraz przez `--acsi` daje nowy (122778 B).
To wskazuje, że Hatari podłącza w tle urządzenie ACSI; trzeba wymusić czysty, IDE-only
start (osobny config/Hatari bez zapisanej historii dysków) i wtedy potwierdzić
`FOUND IDE UNIT 0`. Zwróć uwagę: diagnostyka Phase 1 zadziałała poprawnie także tutaj —
odrzuciła niezgodny obraz z czytelnym `KERNEL LOAD ADDRESS MISMATCH`.

## Phase 3 — trwała konfiguracja (zrobione)

- `include/config.h`: `bios_config_t` (128 B, magic `ASFC`, wersja 2, CRC),
  pola: boot_device, video_driver, video_mode, safe_mode, debug_level, language,
  blitter, boot_delay, boot_valid/boot_id/boot_part. Czyste `cfg_defaults`,
  `cfg_crc`, `cfg_validate` (inline, host-testowalne).
- `hw/config.c`: odczyt/zapis FAT16 `\BIOS\BIOS.CFG` (fallback `\BIOS.CFG`) przez
  `acsi_read/write_sector` (czyli także IDE), z `block_probe`; buduje katalog BIOS
  i alokuje klaster w razie potrzeby.
- `main.c`: `cfg_load()` przy starcie (błąd → bezpieczne domyślne), użycie
  `boot_delay`, `video_driver`, `boot_valid` oraz kod języka z konfiguracji.
- Test: `make check-config` → `CONFIG: ALL PASS`; `check-lang` i `check-machine` zielone.
- **Dowód Hatari:** brak pliku konfiguracji → domyślne, boot działa:
  `docs/screenshots/v10-phase3-config-defaults.png`.

Uwaga: zapis na ACSI zwraca 0 (ACSI read-only); `cfg_save()` działa na IDE.

## Phase 5 — diagnostyka wyjątków (zrobione: rdzeń)

- `include/exception_format.h`: czysty, CPU-świadomy dekoder ramki — rozróżnia
  68000 short od format-word 68010+, wyciąga format/vector/SR/PC/fault
  (fault address z długich ramek bus/address: format 1/2/7).
- `ui/exception_report.c`: ekran `ATOS SYSTEM EXCEPTION` z
  CPU / VECTOR / FORMAT / PC / SR / SP / FAULT.
- `boot/exception.S`: `default_exception` woła raport zamiast pętli nieskończonej.
- Test: `make check-exception` → `EXCEPTION: ALL PASS`; build `BUILD=0`.
- Pozostaje: fatalny Bus/Address **poza sondami** sprzętu (obecnie `bus_error`
  trzyma recovery dla prób) — dopiąć flagę „armed” i raport; oraz wywołać ten sam
  raport z jądra (`kernel_exception.c`).

## Phase 6 — centralna warstwa IRQ (zrobione: rdzeń)

- `include/irq.h` + `system/irq.c`: jedno miejsce rejestracji handlerów dla
  **VBL / HBL / MFP Timer A-D / ACIA**; ograniczone (4 sloty na źródło), bez
  alokacji, bezpieczne do wołania z przerwania; API:
  `irq_register/unregister/dispatch/reset`, liczniki, `irq_source_name`.
- Test: `make check-irq` → `IRQ: ALL PASS` (rejestracja, duplikaty, zły source,
  null handler, kolejność/wywołania, przekazanie kontekstu, przepełnienie slotów,
  reset).
- Pozostaje: podpiąć `irq_dispatch` do realnych wektorów w `kernel_entry.S`
  (VBL/HBL/MFP/ACIA) i zdjąć globalne maskowanie tam, gdzie to bezpieczne.
- Phase 5 — **domknięte**: flaga `bus_probe_armed`; sondy `probe_*` nadal mają recovery, a prawdziwy Bus/Address trafia do `ATOS SYSTEM EXCEPTION` (dowód: boot bez regresji — `docs/screenshots/v10-phase5-bus-address.png`).

## Phase 7/11 — video_driver_t (zrobione: kontrakt + backend software)

- `include/video_driver.h`: `video_format_t` (PLANAR 1/2/4, **CHUNKY 8**),
  `video_surface_t` (width/height/pitch/format/pixels), `video_driver_t`
  (name, init/shutdown, set_mode, get_surface, set_palette, fill_rect, draw_line,
  blit, show_cursor/hide_cursor, vsync). VDI/AES/Desktop mają używać **wyłącznie**
  tego interfejsu — zakaz `if(et4000)` poza backendem.
- `system/video_driver.c`: rejestr + selektor + backend **"software"**
  (CHUNKY 8bpp: clipping, Bresenham, blit chunky/planar) + wspólny `video_put_pixel`.
- Test: `make check-video-driver` → `VIDEO_DRIVER: ALL PASS`.
- Pozostaje: adaptery `video/shifter.c` (PLANAR) i `video/et4000.c` (CHUNKY,
  1024x768x8) na istniejącym kodzie; wpięcie kompozytora/VDI w ten interfejs
  i usunięcie `wm_app_flip` z nowej ścieżki.

## Phase 20 — cpu_ops + CPU030.md (zrobione: rdzeń)

- `include/cpu_ops.h` + `system/cpu_ops.c`: warstwa możliwości per rodzina CPU
  (VBR 68010+, CACR 68020+, data cache/PMMU 68030+, FPU on-chip 68040+),
  `cpu_ops_for(cpu_type)` + czysta `cpu_ops_family_for`. MOVEC/PMMU pozostają
  w sprawdzonym asm (`boot/exception.S`); nic tu nie włącza MMU.
- Test: `make check-cpu-ops` → `CPU_OPS: ALL PASS`.
- `docs/CPU030.md`: VBR, CACR, cache, **PMMU (TC/CRP/SRP, transparent translation)**,
  formaty ramek wyjątków i kolejność włączania (dopiero po V10-A).

## Phase 7/10 — backendy video (zrobione)

- `video/shifter.c`: `video_driver_t` "shifter", **PLANAR_1BPP**, własny surface.
- `video/et4000.c`: `video_driver_t` "et4000", **CHUNKY_8BPP**, domyślny
  **1024x768x8**, oraz `et4000_bind_hw()` na realne rejestry z
  `system/vofa_et4000.c` — backend nie zna sprzętu.
- Test: `make check-video-backends` → `VIDEO_BACKENDS: ALL PASS`
  (fill/linia/blit, format i rozmiar 1024x768, wywołanie hooka, blit planar→chunky).
- Pozostaje: wpięcie kompozytora/VDI w `video_driver_t` i usunięcie
  `wm_app_flip` z nowej ścieżki renderowania.

## Czysty obraz v10 (zrobione)

- `work/atos-v10.img` = baza `work.img` + **v10** `BOOT.SYS` (790 B) i `KERNEL.SYS`
  (122778 B); tworzony przez `gmake image-v10`.
- `run-v10.sh` + `gmake hatari-v10` uruchamiają Hatari z BIOS-em v10 i tym obrazem.
- Uwaga: Uruchomienie pokazuje **stary pulpit/AES (LEGACY)** — v10 KERNEL.SYS jest
  na razie identyczny z v9; debug-harness w `kernel_main.c:294` odpala SysInfo.
  To zmieni dopiero wpięcie nowych warstw (punkt 3).

## Punkt 3 / krok 1 — usunięty debug auto-start SysInfo (zrobione)

- `system/kernel_main.c`: harness nie odpala już `\SYSTEM\TEST\SYSINFO.PRG`
  (zamienione na `NOSYS.PRG` + komunikat). To było źródło widocznego „zawieszenia".
- Build `BUILD=0`, obraz `gmake image-v10` przebudowany.
- Dowód: `docs/screenshots/v10-no-autostart.png` — pulpit mono bez SysInfo.
- Kolejne kroki punktu 3: wpięcie `video_driver_t` w kompozytor + usunięcie
  `wm_app_flip` z nowej ścieżki; wpięcie `irq_dispatch` w wektory; decyzja
  AES/VDI + `docs/EMUTOS-INTEGRATION.md`.

## Punkt 3 / krok 2 — wm_app_flip usunięty z nowej ścieżki (zrobione)

- `system/kernel_wm.c`: `wm_app_flip = 0`; `glue_blit` **zawsze** `wm_copy_rect`
  (jedna konwencja `(0,0)=lewy górny`), bez lustrzenia/inwersji; LEGACY
  `wm_copy_rect_flip` zachowany jako nieużywany (`__attribute__((unused))`).
- Build `BUILD=0`. Uwaga: jeśli wcześniejszy flip maskował odwróconą orientację,
  objaw może wrócić — to celowe; właściwa poprawka to konwencja w źródle rysowania
  (font/surface), a nie łata w kompozytorze.
- Kolejne: podłączenie `video_surface_t`/`video_driver_t` jako źródła
  framebuffera kompozytora (zamiast `kgfx_*`), wpięcie `irq_dispatch`,
  `docs/EMUTOS-INTEGRATION.md`.

## Punkt 3 / krok 3 — video_driver_t wpięty w kompozytor (zrobione)

- `include/video_driver.h` + `system/video_driver.c`: `video_screen_attach()` /
  `video_screen_surface()`.
- `system/kernel_wm.c`: `glue_blit` i `kernel_wm_frame_tick` biorą framebuffer
  przez `wm_get_fb()`, które publikuje `kgfx_framebuffer` do `video_screen_*`
  i czyta go z powrotem przez abstrakcję (nie przez `kgfx_*` bezpośrednio).
- `Makefile`: `system/video_driver.o` dolinkowany do **KERNEL.SYS**.
- Build `BUILD=0`; boot bez regresji (`docs/screenshots/v10-video-wired.png`).
- Kolejne: `irq_dispatch` → wektory; `docs/EMUTOS-INTEGRATION.md`.

## Punkt 3 / krok 4 — irq_dispatch wpięty w przerwania (zrobione)

- `system/kernel_entry.S`: Timer C woła `irq_dispatch` (`IRQ_SRC_MFP_C=4`),
  ACIA woła `irq_dispatch` (`IRQ_SRC_ACIA=6`); rejestry scratch zachowane.
- `Makefile`: `system/irq.o` dolinkowany do **KERNEL.SYS**.
- Build `BUILD=0`; boot bez regresji (`docs/screenshots/v10-irq-wired.png`).
- Kolejne: HBL (do ustalenia), potem AES/VDI + `docs/EMUTOS-INTEGRATION.md`.

## Punkt 3 / krok 5 — analiza integracji EmuTOS (zrobione)

- `docs/EMUTOS-INTEGRATION.md`: mapowanie modułów EmuTOS (bdos/bios/vdi/aes),
  gdzie wpiąć `video_driver_t`, zależności (tosvars 0x380-0x800, VBL, cookie jar,
  adres 0/VBR), .PRG, oraz warianty A/B/C z konsekwencjami GPL.
- Rekomendacja: przy A+GPL -> **wariant B** (ATOS BIOS/kernel + komponenty EmuTOS).
- Decyzja wariantu należy do Ciebie.

## Wariant B — start integracji (w toku)

- Decyzja: **wariant B** (ATOS BIOS/kernel/video_driver_t + EmuTOS GEMDOS/VDI/AES).
- Szkielet: `compat/README.md`; licencje już w `licenses/`.
- Krok 1: build vendored EmuTOS 256k z `work/emutos-reference` (job w tle).

## Wariant B — build EmuTOS: BLOKER toolchain

- `make -j4 TOOLCHAIN_PREFIX=m68k-elf- 256` w `work/emutos-reference` **failuje**:
  `bios/bios.c` → asembler `can't resolve 0 - sp`. EmuTOS zakłada toolchain
  **m68k-atari-mint-** (profil `-mshort`), a w środowisku jest tylko `m68k-elf-`.
- Opcje:
  1. zainstalować `m68k-atari-mint-*` i powtórzyć build,
  2. **użyć gotowego ROM EmuTOS z repo** (`atari-030-port/disk/emutos-pl-256k.img`)
     i wystartować go pod ATOS BIOS — najszybszy krok do działającego GEM,
  3. patchować źródła EmuTOS pod `m68k-elf-` (kosztowne, ryzykowne).
- Rekomendacja: **2** natychmiast (pokaże GEM na naszym BIOS-ie), **1** równolegle.

## Wariant B / opcja 2 — EmuTOS pod ATOS BIOS (ustalenie)

- ATOS loader (`loader/tos.c:78`) uznaje za `loadable` **tylko obrazy RAM-TOS**
  (adres linkowania `0x800..0x80000`). Gotowe `emutos-*-256k.img` mają
  `rom_address=0x00E00000`, więc `loadable=0`; `ui/tos.c` odmawia komunikatem
  **"ROM IMAGE NEEDS MMU OR ROM BANK"**.
- EmuTOS 1.4.0 sam startuje (referencja): `docs/screenshots/emutos-desktop-reference.png`.
- Realne drogi do „EmuTOS pod ATOS BIOS":
  1. **RAM-TOS EmuTOS** — wymaga build z toolchainem `m68k-atari-mint-` (bloker),
  2. **przywrócić ścieżkę MMU** z `files-pasted` (`boot/mmu030.S` + `boot/mmu_boot.c`)
     i bootować 256k ROM na **68030** z `--mmu on` (nasz BIOS robi handoff).
- Rekomendacja: **2**.

## Wariant B / opcja 2 — ścieżka MMU przeniesiona (build OK, test niepełny)

- Przeniesione z `files-pasted`: `boot/mmu030.S` (budowany `-m68030`:
  pmove CRP/TC) + `boot/mmu_boot.c` (mapuje ALT-RAM pod 0x00E00000, tablice
  0x3F0000/0x3F1000). Prototyp `mmu_boot_rom()` w `tos_loader.h`;
  `ui/tos.c` próbuje MMU dla obrazów ROM na 68030.
- Build `BUILD=0`; Hatari rozpoznaje 68030+MMU (`16MHz/030 … /MMU`).
- **Blokery weryfikacji:**
  1. `install_boot_sys.py` odmawia instalacji do root `TOS_BIOS`
     ("destination directory must be SYSTEM or a subdirectory of SYSTEM"),
     więc obraz EmuTOS nie trafił tam, gdzie skanuje katalog F5.
  2. Automatyzacja klawiszy nie dotarła do menu F5 (autoboot przeszedł do BOOT.SYS);
     trzeba wcześniej ustawić `boot_valid` (BIOS.CFG) albo poprawić sekwencję.
- Kolejne: umieścić obraz w ścieżce katalogu F5 i wymusić F5; wtedy sprawdzić
  `mmu_boot_rom` (uwaga: w starszej bazie ta ścieżka miała BLOKER — pierwszy
  tłumaczony dostęp faultował).

## Wariant B / opcja 2 — przygotowane; automat F5 nie zadziałał

- Katalog F5 skanuje teraz **SYSTEM** (installer tworzy katalogi tylko pod SYSTEM);
  EmuTOS wgrany jako `SYSTEM/EMUTOS.IMG`; `BIOS.CFG` (boot_valid=1, crc 0x83948C43)
  w root. Build `BUILD=0`.
- Boot `--cpulevel 3 --mmu on`: **jądro wstaje w pełni** — `CORE/VFS/PROC 2 TASKS/
  TIMER 4Q/EVENT/IKBD/SHELL READY` (`docs/screenshots/v10-m2-menu.png`).
- Problem: wstrzykiwane klawisze (F4/F5 przez `hatari-event keypress`) **nie trafiają**
  w BIOS-owy poll IKBD, więc autoboot przeszedł do BOOT.SYS i menu F5 się nie otworzyło;
  `mmu_boot_rom` nie został wywołany.
- Fix: flaga w `BIOS.CFG` „auto-load first TOS image" (bez interakcji) albo inna
  metoda wstrzykiwania klawiszy. Przypomnienie: w starszej bazie ścieżka MMU miała
  zanotowany bloker (pierwszy tłumaczony dostęp faultował).

## auto_tos + headless MMU boot (kod gotowy; wynik niepewny)

- Dodane pole `auto_tos` w `bios_config_t` (offset 26); `main.c` po `cfg_load`
  skanuje katalog i dla kompatybilnego ROM na 68030 woła `mmu_boot_rom` — **bez klawiszy**.
- Obraz: `SYSTEM/EMUTOS.IMG` + `BIOS.CFG` (auto_tos=1, boot_valid=1, crc 0x83948C63).
- Boot `--cpulevel 3 --mmu on --ttram 64`: **brak widocznego EmuTOS** i brak
  crash-screenu → `mmu_boot_rom` albo zwraca 0, albo faultuje/hang. Zgadza się z
  blokerem MMU zanotowanym w starszej bazie.
- Następny krok: diagnoza `mmu_boot_rom` (log reason z handlera / Hatari `--trace`)
  albo **droga 1** (toolchain `m68k-atari-mint-` → RAM-TOS EmuTOS).

## Droga 1 (toolchain mint → RAM-TOS EmuTOS) — BLOKER środowiska

- `m68k-atari-mint-*` **nie jest zainstalowany**; Homebrew oferuje tylko `m68k-elf-*`
  (zainstalowane). Brak vendored toolchainu w repo.
- Build EmuTOS z `m68k-elf-gcc 16.2` **failuje** w `bios/bios.c`:
  assembler `can't resolve 0 - sp` — zarówno target `192`, jak i `256`.
  To niezgodność codegenu, nie brakujący plik.
- EmuTOS ma target **RAM-TOS**: `make prg` / `prg256` → `emutos*.prg`.
- Dokończenie drogi 1 wymaga:
  1. instalacji `m68k-atari-mint-*` (prebuilt z freemint lub build),
  2. `make TOOLCHAIN_PREFIX=m68k-atari-mint- prg`,
  3. wgrania `emutos*.prg` do `SYSTEM` — ATOS loader załaduje go **wprost**
     (adres <0x80000, bez MMU).
- Alternatywa bez mint: patch `bios/bios.c` pod `m68k-elf` (niepewne).

## MILESTONE — EmuTOS RAM-TOS działa pod ATOS BIOS (droga 1 bez mint!)

- `emu256pl.prg` (RAM-TOS z `emutos-prg256-1.4`) wgrany do `SYSTEM`; loader
  wyłuskał osadzony obraz (`loadable=1`) i `tos_load_once` uruchomił go **bez MMU**.
- `auto_tos=1` w `BIOS.CFG`; `main.c` automatycznie skanuje katalog i bootuje.
- **Dowód:** `docs/screenshots/v10-prg-2.png` — ekran EmuTOS 1.4
  (Wersja 1.4, M68000, Atari Mega STe, 4 MB); `v10-prg-3.png` — biały pulpit GEM mono.
- To osiąga cel **„EmuTOS/GEM pod naszym BIOS-em"** bez toolchainu mint. Nasz BIOS
  robi handoff (loader RAM-TOS), a EmuTOS przejmuje GEM.
- Uwaga: to nadal EmuTOS (jego AES/VDI/desktop), nie nasz LEGACY GUI. Kolejny krok to
  integracja wg wariantu B (podpięcie rysowania EmuTOS pod `video_driver_t`).

## Do zrobienia (kolejne sesje)
- **Phase 3** trwała konfiguracja `\BIOS\BIOS.CFG` (wersja+suma, wartości domyślne).
- **Phase 4** `block_device_t` (ACSI + przywrócone IDE; SCSI później).
- **Phase 5** diagnostyka wyjątków (formaty ramek 68030, crash screen).
- **Phase 6** centralna warstwa IRQ (VBL/HBL/MFP/ACIA), bez globalnego maskowania.
- **Phase 7-13** `video_driver_t` + `video_surface_t` + backendy Shifter/ET4000,
  usunięcie `wm_app_flip` z nowej ścieżki, własność framebuffera.
- **Phase 18** `docs/EMUTOS-INTEGRATION.md`; **Phase 20** `cpu_ops` + `docs/CPU030.md`.
- **Milestone V10-A** (checklista), V10-B (testy na TOS/EmuTOS/ATOS), V10-C (decyzja).
