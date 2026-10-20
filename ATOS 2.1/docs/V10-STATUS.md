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

Uwaga: zapis na ACSI działa od planu item 1 (2026-09-26, WRITE(6) po DMA);
`cfg_save()` i `block_write` utrwalają dane na ACSI i IDE.

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

## Ciągły log startu + brak mignięcia przy przejęciu obrazu (2026-09-26)

- **Problem:** start kasował tekst trzy razy (ROM `console_clear` w BOOT.SYS,
  `kernel_main` `h->console_clear()`, `kconsole_clear()`) i przełączał
  Shifter→ET4000 w połowie logu, co dawało serię „kasowanie i rysowanie na nowo"
  oraz ~1,5 s czerni przy przejęciu obrazu.
- **Ciągły log:** konsola ROM (`ui/console.c`) prowadzi lustro znaków 80x25
  z kursorem i eksportuje `console_text_copy`; `boot_handoff_t` v5 (58 B)
  przekazuje je dalej. BOOT.SYS (`system/boot_main.c`) i jądro **nie czyszczą**,
  a `kconsole_seed()` przejmuje lustro, więc pierwszy render maluje dokładnie te
  same znaki. Fonty ROM-u i jądra są identyczne (`ui/font.c` 8x16 vs
  `emutos_system_font` — sprawdzone glify 'A'/'B'), więc przemalowanie jest
  niewidoczne.
- **Brak mignięcia:** kasowanie VRAM przeniesione **przed** zaprogramowanie trybu
  CRTC (wtedy jeszcze widać Shifter), a inicjalizator ET4000 woła z powrotem
  kernel (`vofa_io_t.render`) już po włączeniu skanowania, ale **przed** wolnym
  `count_frames()`; kernel (`kernel_video_boot.c: video_render_log`) maluje cały
  log wprost do VRAM karty, więc przejęcie obrazu pokazuje gotowy tekst.
  Przy nieudanej inicjalizacji `kvideo_deactivate_vofa()` wraca na Shifter.
- **Dowód:** pomiar klatka po klatce (0,4 s): wcześniej klatka 2440 B (czarna)
  w okolicy przełączenia, teraz brak; finalny ekran poprawny
  (`VIDEO: VOFA REQUESTED / 640x480 MONO / VIEW 640x400`).
- **ABI:** kernel↔VOFA.SYS zmienione razem (`vofa_io_t` z polem `render`);
  `--install` wgrywa oba.

## MILESTONE — inicjalizacja ET4000 w ROM-ie (2026-09-26)

Karta wstaje w BIOS-ie, **przed pierwszym komunikatem**, więc cały log startu
(POST, BOOT.SYS, jądro, shell) leci na VGA bez żadnego przełączania Shifter→karta.

- `hw/vofa_rom.c`: `vofa_rom_init()` — ta sama sekwencja rejestrów co
  `system/vofa_et4000.c` (ten sam plik linkowany też do ROM-u jako
  `build/hw/vofa_et4000_rom.o`), odczyt/zapis przez `probe_read8`/`probe_write8`
  (łapanie bus-errorów, `boot/exception.S`).
- Ticki: MFP timer D — liczone są **zawinięcia** (246→0 = 50 Hz), nie każda zmiana
  `TCDR` (12,3 kHz). Pierwsza wersja dawała okno ~1 ms i `VOFA_CLOCK_ERROR`.
- Konsola ROM (`ui/console.c`) pisze do wymiennej bazy (`console_set_screen`);
  `main.c` przestawia ją na `VOFA_MEM` po udanej inicjalizacji (układ bajtów VRAM
  karty jest identyczny z ramką Shiftera mono, więc kod konsoli bez zmian).
- Handoff v6 (60 B): `nova_initialized`. Jądro **adoptuje** kartę
  (`kvideo_activate_vofa`) i nie powtarza inicjalizacji — `VIDEO: VOFA (ROM INIT)`;
  `VOFA.SYS` zostaje jako ścieżka dla obrazów ze starym BIOS-em.
- Brama: `nova_present && adapter==VOFA && ram_bytes<VOFA_MEM &&
  video_driver!=SHIFTER`, wykonywana tuż przed `post_run()` — konsola przed
  `shifter_init()` jest niewidoczna (sprawdzone empirycznie).
- **Dowód:** POST (F1-F10) na karcie od ~3 s, zero czarnych klatek (pomiar 0,4 s),
  finał `VIDEO: VOFA (ROM INIT) / 640x480 MONO / VIEW 640x400`; fallback Shifter:
  `VIDEO: VOFA REQUESTED / NOT FOUND - SHIFTER`, shell i `hw`/TAB działają.
- **Kopia `.data` w starcie ROM** (`boot/reset.S` + symbole `__data_start`,
  `__data_end`, `__data_load_start` w `linker/mste-rom.ld` i `linker/st-rom.ld`):
  wcześniej startup tylko zerował BSS, więc każdy inicjalizowany static cicho
  czytał 0. `console_screen` wrócił do normalnego inicjalizatora — i to jest
  zarazem test tej ścieżki (boot bez bus-errorów na adresie 0).
- **`autoboot_countdown()` naprawiony**: tick = zawinięcie `TCDR`, nie każda zmiana.
  Z `boot_delay=4` menu startowe naprawdę czeka ~4 s, więc **F1–F10 są teraz
  osiągalne przy starcie** (wcześniej countdown mijał natychmiast i BIOS bootował
  od razu). Szybciej: `boot_delay=0` w SETUP / `BIOS.CFG`.
