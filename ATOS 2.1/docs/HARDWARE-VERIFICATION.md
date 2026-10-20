# ATOS 2.0 — weryfikacja sprzętowa (R2/R3/R5/R7/R8)

Data: 2026-09-26. Uzupełnia \`docs/BIOS-BRINGUP-CHECKLIST.md\` i
\`docs/PRZEGLAD-PRACY.md\` §4. Dzieli ryzyka na to, co **da się** sprawdzić w
emulatorze (automatycznie) i to, co wymaga **fizycznej kości**.

## Automatyczne (host + Hatari)

| Ryzyko | Co sprawdzamy | Jak | Status |
|---|---|---|---|
| R7 | ST/68000 bootuje bez faultu; kod MMU (MOVEC) się nie wykonuje | \`scenario_hw_profiles.py\` profil \`st68000\` (\`--cpulevel 0\`) | ✅ |
| R7 | statyczna brama \`cpu_type>=CPU_68030\` przed \`mmu_boot_rom\` | \`ui/tos.c:27\`, \`main.c\` (auto_tos) | ✅ |
| R5/R8 | bank1 (ST, 0xfc0000) i bank2 (MSTE, 0xe00000) startują | profile \`st68000\`/\`ste\` i \`megaste\` | ✅ |
| R2 | MegaSTE nie regresuje po \`fdc_clock_init\` (0xffff860e) | profil \`megaste\` | ✅ (kod) |
| R3 | retry IDENTIFY (4×) nie psuje bootu IDE | \`hw/ide.c:ide_detect\` | ✅ (kod) |
| R4 | suma ROM: dobry PASS, przekłamany bajt FAIL | \`tests/test_rom_checksum.py\` + Hatari | ✅ |
| POST | \`diag_*\` + PASS/FAIL + ekran awaryjny | \`ui/post.c\` + Hatari | ✅ |

Uruchomienie: \`python3 atos2-tools/scenario_hw_profiles.py\` (używa patched
Hatari \`atari-030-port/hatari-tf536\`). Odczyt to lustro konsoli ROM
(\`console_text\` @ RAM 0x119c, \`char[80*25]\`).

## Wymaga fizycznej kości (procedura)

### R1 — zapis ACSI
1. Dysk ACSI (AHDI/FAT16) z \`work/atos-v2.img\`.
2. Zmień ustawienie w SETUP (F1) i zapisz F10; albo skorzystaj z failsafe Alt.
3. Reset i sprawdź, że ustawienie wróciło — brak cichego powrotu do domyślnych.
   Kontrola: porównaj sektor \`\\BIOS\\BIOS.CFG\` przed/po
   (\`tools/probe_bios_cfg.py\`).

### R2 — gęstość FDC (MegaSTE/TT)
1. Dyskietka **720 KB DD** rozruchowa (\`tools/mk_boot_floppy.py\`).
2. Boot z A: na MegaSTE ma dojść do shella (\`fdc_clock_init\` ustawia 8 MHz + DD).
3. Sprawdzić **HD 1.44 MB**: obecny kod wymusza DD, więc HD może nie zadziałać —
   jeśli potrzebny, rozszerzyć \`fdc_clock_init\` o detekcję nośnika.

### R3 — zimny CF/HDD
1. Zimny start z podłączonym CF/HDD.
2. BIOS ma zaraportować urządzenie IDE (4 próby IDENTIFY z odstępem) zamiast
   \`NO BOOT DEVICE\`.

### R5 — wybór banku EPROM (TF536 ST Edition)
1. Wypal \`build/atos-bios-tf536-st-dual.bin\` (512 KiB, 2 banki).
2. Oba położenia zworki: bank1 = ST (0xfc0000), bank2 = MSTE/TT (0xe00000).
3. Na 68000 ma działać bank1; na MSTE/TT bank2.

### R7 — 68000 bez MOVEC
1. Bank1 w ST/68000 (nie 68030).
2. Oczekiwane: POST bez \`ILLEGAL INSTRUCTION\`, \`CPU MC68000\`.

**Uwaga (znaleziona w Hatari):** bank1 ma w nagłówku wersję \`0x0104\`, którą
Hatari traktuje jako TOS <= 1.4 i **odmawia pracy poza trybem ST/68000** (próbę
STE przełącza do trybu ST). To poprawne dla EPROM-u „ST Edition" (bank1 jest
dla ST/68000), ale maszyna STE wymagałaby obrazu z wersją >= \`0x0162\` albo
banku MSTE/TT.

### R8 — obraz dualny vs pojedynczy
\`build/atos-bios-st-68000.img\` = pierwsze 256 KB dualnego (bank1 + 0xFF),
zweryfikowane binarnie; na kości potwierdzić boot bank1.

## Znane ograniczenia
- Hatari z cykl-dokładnym WD1772: boot z FDD ~3 min.
- Hatari nie oddaje opóźnień spin-upu IDE ani analogowych różnic FDC/ACSI.
- Zapis ACSI nie był uruchamiany na sprzęcie — to port sekwencji jądra
  (\`system/kernel_block.c:disk_write\`).
