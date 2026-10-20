# ATOS 2.0 — analiza rozmiaru: czy kernel + shell zmieszcza sie w ~50 KB

Data: 24.09.2026. Autor: DeepSeek. Pomiary na aktualnym buildzie (z poprawka SR).

## 1. Stan obecny (zmierzone)

| Element | Plik (text+data) | BSS | Razem w RAM |
|---|---|---|---|
| KERNEL.SYS | 101 560 B | 261 618 B | ~363 KB |
| SHELL.SYS | 9 516 B | 6 940 B | ~16,5 KB |
| pozostale moduly SYS (BOOT, ACSI, CORE, VFS, PROC, TIMER, EVENT, INPUT) | ~7,7 KB | drobne | ~8 KB |
| BIOS | bios_mste.bin 262 144 B / bios_st.bin 196 608 B | — | ROM, nie RAM |

Zmierzone komenda memory na maszynie 4 MB: ST-RAM FREE 3083 KB, czyli **system zajmuje ~1013 KB
(~1 MB) jeszcze przed uruchomieniem aplikacji**. To jest kluczowa liczba dla maszyn 512 KB/1 MB.

## 2. Co dominuje

### BSS (RAM) — 261,6 KB
- kernel_wm.c: console_surface[32000] + app_surface[2][32000] = **96 000 B**
- kernel_fat.c: undo[128] po ~516 B = **66 048 B** + chain_seen[8192] + io_data[512] = ~75 KB
- kernel_video.c: shadow_buf[16000] (uint16) = **32 000 B**
- tos_prg.c: tos_scaled i okolice = ~30 KB
- kernel_graphics.c: ~21 KB
- kernel_console.c: ~16 KB; session_cells 12 KB; process_table 5 KB; reszta drobna

### Text (kod) — 101 KB kernel + 9,5 KB shell
Najwieksi (text+data, po gc-sections): kernel_exception 22,2 KB, kernel_graphics 18,9 KB,
tos_prg 18,5 KB, kernel_fat 10,5 KB, kernel_module 7,2 KB, kernel_main 7,1 KB, tos_vdi 4,8 KB,
kernel_wm 4,1 KB, kernel_entry 3,9 KB, ui/font 3,7 KB, wm_core 2,9 KB, vofa_et4000 2,9 KB,
kernel_block 2,4 KB, text_terminal 2,1 KB, reszta po ~1 KB i mniej.

Wazne: kernel jest juz kompilowany z -ffunction-sections -fdata-sections i linkowany z --gc-sections,
wiec nieuzywane funkcje sa w wiekszosci usuniete. 101 KB to naprawde osiagalny kod, nie smieci.

## 3. Dwa znaczenia "50 KB"

(a) Rozmiar pliku/ROM (to, co sie laduje): kernel 101,5 KB + shell 9,5 KB = **111 KB**.
(b) Pamiec w czasie pracy (text+data+BSS): kernel 363 KB + shell 16,5 KB = **~380 KB**, a caly
system przy starcie ~1 MB.

## 4. Czy sie da?

### (a) ~50 KB kodu/pliku
Tak, ale **tylko w trybie minimalnym** i po refaktorze. Obecna konfiguracja nie zejdzie do 50 KB
bez wyciecia/modularyzacji calych podsystemow. Realny budzet rdzenia tekstowego: ~40-45 KB,
shell ~8-9 KB. To wymaga usuniecia TOS, WM/desktop, VOFA i wiekszosci grafiki GUI oraz odchudzenia FAT.

### (b) ~50 KB RAM
**Nie w tej architekturze.** Po przeniesieniu buforow do kmem zostaje ~300-400 KB na tekst, stosy,
ekran i system plikow. Realne minimum tekstowego systemu to ~100-200 KB. 50 KB RAM to projekt
mniejszego systemu od zera (bez FAT albo FAT w ROM, bez multitaskingu, bez GUI).

## 5. Plan optymalizacji (etapami, od najmniejszego ryzyka)