- **EPROM dla TF536 ST Edition:** `build/atos-bios-tf536-st-dual.bin` = 512 KiB,
  dwa banki po 256 KiB — bank1 = wariant ST (0xfc0000, 192 KiB, nagłówek 0x0104),
  bank2 = wariant MSTE/TT (0xe00000, 256 KiB, 0x0206). Buduje
  `tools/mk_dualrom.py --bank-size 262144`; zworka karty wybiera bank.
  Oba banki sprawdzone w Hatari: bank1 w trybie ST (POST + skan ACSI),
  bank2 w trybie MSTE+TF536 (pełny boot na VGA).
- **Failsafe wideo (dwie ścieżki).** (a) Szybka: `Alt` wciśnięty **przed**
  decyzją o karcie → karta w ogóle nie wstaje (timingowo wrażliwe: ACIA/IKBD muszą
  już odebrać bajt; na realnym sprzęcie ACIA jest gotowa od pierwszej instrukcji
  ROM-u, więc trzymanie `Alt` od włączenia powinno trafić). (b) **Niezawodna:**
  `Alt` wciśnięty podczas 4-sekundowego menu startowego → BIOS przestawia konsolę
  z powrotem na `SCREEN_BASE`, ustawia `video_force_shifter` i **kontynuuje
  bootowanie** (`boot_stage2_run()`), a jądro nie aktywuje VOFA. Na realnym
  sprzęcie widać to na monitorze ST (Shifter zawsze steruje swoim portem).
  Zweryfikowane w Hatari: `runtime_handoff.video_force_shifter == 1`.
- **Retry bootowania (zrobione 2026-09-26).** `loader/bootstage.c`: skanowanie
  wyciągnięte do `boot_scan_device()`; gdy nic nie znajdzie, BIOS pokazuje
  `NO BOOT DEVICE - RETRY? (SPACE)`, a **spacja** ponawia skanowanie
  (`RESCANNING...`), Esc/Return kończy. Zweryfikowane bez dysków w Hatari
  (PROBING ACSI 0..7 + IDE 0..1 → prompt → spacja → ponowny skan → prompt).
- **Konfiguracja `BIOS.CFG` — NAPRAWIONE (2026-09-26).** Trzy niezależne
  przyczyny, wszystkie usunięte i zweryfikowane:
  1. **Config się nie wczytywał.** `cfg_load()` woła `cfg_find_boot()`, który
     sonduje urządzenia **bardzo wcześnie** (zaraz po `expansions_detect`) i tam
     nie widzi jeszcze dysku (`g_config_error == 1`). Ponowienie `cfg_load()` po
     `shifter_init()` (z ponownym zastosowaniem ustawień) daje `error 0`,
     `loaded 1` — **2/2 przebiegi**.
  2. **Brak fallbacku na `\BIOS.CFG`.** Stary, ucięty plik `\BIOS\BIOS.CFG`
     (**64 B**) przesłaniał poprawny `\BIOS.CFG` (128 B, w katalogu głównym);
     `cfg_load` nie próbował drugiej lokalizacji. Dodany `cfg_read_valid()` +
     fallback na korzeń.
  3. **SETUP nic nie zapisywał.** `ui/setup.c` przenosił edycje tylko do
     `g_system` i **nigdy nie wołał `cfg_save()`**. Dodane: zapis do `g_config`
     (video, blitter) + `cfg_save()`. Do tego `cfg_file_write()` **aktualizuje
     teraz rozmiar we wpisie katalogowym**, gdy plik rośnie (wcześniej rosnący
     plik czytał się ze starą długością).
  **Dowód:** hash obrazu po „F10 SAVE & EXIT" zmienia się (`6be5aa06…` →
  `bc760823…`), a przy kolejnym starcie `g_config_error == 0`,
  `g_config_loaded == 1`. **Wniosek z wcześniejszych obserwacji był błędny**:
  BIOS *potrafi* pisać po IDE — brakowało wywołania, nie sprzętu.
  To odblokowuje SETUP, język/`boot_delay`/preferencję wideo, `auto_tos` (EmuTOS)
  oraz projektowany failover wideo.
  **Uwaga metodyczna:** syntetyczny `press` w Hatari wysyła make+break, a ACIA ma
  **1-bajtowy bufor** — czytający przez odpytywanie BIOS często widzi tylko kod
  zwolnienia (`0x100|scan`). Dlatego testy klawiszy trzeba robić osobno
  `keydown` … `keyup` z odstępem; to samo wyjaśnia wcześniejszą „wrażliwość
  czasową" failsafe `Alt`.
- **Failsafe wideo: przytrzymaj `Alt` od włączenia.** Konsola ROM zostaje wtedy na
  Shifterze, a karta jest pomijana (`vofa_rom_status=0x101`); do jądra idzie
  `video_force_shifter` w handoffie, więc ono też nie podnosi ET i wypisuje
  `VIDEO: SHIFTER (FORCED - ALT OR SETUP)`. To droga powrotu, gdy karta nie da
  obrazu na tej maszynie. Uwaga wdrożeniowa: ACIA musi być wstępnie
  zainicjalizowana **w `_reset`** (pierwsze instrukcje ROM-u), a `ikbd_init()`
  nie może już być wołane później — jego master reset kasuje bajt klawisza
  (`boot/reset.S`, `main.c`).
- **Ctrl+Alt+Del = warm reset.** `bios_reset()` w `main.c` (instrukcja `reset` +
  powrót do `_reset`), wystawiony w handoffie v7 (64 B, pole `reset`). Jądro
  wykrywa kombinację w `ksessions_key()` (`system/kernel_sessions.c`) i woła ten
  hook — **działa systemowo**, także gdy klawiaturę ma aplikacja; w BIOS-ie obsłużone
  dodatkowo w pętli menu. Sprawdzone w Hatari: 4 s po kombinacji wraca POST
  z menu startowym.

- **Automatyczny failover wideo (zrobione 2026-09-26).** Zapadka
  `video_pending` w `\BIOS\BIOS.CFG`: BIOS zapisuje ją (1) tuż przed
  przeniesieniem konsoli na kartę, więc jeśli loader/jądro zawiesi się na tej
  ścieżce, następny start widzi 1 i wymusza Shifter
  (`VIDEO: SHIFTER (CARD DISABLED AFTER FAILED BOOT)`), dopóki użytkownik nie
  naciśnie `Alt` (kasuje zapadkę i próbuje karty ponownie). Udany start na
  karcie kasuje zapadkę — ale **nie z jądra**: na IDE bez modułu `ACSI.SYS`
  jądro nie ma ścieżki zapisu (rejestry DMA ACSI nie sterują IDE TF536;
  `kfat_save_path` zwracał `-FAT_WRITE_IO`). Dlatego handoff v8 (68 B)
  niesie nowy callback `video_boot_ok`; jądro, gdy dojdzie do shella na
  karcie, woła `bios_video_boot_ok()`, a ROM kasuje zapadkę swoją
  sprawdzoną ścieżką zapisu. Komunikat failoveru rysowany jest **po**
  `post_run()` (wiersz 11), bo tamto czyści ekran. Test
  `scenario_failover.py`: A) dwa starty z rzędu = karta,
  `nova_initialized=1`, zapadka na dysku = 0; B) zapadka 1 =
  `video_force_shifter=1`, `nova_initialized=0`, w logu ROM komunikat.
  **Wszystkie asercje PASS.**
- **`make_bios_cfg.py` — NAPRAWIONE (2026-09-26).** Generator ustawiał
  `boot_valid=1` z `boot_id=0`, czyli wskazywał nieistniejące ACSI 0,
  mimo że dysk jest na IDE — BIOS kończył na `BOOT DISK READ ERROR`, a
  jądro w ogóle nie startowało (to maskowało testy failoveru). Teraz domyślnie
  `boot_valid=0` (auto-skan); `--boot-id/--boot-part` tylko gdy jawnie
  podane. Dodatkowo `build_atos2.py --install` zapisuje kanoniczny
  `\BIOS\BIOS.CFG` (nie tylko korzeń), więc świeży obraz nie jest przesłaniany
  przez stary plik.

- **Linia komend w Commanderze (jak w Norton Commander, zrobione 2026-09-26).**
  Na dole (wiersz 22, dawniej podpowiedzi myszy) jest `CMD>`; wpisany tekst po
  Enter idzie do interpretera shella. Plumbing: `kernel_shell_service_t` v4 +
  `submit(line)`/`run_pending(void)` w SHELL.SYS (submit tylko kolejkuje, wykonanie
  w kontekście shella), syscall 61/62, przechwycenie `kconsole_puts` do bufora
  (`kconsole_capture_begin/end/state`). Wynik trafia do wbudowanego podglądu
  (`View: COMMAND`), zamykany F3. **Pułapka:** `run_pending` musi być wołane
  także w `kernel_user_yield`, bo `pause_tick()` aplikacji (pętla oczekiwania)
  nie wraca do pętli głównej — inaczej zakleszczenie. Test
  `scenario_cmdline.py` w Hatari: `dir`, `memory`, `type` wykonane, wynik w
  podglądzie.
- **Nakładka sesji i kursor na karcie (zrobione 2026-09-26).** W trybie tekstowym
  konsola na ET4000 renderuje wprost do VRAM karty, a nakładka `[1] v` i kursor
  rysowały do `desktop_framebuffer`, którego `kvideo_flush()` nie skanuje —
  znacznik znikał (na Shifterze był widoczny). `system_scanout()` =
  `kvideo_front()` w `kernel_graphics.c` dla nakładki i kursora.

- **Zapis z jądra na IDE — NAPRAWIONE (2026-09-26).** Handoff v9 (72 B) niesie
  `acsi_write_sector` ROM-u. `kblock_write` dla IDE (id & 0x80) stage'uje transfer
  w wyrównanym `dma_buffer` i woła BIOS-owy `ide_write_sectors`, zamiast
  rejestrów DMA ACSI (te nie sterują IDE TF536). Weryfikacja: `mkdir` z
  Commandera (`gui_call(50)` → `kfat_mkdir_path`) tworzy trwały katalog
  `IDEWRITE` (host: `tools/img_dir.py`). To odblokowuje zapis plików z
  aplikacji na TF536.
- **`cd` z linii komend przestawia aktywny panel.** `command_cd()` w
  `commander.c`: `cd \X`, `cd ..`, `cd X` waliduje katalog
  (`gui_call(5)`) i ustawia ścieżkę panelu; shell i tak wykonuje komendę,
  więc oba są spójne. Test `scenario_next.py`: panel `\SYSTEM` z jego
  zawartością.
- **Boot: brak zapamiętanego urządzenia nie kończy bootu.** `boot_stage2_run()`
  przy `boot_selection_valid` i niedostępnym urządzeniu pokazuje
  `REMEMBERED BOOT DEVICE UNAVAILABLE`, czyści wybór i wraca do skanu z
  `NO BOOT DEVICE - RETRY? (SPACE)`; zamiast `BOOT DISK READ ERROR` system
  wstaje z IDE (test: `scenario_next.py`).

- **Pole `auto_tos` w BIOS SETUP (2026-09-26).** `ui/setup.c` ma nowy wiersz
  `AUTO TOS  OFF / EMUTOS FIRST`; na ENTER zapisuje `g_config.auto_tos` +
  `cfg_save()`. Test: F1 → pole → Up → Enter → na dysku `auto_tos 1`.
  **Ograniczenie:** `auto_tos` w `main.c` działa tylko przy zapamiętanym
  urządzeniu rozruchowym (`boot_valid`), bo katalog TOS skanowany jest po
  `boot_acsi_id`/`boot_partition`; samo F4 BOOT z menu nie zapisuje wyboru do
  configu (do zrobienia).