### Etap 0 — pomiar
Dodac cel gmake size (rozmiary per plik + top symbole) i prowadzic rejestr przed/po.

### Etap 1 — male ryzyko, bez utraty funkcji
- LTO + -fno-tree-loop-distribute-patterns. **Sprawdzone:** samo -flto wywala link na
  undefined reference to memcpy (GCC odtwarza memcpy z petli, a kernel jest freestanding i
  celowo go nie ma). Z ta flaga LTO ma szanse.
- -fno-align-functions/-loops/-jumps/-labels oraz -fmerge-constants.
- Usunac kgfx_self_test (~2 KB) i inne self-testy z buildu produkcyjnego.
- Szacunek: 5-15% textu (~5-15 KB).

### Etap 2 — BSS/RAM (najwiekszy zysk, bez utraty funkcji)
- FAT undo 128 sektorow -> 8/16 albo alokacja dynamiczna tylko na czas zapisu: -50 KB.
- app_surface/console_surface (96 KB) -> kmem tylko gdy GUI/desktop naprawde dziala: -96 KB w profilu tekstowym.
- shadow_buf (32 KB) -> tylko gdy wykryto VOFA/ET4000: -32 KB.
- tos_scaled (32 KB) -> tylko gdy osoba TOS jest aktywna: -32 KB.
- Razem ok. -210 KB: start z ~1 MB schodzi do ~800 KB. Nadal za duzo na 512 KB, ale duzo lepiej na 1 MB.

### Etap 3 — modularyzacja (kod i BSS)
- TOS persona (tos_prg/tos_vdi/tos_entry/tos_start/loader/tos/ui/tos) -> opcjonalny modul:
  ok. -25 KB text, -30 KB BSS.
- WM/desktop (kernel_wm/wm_core/wm_blit/desktop_module/mono_desktop/desktop_app) -> modul:
  ok. -9 KB text, -96 KB BSS.
- VOFA (vofa_et4000/video_bus) -> modul: ok. -3 KB text, -32 KB BSS.
- Rdzen tekstowy: entry/main/memory/module/exception + console/text_terminal/font + sessions +
  video_boot/video_driver + block/fat/partition + shell. Szacunek ~55-65 KB text.

### Etap 4 — profil tiny/text (jedyna droga do ~50 KB kodu)
- FAT tylko do odczytu (bez undo/dziennika): 10,5 -> ~4-5 KB.
- Bez wywlaszczania (kooperacja) albo mniejszy model procesu: kernel_exception 22 -> ~10-12 KB.
- Staly offset partycji AHDI, brak fallbacku fontu (font 8x8 z ROM), 1-2 sesje.
- Szacunek: kernel ~35-40 KB + shell ~8 KB = **43-48 KB**. Realne, ale kosztem funkcji
  (multitasking, GUI, TOS).
- RAM takiego minimum: ~35-40 (text) + 8 (shell) + 4-12 (bufor ekranu/komorek) + 4 (stosy)
  + ~2 (dysk) = ~55-65 KB. Do 50 KB tylko rezygnujac z osobnego bufora ekranu i trzymajac 1 sesje.

## 6. Wnioski

1. ~50 KB **kodu** jest osiagalne, ale tylko w trybie minimalnym (Etap 4), nie w obecnej konfiguracji.
2. ~50 KB **RAM** — nie w tym designie. Obecnie start zjada ~1 MB; realne tekstowe minimum ~100-200 KB.
3. Najlepszy stosunek zysku do ryzyka: **Etap 2** (dynamiczne bufory) — ok. -200 KB RAM bez utraty
   funkcji, potem **Etap 3** (modularyzacja) — ok. -60 KB kodu i -160 KB BSS.
4. Dla 512 KB trzeba Etapu 4 lub osobnego, minimalnego profilu; na 1 MB po etapach 2-3 powinno byc
   komfortowo.