- **F4 BOOT: trwały wybór urządzenia + IDE (2026-09-26).** `ui/boot.c` skanuje
  ACSI 0..7 **i IDE 0..1** (`block_probe`), pokazuje `DETECTED IDE0`, a na
  ENTER zapisuje wybór do `g_config` (`boot_valid=1`, `boot_id`,
  `boot_part`) przez `cfg_save()`. To domyka pętlę: `auto_tos` i
  „remembered boot device" mają skąd wziąć urządzenie. Test: F4 → IDE 0 →
  Enter → na dysku `boot_valid=1`, `boot_id=0x80`, `boot_part 0`.

- **Latentny zawis jądra (dwu-fazowy `.APP`) — ZBADANY, uznany za rozwiązany
  (2026-09-26).** Źródło leżało w starym loaderze pierwszego planu, który wracał
  do wołającego na pierwszym kwancie i zwalniał **wciąż żywy** obraz i stos
  (komentarz przy `kernel_spawn_user` w `kernel_module.c`). Obecna droga
  `kernel_spawn_user_async`/`kernel_user_create_loaded` jest atomowa, a `.APP`
  doklejane jest z góry (`shell_start_add_app`). Weryfikacja w Hatari:
  `run NOPE.APP`, `run NOPE`, `start NOPE.APP`, `run NOTES.TXT`,
  `run BIOS.CFG`, `run \SYSTEM\BOOT.SYS` → wszystkie `PROCESS START FAILED`,
  licznik jądra rośnie przez cały czas, `memory` pokazuje stałe wolne (brak
  wycieku). Usuniętej dwufazowej ścieżki nie reanimuję — obecna jest bezpieczna.

- **Porządki w obrazie (2026-09-26).** Aplikacje instalują się już tylko w
  `\SYSTEM\APP` (domyślny PATH jądra:
  `\SYSTEM;\SYSTEM\COMMAND;\SYSTEM\APP;\SYSTEM\TEST`), więc nie ma podwójnych
  kopii (~37 KB). `tools/img_delete.py` (nowe) usuwa wpis i zwalnia łańcuch
  klastrów; z obrazu zniknęły `\SYSTEM\COMMAND.APP`,
  `\SYSTEM\SYSINFO.APP`, `\SYSTEM\HW.APP` oraz testowy `\IDEWRITE`.
  Weryfikacja: `command` i `sysinfo` nadal startują (z `\SYSTEM\APP`).
  Uwaga: narzędzie nie czyści osieroconych wpisów LFN (atrybut 0x0F) — jądro je
  ignoruje, kosmetyczne tylko dla `img_dir.py`.

- **Floppy boot — etap 1 ZROBIONE (2026-09-26).** `hw/fdc.c` + `include/fdc.h`:
  odczyt sektora WD1772 przez DMA (protokół z referencji EmuTOS), selekcja
  stacji/boku na PSG port A (rejestr 14), oczekiwanie na **bit BUSY** FDC
  (GPIP4 bywał zawodny — zwracał 0 dla zastanego przerwania i `fdc_wait`
  wracał przed transferem DMA). Probe w F3 HARDWARE; test w Hatari
  (`--disk-a` z obrazem): `fdc_dbg_sig=0x41544f53` (ATOS), ekran
  `FLOPPY A  READ OK, SIG ATOS`. Dalsze etapy (FAT12, integracja w
  `loader/bootstage.c`, `emutos.prg`) w `docs/FLOPPY-BOOT-PLAN.md`.
- **Floppy boot — etap 2 ZROBIONE (2026-09-26).** `hw/fat12.c` + `include/fat12.h`:
  mount BPB, katalog główny, 12-bitowy łańcuch klastrów, odczyt pliku. Test w
  Hatari: `fat12_dbg_status=1`, `fat12_dbg_sig=0x48454c4c` (HELL z „HELLO FROM
  FLOPPY"), ekran F3 `FAT12  TEST.TXT OK, SIG HELL`.

- **FDD w systemie (2026-09-26).** Handoff v10 (84 B) niesie `floppy_mount`/
  `floppy_dir`/`floppy_read_file` (BIOS, FAT12). Jądro wystawia syscall 63
  (listing katalogu) i 64 (odczyt pliku); Commander ma komendę `fdd` (listing)
  oraz `fdd NAME` (podgląd pliku) — bez FAT12 w jądrze, bo czyta przez callbacki
  ROM-u. Dowód: `fdd-dir.png` (`TEST.TXT 18`), `fdd-file.png` (`HELLO FROM FLOPPY`).
  (Komenda `fdd` w shellu i start systemu z flopa — niżej, oba ZROBIONE.)

- **FDD w shellu i Commanderze (2026-09-26).** API modułu v4 (`floppy_mount/dir/
  read_file`, offsety 176/180/184) + `fdd` w SHELL.SYS: `fdd` listuje katalog,
  `fdd NAME` wypisuje plik (błąd kolejności argumentów `buffer`/`capacity` na
  stosie znaleziony i naprawiony). Commander: `fdd` / `fdd NAME` w viewerze.
  Dowody: `shell-fdd-list.png`, `shell-fdd-file.png` (HELLO FROM FLOPPY),
  `fdd-dir.png`, `fdd-file.png`.

- **Start systemu z FDD (etap 3) — ZROBIONE (2026-09-26).** Gdy skan nie znajdzie
  żadnego ACSI/IDE, BIOS próbuje stacji A: loader ustawia `acsi_part_id[0]="FDD"`,
  `part=0`, `start=0`, a `hw/acsi.c` kieruje `BLOCK_FLOPPY_ID` (0xfe) do
  `fdc_read_sector`, więc `BOOT.SYS`/`KERNEL.SYS`/moduły jadą tą samą ścieżką co
  z dysku. Jądro montuje FAT na flopie fallbackiem `kfat_mount(0u,"FDD")`, a
  `kfat_mount` przyjmuje teraz `start==0` i małe liczby klastrów. Obraz
  rozruchowy (`tools/mk_boot_floppy.py`, 720 KB, `\SYSTEM\` z modułami) ma układ
  16-bitowego FAT z małą liczbą klastrów (loader czyta 16-bit; `hw/fat12.c` to
  osobny 12-bitowy czytnik danych). **Dowód:**
  `docs/screenshots/boot/fdboot-shell.png` — `FLOPPY FAT: MOUNTED`,
  `KERNEL STATUS: RUNNING / SHELL READY`, `STARTUP SERVICES:
  SYSTEM.INIT/GFX/GUI READY`, prompt `tts1 C:\>`; scenariusz
  `atos2-tools/scenario_fdboot.py` (tylko `--disk-a`, bez ACSI/IDE). Boot w Hatari
  trwa ~3 min przez cykl-dokładny WD1772; na sprzęcie to sekundy.

- **Dyskietka rozruchowa z aplikacjami + F4 FLOPPY (2026-09-26).**
  `tools/mk_boot_floppy.py` buduje teraz także `\SYSTEM\APP` z COMMAND.APP,
  SYSINFO.APP i HW.APP (zgodnie z PATH jądra); razem ~166 z 714 klastrów. Na
  flopie `dir`, `dir \SYSTEM\APP` i `run \SYSTEM\APP\HW.APP` działają —
  aplikacja startuje z dyskietki (`PROCESS READY, PID: 4`). F4 BOOT skanuje też
  stację A (`floppy_bootable()` w `ui/boot.c`, id `BLOCK_FLOPPY_ID 0xfe`),
  pokazuje `FLOPPY`/`FDD` i zapisuje wybór (`boot_valid=1, boot_id=0xfe,
  boot_part=0`); loader obsługuje zapamiętany flop (`REMEMBERED BOOT FLOPPY
  UNAVAILABLE`, gdy brak dyskietki). **Uwaga metodyczna:** syntetyczne klawisze
  w Hatari gubią znaki (ACIA), więc wynik `pwd`=`PROCESS START FAILED` był
  artefaktem; przy wolnym wpisywaniu `pwd` drukuje `\`.
  **Dowód:** `docs/screenshots/boot/bootmenu-floppy.png` — `DETECTED FDD /
  DEVICE FLOPPY / BOOT PARTITION P0 FDD START 0`; z podłączonym HDD (IDE) i
  flopem wybór zapisał `boot_valid 1 boot_id 0xfe boot_part 0` w `\BIOS\BIOS.CFG`
  (odczyt hostem). Aplikacje z flopa: `docs/screenshots/boot/fdboot-app-dir.png`
  (`dir \SYSTEM\APP`) i `docs/screenshots/boot/fdboot-app-run.png`
  (`run \SYSTEM\APP\HW.APP` → ekran HW, `[PID 4 EXIT 01]`). **PATH na flopie
  działa**: samo `command` otwiera Commandera z panelem `\SYSTEM\APP`
  (`docs/screenshots/boot/fdboot-app-command.png`) — wcześniejszy
  `PROCESS START FAILED` był gubieniem znaków przez ACIA, nie błędem PATH.

- **FAT12 (12-bit) w loaderze i jądrze — ZROBIONE (2026-09-26).** Dyskietka
  rozruchowa to teraz **prawdziwy FAT12** (714 klastrów), nie układ 16-bitowy.
  `loader/bootstage.c` i `system/kernel_fat.c` wybierają szerokość z liczby
  klastrów (`< 4085` = FAT12), czytają wpisy 12-bit z obsługą wpisu
  przechodzącego przez granicę sektora i używają progów końca łańcucha
  zależnych od szerokości (FAT12 EOC = `0xFF8`, FAT16 = `0xFFF8`; zły klaster
  odpowiednio `0xFF7`/`0xFFF7`). Zapis do FAT12 jest zablokowany
  (`FAT_WRITE_READ_ONLY`). `tools/mk_boot_floppy.py` zapisuje 12-bit FAT
  (`FAT[0]=0xFF8`, `FAT[1]=0xFFF`, typ `FAT12`); ten sam obraz czyta ROM-owy
  `hw/fat12.c` (`fdd`). **Dowód:** `docs/screenshots/boot/fat12-shell.png`
  (loader+jądro z FAT12 do `KERNEL STATUS: SHELL READY`),
  `docs/screenshots/boot/fat12-dir.png` (`dir` → `SYSTEM <DIR>`) i
  `docs/screenshots/boot/fat12-fdd.png` (`fdd` → `SYSTEM 0`). Poprzednia
  wersja 16-bit low-cluster działała, ale nie była standardowym FAT12.

- **`hw`: model/pojemność HDD oraz konfigurowalny zegar CPU (2026-09-26).**
  Dwie dziury w raporcie sprzętowym (`system_report` → HW.APP/SYSINFO):
  1. **HDD** — kernel sonduje dysk komendami SCSI (INQUIRY/READ CAPACITY) po
     rejestrach ACSI, co nie działa na TF536 IDE, więc pokazywał
     `(no model) / capacity unknown`. Handoff **v17 (128 B)** niesie teraz
     `disk_name/disk_sectors/disk_sector_bytes/disk_known`, wypełniane przez ROM
     (`ide_model` + `ide_capacity_sectors` dla IDE, `acsi_inquiry_name` dla
     ACSI); `kernel_system_report` używa tego, z fallbackiem na
     `kblock_identify`.
  2. **CPU clock** — na ST/Mega ST z akceleratorem (TF536) zegar nie jest
     czytelny (magistrala daje 8 MHz, nie rdzeń), więc było `clock unknown`.
     Dodane trwałe `cpu_clock_mhz` w `BIOS.CFG` (offset 28), edytowalne w SETUP
     (pole `CPU CLOCK`, wartości 0/8/16/25/32/40/50 MHz dla każdej maszyny, nie
     tylko Mega STE) i ustawiane hostem (`make_bios_cfg.py --cpu-clock N`).
     Raport pokazuje je jako `(nominal)`, nie `(measured)`.
  **Dowód:** `docs/screenshots/boot/hw-identity.png` — `CPU: MC68030 @ 32 MHz
  (nominal)`, `HDD: Hatari IDE disk 768M / 768 MiB / 1572865 sectors`;
  `docs/screenshots/boot/setup-clock.png` — pole `CPU CLOCK 32 MHZ SETUP VALUE`
  (Down zmienia na `25 MHZ`).

- **`path`: wadliwy builtin usunięty — teraz uruchamia `PATH.APP` (2026-09-26).**
  *(KOREKTA: builtin **nie** został usunięty — nadal żyje w
  `shell_module.S:763-767` i `:907-934`; usunięto tylko panic przez zapis
  `%d2` (sekcja „Panic `path`” niżej). Samo `path` bez argumentu trafia w
  builtin, a `path \X` spada do `PATH.APP`.)*
  Niedokumentowany builtin `path` kończył się `KERNEL PANIC / ILLEGAL
  INSTRUCTION` przy wywołaniu `api+160` (`kernel_exec_path_get`) — mimo że API
  było poprawne (runtime: `module_api` @0x3b3a4, `+160 = 0x2395e` =
  `kernel_exec_path_get`, `+180 = 0x2840e`; bajty funkcji i `system_exec_path`
  zgodne z ELF/KERNEL.SYS) i **to samo wywołanie z `shell_make_exec_path`**
  (start programu po nazwie, np. `command`) działa. Usunięto więc builtin —
  `path` jest teraz zwykłą nazwą programu i uruchamia `\SYSTEM\APP\PATH.APP`
  (tak jak `AUTOEXEC.BAT`), który wypisuje/ustawia PATH. **Dowód:**
  `docs/screenshots/boot/path-app.png` — `PATH=\SYSTEM;\SYSTEM\COMMAND;
  \SYSTEM\APP;\SYSTEM\TEST`. `pwd`, `dir`, `help`, `command` bez zmian.

- **Launcher TOS (Etap 0) — ZROBIONE (2026-09-26).** Program TOS `.PRG`/`.TOS`
  jest teraz zwykłą komendą z shella i Commandera:
  - jądro: **syscall 65** (`kernel_exception.c`) → `user_path` +
    `kernel_tos_run_program`;
  - API modułu **v5**: `tos_run` @ offset 188 (`include/kernel_module.h`,
    `kernel_module.c`);
  - shell: komenda **`tos <PLIK.PRG>`** (`shell_module.S`);
  - Commander: `run_program()` rozgałęzia po rozszerzeniu — `.PRG`/`.TOS` →
    `gui_call(65)`, `.APP`/`.EXE` → natywny spawn; pasek statusu pokazuje
    `TOS RC <kod>`. Kod TOS jest poza `COMMANDER_SELFTEST`, bo COMTEST ma limit
    zasięgu `-mpcrel`.
  - **Dwa krytyczne fixy:** (1) `asf_tos_enter` ustawiał `%usp`, ale go nie
    zapisywał, a `asf_tos_exit` nie odtwarzał — wejście z **procesu** (Commander
    przez syscall) wracało z zepsutym stosem (`USER_STACK_POINTER_FAULT`); teraz
    USP jest odkładany na stosie persony przez `%a0` (bo `MOVE USP` wymaga
    rejestru adresowego) i przywracany. (2) Commander nie zdejmuje już ekranu
    przez `rpc(gui,7/6)` wokół uruchomienia TOS — ta podróż tam i z powrotem
    kończyła proces Commandera (`[PID n EXIT]`); persona sama rządzi konsolą.
  **Dowód:** `docs/screenshots/boot/tos-shell-run.png` — `tos \PROG1.PRG` →
  `TOS ENV: XBIOS GEMDOS VDI AES READY`, `LD:enter/stat/alloc-file/alloc-mem`,
  powrót do `tts1 C:\>`; `docs/screenshots/boot/tos-commander-run.png` — Enter
  na `PROG1.PRG` w Commanderze → persona, powrót do Commandera z `TOS RC 0`.
  Znane ograniczenie: tekst programu TOS trafia do konsoli ROM (`0x00C00000`),
  więc Commander po powrocie przemalowuje swój ekran (docelowo przechwycić tekst
  do terminala Commandera).
  **Etap 1 (następny):** trap AES (`appl_init`, `wind_*`, `evnt_multi`,
  `objc_draw`) — bez tego prawdziwe programy GEM padają, a GEM desktop jako
  `.PRG` nie wstanie.
- **PATH: porządki + stress-test granicy faultów (2026-09-26).**
  - `system/AUTOEXEC.BAT` nie wymusza już `C:`. Uwaga: `call 35` (ustawianie
    PATH) i tak obcina prefiks `X:` (`segment[1]` to dwukropek), więc abstrakcja
    boot-drive była zachowana runtime — teraz źródło to odzwierciedla
    (`\SYSTEM;\SYSTEM\COMMAND` + `+\SYSTEM\APP;\SYSTEM\TEST`).
  - `PATH.APP` było budowane, ale **nieinstalowane** (`build_atos2.py` wgrywał
    tylko COMMAND/SYSINFO/SETUP/HW), więc `path` dawało `PROCESS START FAILED`.
    Dodane do listy instalacji; `path` wypisuje teraz
    `PATH=\SYSTEM;\SYSTEM\COMMAND;\SYSTEM\APP;\SYSTEM\TEST`.
  - **Zły PATH nie powoduje paniki:** `path \NOPE` → PATH.APP odrzuca segment
    (`call 35` waliduje przez `kfat_stat_path` i przy błędzie **nie zapisuje**
    PATH) → `USAGE: PATH.APP DIRECTORY`, powrót do promptu.
  - **Granica faultów trzyma:** program TOS `SYSINF83.PRG` (crash bez AES) →
    `TOS PROGRAM FAILED` i powrót do shella — bez `KERNEL PANIC` i bez haltu.
    `system_exec_path` (runtime 0x3e2d4) potwierdzony niezmieniony przed/po.
  - **ZNALEZIONY BŁĄD (stan shella, do naprawy w przebudowie):** po
    **nieudanym** uruchomieniu procesu (np. `path \NOPE`, exit 1) następne
    **bare-name** (`path`) daje `PROCESS START FAILED`, choć `system_exec_path`
    jest poprawny; jawne `run \SYSTEM\APP\PATH.APP` działa i leczy kolejne
    bare-name. Repro: `path` → `path \NOPE` → `path`. Plan:
    `docs/SHELL-REFACTOR-PLAN.md` (wydzielenie `shell_path`/`shell_proc`);
    nie dokładamy już zmian do `shell_module.S`/`kernel_exception.c`.
- **Model drive/PATH + test „hostile PATH" (2026-09-26).**
  - **Ścieżki z prefiksem `X:` działają** (device-independent): `shell_build_path`
    traktuje `X:\...` jak absolutną i ignoruje literę; `resolve_path`/
    `mutation_parent` w `kernel_fat.c` też obcinają prefiks. Wynik:
    `dir C:\SYSTEM` listuje, `cd C:\SYSTEM` → `tts1 C:\SYSTEM>` + `pwd` = `\SYSTEM`,
    `run C:\...\HW.APP` startuje. FAT obsługuje jeden zamontowany wolumen, więc
    litera jest akceptowana i ignorowana (zgodnie z podejściem `call 35`).
  - **Hostile PATH — ani razu panic.** Bateria: `dir/cd C:\...`, `run NOSUCH.APP`,
    `run C:\...`, `path C:\SYSTEM`, `path \NOPE`, `path` (puste/niepoprawne),
    `run` bez argumentu — wszystko kończy się kontrolowanym komunikatem
    (`PATH NOT FOUND`, `PROCESS START FAILED`, `USAGE: ...`).
  - **Pułapka metodyczna:** `hatari-event keydown :` podawał **nieshiftowany**
    klawisz, więc shell widział inny znak i testy wyglądały na regresję. Harness
    (`atos2-tools/atos2_auto.py`) mapuje teraz `:` na Shift+`;` i `keydown`/
    `keyup` obsługują Shift.
## Do zrobienia (kolejne sesje)
**Uwaga:** stara lista „Phase 3/4/5/6/7 — do zrobienia” była nieaktualna; te
fazy są opisane jako zrobione wyżej. Aktualny stan:

- **Zrobione:** Phase 1, 2, 3, 4 (storage), 5 (rdzeń), 6 (rdzeń), 7/10/11
  (video_driver_t + backendy), 20 (cpu_ops); ET4000 w ROM-ie; handoff v17;
  **plan item 1 — prawdziwy zapis ACSI**, **ABI 1.0 natywnych aplikacji**
  (sekcje na końcu).
- **Otwarte wg `docs/PRZEGLAD-PRACY.md` §5:**
  1. ~~ACSI write~~ — **ZROBIONE** (plan item 1).
  2. ~~Testy z asercjami (hostile PATH, `-2`/reuse pamięci)~~ — **ZROBIONE**:
     `atos2-tools/scenario_path_assert.py` (5/5 PASS, `SCENARIO_EXIT=0`).
  3. ~~`diag_ram_pattern` w POST + suma kontrolna ROM-u + ekran awaryjny~~
     — **ZROBIONE** (POST 9 testów, `ROM:PASS/FAIL`, banner przy FAIL i brak
     auto-bootu).
  4. ~~FDC `0xffff860e`~~ — **ZROBIONE** (8 MHz + DD na MegaSTE/TT/Falcon).
  5. ~~Retry IDENTIFY IDE~~ — **ZROBIONE** (4 próby z odstępem).
  6. ~~Porządki w sprzecznych dokumentach~~ — **ZROBIONE**.
  7. Builtiny shella COPY/MOVE/DEL/MKDIR — **świadomie odłożone** do
     wydzielenia `shell_fs` (`docs/SHELL-REFACTOR-PLAN.md`: zakaz dokładania
     nowych rzeczy do `shell_module.S`).
  8. ~~`console_lines()`/`type_checked()`~~ — **ZROBIONE**. Prawdziwa
     przyczyna: `memdump` wymaga dziesiętnego licznika, a komórki konsoli są
     **big-endian** (bajt znaku to `2*i+1`).
- **Mile:** V10-A (checklista), V10-B (testy na TOS/EmuTOS/ATOS), V10-C
  (decyzja); docelowo AES/VDI EmuTOS wpięte w `video_driver_t`.

## Panic `path` — przyczyna źródłowa i naprawa (item 3)
**Objaw:** `vec=4 PC=4 SR=9472`, KERNEL PANIC po wpisaniu `path`.

**Mechanizm (dowód z `--trace cpu_disasm`):**
- `kernel_main.c:322` GCC hoistuje `d2 = &kernel_user_gui_keyboard_active`
  **poza** pętlę `for(i=0;i<32;++i)` i woła przez `movea.l %d2,%a1; jsr (%a1)`
  (`0x2210e`/`0x22110`). Back-edge pętli (`0x2250e bne 0x22108`) **pomija**
  `movel #0x2467e,%d2`, więc `%d2` musi przetrwać całe ciało pętli.
- Trace: przy `0x29002` (`kernel_ikbd_irq_capture`) na stosie leży już
  `d2 = 4` → `jsr (%a1)` = `jsr 4` → wektor 4 / PC 4.
- Sprawca: **`shell_do_path_display`** (`shell_module.S`, offset `0xAC6`,
  runtime `0x01014ed6`) — wołany jako `shell->pump` → handler tail-jumpem
  `01014d0a bne 01014ed6`. Używał `%d2` jako licznika pętli 0..7 **bez
  zapisu**; gdy `get_exec_path(4)` zwróciło 0, wychodził `rts` z `%d2 = 4`.

**Naprawa** (ABI m68k: d2-d7/a2-a6 są callee-saved), w `shell_module.S`:
- `shell_do_path_display` — `move.l %d2,-(%sp)` + `move.l (%sp)+,%d2` przed `rts`;
- `shell_parse_u32` — to samo (wołany przez 8 handlerów);
- `shell_do_wait` — zapis + lokalny epilog `shell_wait_done` dla wszystkich wyjść;
- `shell_do_watchdog` — usunięty tymczasowy `%a2` (wystarcza `%a1`, którego
  `shell_parse_u32` nie rusza).

**Zapobieganie:** `tools/asm_abi_audit.py --check` jest teraz krokiem builda
(`tools/build_atos2.py`) dla 17 plików `.S` wołanych synchronicznie z kernela.
Pliki aplikacji są wyłączone — apka działa jako osobny proces, nie podprogram.
Wynik: `ABI CHECK PASS`, 0 naruszeń.

**Weryfikacja runtime:** `path`, `path \\NOPE`, `wait`, `watchdog`, `kill`,
`help`, `run \\SYSTEM\\APP\\HW.APP` — brak paniki, shell żyje; log Hatari:
0 wystąpień `panic|illegal|fault`. Zrzuty: `atos2-tools/shots/abifix/`.

**Osobno (item 4):** `path \\NOPE` oraz `wait` bez argumentu zwracają
`PROCESS START FAILED` zamiast `USAGE:` — **zastany** problem dyspozytora
(handler wymagający argumentu nie trafia w gałąź builtinu i spada do
„uruchom jako program”). `run` działa poprawnie.

## Walidacja na zewnętrznym emulatorze — noSTalgia (2026-09-26)
ROM ATOS 2.0 **bootuje i działa w emulatorze noSTalgia** (68000, bez ET4000),
tryb **mono OK**. Potwierdza to, że **bank1 (ST)** obrazu dualnego jest poprawny
dla prawdziwego 68000/ST — niezależnie od patched Hatari, TF536 i ścieżki
VOFA/ET4000.

- Brak ET4000/VOFA to oczekiwane ograniczenie: grafika ET4000 i desktop nie
  będą dostępne; profil tekstowy/mono działa poprawnie.
- Dedykowany plik dla emulatorów 68000: **`build/atos-bios-st-68000.img`**
  (256 KB = bank1 obrazu dualnego: ST 192 KB + wypełnienie `0xFF`).
- Pliki ROM: `atos-bios-st-68000.img` (ST, 68000), `atos-bios-mste.img` (MSTE),
  `atos-bios-tf536-st-dual.img` (EPROM 512 KB, bank1 ST + bank2 MSTE).
  Nośnik systemu: `work/atos-v2.img`.

## Plan item 1 — prawdziwy zapis ACSI (2026-09-26)
Źródło: plan następnych kroków z `docs/PRZEGLAD-PRACY.md` §5 pkt 1 (ryzyko R1 —
„cichy” błąd na fizycznym ST/STE z dyskiem ACSI).

- **Przed:** `hw/acsi.c:acsi_write_sector` dla ACSI robiło `return 0` bez
  zapisu, więc `cfg_save()`, `block_write` i zatrzask failoveru wideo
  milcząco traciły dane na ACSI (IDE działało).
- **Po:** `acsi_write_command6` wysyła **WRITE(6) (0x0A)** po DMA, tą samą
  sekwencją rejestrów co zweryfikowana natywna ścieżka jądra
  (`system/kernel_block.c:disk_write`): `0x90 → 0x190` (kierunek zapisu,
  `DMA_WRBIT=0x100`), licznik sektorów przez SCREG, bajty komendy z `0x18a`,
  status odczytany `0x8a`; sukces tylko dla statusu 0. IDE nadal przez
  `ide_write_sectors`. Walidacja: `sector<=0x1fffff`, `buffer` wyrównany do
  słowa.
- **Docs:** usunięty sprzeczny akapit „Zapis z jądra na IDE: BRAK ŚCIEŻKI”
  (stary `:710-714`); poprawiona uwaga Phase 3 i komentarz







## Stress 5 minut na żywo (2026-09-27)
- Krotki skrypt `atos2-tools/scenario_stress_live.py` (okno Hatari) przez 300 s
  wykonal 7 cykli i **125 komend, kazda potwierdzona ENTER, 0 zgubionych**.
- Mieszanka: `memory`/`inputstat`/`tasks`, fault-apki (BUSFLT/ADRFLT/DIVFLT i
  `\NOPE.APP`), `shell_fs` (md/copy/dir/move/del, takze sciezka bledu
  `del \NOPE`), `start hang.app` + `jobs` + `kill PID`, przelaczanie
  `tts2`/`tts1` oraz zrzuty `shots/stress-cycle-*.png` co dwa cykle.
- Wynik: brak paniki, brak wycieku procesow (HANG ubijany przez `kill`),
  sesje i montowania per-sesja stabilne.