5. Uwaga na LTO: wymaga -fno-tree-loop-distribute-patterns, inaczej link pada na memcpy.
6. Kompatybilnosc 68000 zostaje (-m68000); kazda zmiane rozmiaru trzeba sprawdzic na prawdziwym ST.

## 7. Propozycja nastepnego kroku

Zrobic Etap 1 + Etap 2 (flagi + dynamiczne bufory FAT/WM/video/TOS) i zmierzyc tabele przed/po.
To nie zmienia funkcjonalnosci, a daje pierwsze konkretne setki KB. Dopiero potem decyzja, czy
budujemy osobny profil tiny do 512 KB.

## 8. Modularyzacja VOFA/ET4000 i TOS persony

### 8.1 VOFA / ET4000 — tak, wiekszosc moze wyleciec z jadra

BIOS juz wykrywa karte: hw/nova.c::nova_detect() ustawia nova_present / nova_adapter / nova_chipset
(wolane z hw/expansion.c). Kernel robi jednak drugie wykrywanie: kernel_video_boot.c::kvideo_boot()
czyta SYSTEM/VIDEO.CFG i jesli jest "VOFA", wola vofa_et4000_init() + kvideo_activate_vofa().

Sprzezenie z rdzeniem (zmierzone):
- kernel_video_boot -> vofa_et4000_init, kvideo_read8, kvideo_write8, kvideo_activate_vofa
- kernel_graphics / kernel_wm / kernel_module -> kvideo_damage, kvideo_flush, kvideo_is_vofa,
  kvideo_reset, kvideo_select (to cienka abstrakcja, moze zostac)
- kernel_video.c trzyma shadow_buf[16000] = 32 000 B

Wniosek: do modulu VOFA.SYS przenosimy vofa_et4000.c + video_bus.c + sciezke "card"/shadow/blit
z kernel_video.c. Rdzen zostawia kvideo_* jako no-op/Shifter, gdy usluga nie zarejestrowana.
Zysk: ~3 KB text + ~32 KB BSS. Ladowanie tylko gdy VIDEO.CFG prosi o VOFA.

### 8.2 TOS persona (tos_prg.c) — tak, to obsluga aplikacji TOS

tos_prg.c + tos_vdi.c + tos_entry.S + tos_start.S to persona zgodnosci TOS: trap GEMDOS/AES/VDI/
BIOS/XBIOS, loader programow TOS, scheduler TOS i kompozytor okna TOS. Natywne aplikacje ATOS
(shell, Commander, SYSINFO) jej nie potrzebuja.

Sprzezenie z rdzeniem (zmierzone):
- kernel_entry.S (Timer-C) -> tos_composite_tick, tos_task_tick
- kernel_main.c -> kernel_tos_set_console/_console_clear/_cpu/_input/_trace/_video,
  tos_vdi_set_surface, tos_syshdr (plus wpisy 0x44E/0x4F2/0x4BA)

Wniosek: do modulu TOS.SYS przenosimy tos_prg.c, tos_vdi.c, tos_entry.S, tos_start.S.
Rdzen wola je przez opcjonalna usluge kernel_tos_service_t; hook Timer-C staje sie wskaznikiem
(NULL = pomijamy), a wiring w kernel_main jest warunkowy. Zysk: ~24,5 KB text + ~33 KB BSS.

### 8.3 Jak to wpasowac w istniejaca architekture

Kernel juz laduje moduly po nazwie i szuka uslug:
  kernel_load_system_module(name, owner, prefer_tt) + kernel_find_service(id, version)
(patrz kernel_main.c, CORE/VFS/PROC/TIMER/EVENT/INPUT/SHELL). Dodajemy VOFA.SYS i TOS.SYS tym
samym mechanizmem, z wpisem do listy instalatora i do listy ladowania zaleznie od konfiguracji.

Kolejnosc prac:
1. VOFA.SYS: nowy kernel_video_service_t; przeniesienie sterownika i shadow; kvideo_boot laduje modul
   tylko gdy VIDEO.CFG="VOFA"; fallback do Shiftera jak dzis.
2. TOS.SYS: nowy kernel_tos_service_t; przeniesienie persony; hooki Timer-C przez wskaznik; warunkowy
   wiring w kernel_main; ładowanie na zadanie (przy pierwszym programie TOS) albo na starcie.
3. Potem Etap 2 z sekcji 5 (dynamiczne bufory FAT/WM/video), juz bez BSS VOFA.

### 8.4 Ryzyka

- tos_entry.S instaluje wektory trapow (Line-A/F, BIOS/XBIOS/GEMDOS/AES/VDI) — instalowac tylko gdy
  modul zaladowany, inaczej zostawic czyste wektory.
- Hook Timer-C musi byc tani i NULL-safe (wolany co tick); tos_task_tick zwraca SP dla schedulera TOS,
  wiec bez modulu trzeba zachowac dotychczasowa sciezke.
- Realny sprzet z karta i bez karty; test po kazdym kroku, pojedynczo.
- Nie przenosic obu naraz — jedna zmiana, test, dopiero potem druga.

### 8.5 Bilans

Razem VOFA + TOS: ok. -27,5 KB text i -65 KB BSS z rdzenia, plus mozliwosc nieładowania ich na
malych maszynach. To nie jest jeszcze 50 KB, ale to najwiekszy pojedynczy krok w te strone i nie
odbiera funkcji maszynom, ktore tych modulow potrzebuja.

## 9. Wynik: VOFA/ET4000 wydzielone do VOFA.SYS (24.09.2026)

Zrealizowano pierwszy krok z sekcji 8:

- Nowy modul VOFA.SYS (2 996 B): vofa_et4000.c + vofa_module.c + vofa_entry.S, linkowany
  linker/vofa-module-sys.ld, kompilowany z -mpcrel -fno-tree-loop-distribute-patterns;
  rejestruje KERNEL_SERVICE_VIDEO (vtable wypelniany adresami runtime, jak w modulach .S).
- kernel_video_boot.c: przy VIDEO.CFG="VOFA" laduje VOFA.SYS przez kernel_load_system_module
  i wola service->init(&io); brak modulu => komunikat "MODULE MISSING - SHIFTER".
- kernel_video.c: 32 KB shadow alokowane leniwie z kmem dopiero przy aktywacji karty
  (wczesniej statyczne BSS, placone zawsze).
- Makefile + tools/build_atos2.py: VOFA.SYS budowany i instalowany; vofa_et4000.o usuniety
  z linku jadra.

Pomiary:

| Metryka | Przed | Po |
|---|---|---|
| KERNEL.SYS | 101 560 B | 98 826 B (-2 734) |
| kernel .text | 101 378 | 98 679 (-2 699) |
| kernel .bss | 261 618 | 229 622 (-31 996) |
| VOFA.SYS | — | 2 996 B (opcjonalny) |
| ST-RAM FREE po starcie | 3083 KB | 3114 KB (+31 KB) |

Test: python3 tools/build_atos2.py PASS; boot w Hatari OK — "VIDEO: VOFA REQUESTED / NOT FOUND -
SHIFTER" oznacza, ze modul sie zaladowal, a jego init zwrocil VOFA_ABSENT (bez karty zostaje Shifter);
inputstat wszystkie liczniki 0. Programowanie trybu VOFA nie bylo testowane bez karty ET4000 —
kod jest ten sam, tylko przeniesiony do modulu.

Patch: work/vofa-module.patch.

## 10. TOS opcjonalny (TOS=0) i dynamiczne bufory (24.09.2026)

### TOS=0 jako profil build-time

Dodano przelacznik Makefile TOS=1/0. TOS=0 wyklucza tos_prg/tos_vdi/tos_entry/tos_start i wylacza
wolania w kernel_main.c oraz kernel_entry.S.

Pomiar: KERNEL.SYS 98 826 -> 96 153 B (-2 673), BSS 229 622 -> 219 602 (-10 020).

Wazne: gc-sections i tak usuwa wiekszosc persony TOS w profilu tekstowym (trafienia TOS sa rzadko
uzywane), wiec realny zysk to ~13 KB, a nie 24/33 KB z sumy obiektow. Pelny modul TOS.SYS wymagalby
API dla 27 symboli jadra (plus dane) i zmieszenia sie w zakresie +/-32 KB przy 33 KB BSS — zly
stosunek zysku do pracy. Sensowniejszy kierunek to TOS=0 jako profil dla malych maszyn.

### Dynamiczne bufory (bez utraty funkcji)

- kernel_fat.c: undo[128] (66 KB) -> wskaznik alokowany leniwie przy pierwszym zapisie.
- kernel_wm.c: console_surface[32000] + app_surface[2][32000] (96 KB) -> wskazniki alokowane przy
  pierwszym uzyciu (WM/GUI); w profilu tekstowym nie zajmuja nic.

Pomiar: BSS 229 622 -> 67 590 (-162 032); ST-RAM FREE po starcie 3114 -> 3272 KB (+158 KB).

Razem z VOFA (shadow 32 KB) i wczesniejszymi zmianami: BSS 261 618 -> 67 590 (-194 KB), wolny
ST-RAM po starcie 3083 -> 3272 KB. Build i testy PASS; boot OK; stress 15 cykli bez strat.

## 11. Audyt kmem i obnizenie bazy kernela (24.09.2026)

Audyt (osobny plik: docs/AUDYT-RAM-KMEM_2026-09-24_PL.md) pokazal, ze glownym kosztem nie sa bufory,
lecz rezerwacja: ST free 3 349 840 B, reserved 708 064 B, used ~824 KB. Reserved to glownie baza
kernela 0x00080000 (512 KB) + obraz kernela ~167 KB + 16 KB gory.

Zmiana ukladu:
- KERNEL.SYS: 0x00080000 -> 0x00020000,
- BOOT.SYS: 0x00020000 -> 0x00008000 (musi zejsc, bo kernel zajmuje jego miejsce),
- spójnie: include/boot_protocol.h, linker/kernel-sys.ld, linker/boot-sys.ld, system/kernel_entry.S,
  system/boot_entry.S, loader/bootstage.c, tests/test_kernel_load_addr.py.
- Ekran (SCREEN_BASE 0x00010000, 32 KB) i BIOS (data/bss od 0x400, ~7 KB) nie koliduja:
  0x8000 BOOT.SYS, 0x10000-0x17D00 ekran, 0x20000 kernel.

Wynik: ST-RAM FREE po starcie 3272 -> **3656 KB (+384 KB)**, used ~824 -> ~440 KB.
Clean rebuild + wszystkie testy PASS; boot OK; stress 15 cykli bez strat.

Patch: work/layout-ram.patch.





## 12. Aktualizacja 27.09.2026 — TOS=0 domyslny, pomiar po rozbudowie persony

Po rozbudowie persony TOS (GEMDOS/AES/VDI) `gc-sections` przestal usuwac
personę w profilu domyslnym, bo `kernel_module.c` (tablica API) i
`kernel_exception.c` (syscall 65) bezwarunkowo siegaja po
`kernel_tos_run_program`. Zmierzone `m68k-elf-size` dla biezacego jadra:

| Profil | text | data | BSS |
|---|---|---|---|
| `TOS=0` (domyslny) | 101 241 | 128 | 60 232 |
| `TOS=1` | 125 129 | 157 | 87 308 |
| **roznica** | **−23 888** | −29 | **−27 076** |

Stub `kernel_tos_run_program` pod `#if !CONFIG_TOS` (zwraca `-0x2ff`) domyka
link, a shell raportuje `TOS PERSONA NOT BUILT IN THIS KERNEL`. Wniosek z §10
(modul TOS.SYS nieoplacalny) pozostaje aktualny; profil build-time jest
domyslny, a `TOS=1` wraca do zgodnosci bez regresji.
