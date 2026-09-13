# Analiza systemu ASF BIOS (Atari ST / STE / Mega STE)

Raport z analizy katalogu `files-pasted-by-the-user-projekt` (3,0 GB, ~1000 plików źródeł).
Analiza obejmowała czytanie źródeł, inspekcję artefaktów oraz **wymuszony pełny rebuild**.
Nie uruchamiałem ROM-u interaktywnie w emulatorze — twierdzenia o zachowaniu wynikają z kodu,
a nie z testów emulacyjnych.

## Werdykt w jednym akapicie

To nie jest zabawka ani szkielet: to **działający, samodzielny system operacyjny dla 68000**,
który startuje z ROM-u, wykrywa sprzęt, czyta FAT16 z ACSI, ładuje własny łańcuch modułów
i uruchamia wielozadaniowe procesy użytkownika z powłoką i warstwą graficzną. Build jest
**zielony i bit-for-bit reprodukowalny**, a testy przechodzą. Główne problemy nie dotyczą
poprawności, lecz higieny wydania, kilku martwych ścieżek w kodzie i świadomych uproszczeń,
które są konsekwencją braku MMU na 68000.

| Obszar | Stan |
|---|---|
| Toolchain `m68k-elf-` | kompletny (GCC 16.2.0) |
| Build | ✅ zielony, 0 błędów, 53 ostrzeżeń (segmenty RWX) |
| Reprodukowalność | ✅ `checksums.txt` identyczny po pełnym rebuildzie |
| Testy | ✅ `check`, `check-graphics`, `check-wm`, `check-gui-apps` przechodzą |
| Rozmiary ROM | ✅ `bios_mste.bin` = 262144 B, `bios_st.bin` = 196608 B |
| System kontroli wersji | ❌ brak repozytorium git |
| Pamięć TT-RAM | ⚠️ zaszyta na `0x01000000` |
| Izolacja procesów | ⚠️ brak MMU, wyłącznie programowa |

## 1. Łańcuch startu

ROM nie udostępnia usług TOS — ma 48-bajtowy nagłówek zgodny ze sprzętem Atari, ale własną
ścieżkę startu (`boot/reset.S:7-28`, sygnatura `"ASFW"`, brak klasycznej pary SSP/PC).

1. `_reset` (`boot/reset.S:30`) — `SR=0x2700`, `SP=0x8000`, skok do `ram_detect_early`.
   Powrót przez `A6`, bo zmiana MEMCONF unieważnia zwykłe adresy powrotu.
2. **Detekcja banków** (`boot/memdetect.S:11-62`) — metodą zawijania adresów, kody MEMCONF
   0/1/2, wynik w `D5`. Próg walidowany w krokach 512 KiB (`reset.S:43-53`), stos
   `SP = rozmiar - 0x1000`.
3. `firmware_main` (`main.c:28`) — CPU/FPU → maszyna → TT-RAM → expansions → RTC/Shifter/IKBD
   → POST → 4-sekundowe odliczanie → `boot_stage2_run()`.
4. **Ładowanie z dysku** (`loader/bootstage.c:87-102`) — skan ACSI ID 0-7, BPB FAT16,
   plik `SYSTEM/BOOT.SYS`, obraz pod `0x00020000`, magic `ASYSBOOT`.
5. **BOOT.SYS** dostaje `boot_handoff_t` (52 B, magic ASHO v3) z callbackami
   console / start_kernel / load_module / acsi_read_sector i woła `h->start_kernel()`
   (`system/boot_main.c:15-32`), co ładuje **KERNEL.SYS** pod `0x00040000` (magic `ASYSKERN`).

Adresy `0x20000` i `0x40000` są **stałe — nie ma relokacji**, limit obrazu to 512 KiB
i `≤ rozmiar_ram - 8192`.

## 2. Pamięć

- ROM: `0x00FC0000` / 192 KiB (ST) oraz `0x00E00000` / 256 KiB (MSTE) — `linker/st-rom.ld:6`,
  `linker/mste-rom.ld:6`; oba mają `ASSERT` przeciw przepełnieniu.
- RAM: `0x400`–`0x400000`; pierwszy 1 KiB to wektory 68000, dlatego `.bss` startuje od `0x400`.
- Alokator (`system/kernel_memory.c:12-23`): pula ST-RAM od końca obrazu jądra do
  `st_total - 0x4000` (16 KiB rezerwy); bloki z magic `0x4b4d454d`, właścicielem, flagami
  READ/WRITE/EXEC/SUPERVISOR, guardem, coalescingiem i PIN-em.
- **TT-RAM na sztywno od `0x01000000`** (`kernel_memory.c:21`) — brak modelu „brak TT-RAM”.

## 3. Wyjątki i wejście do jądra

Wektory instaluje `kernel_exception_install` (`system/kernel_entry.S:36-67`): 2, 3, 4-8, 10, 11
oraz `trap 32-47`. W fazie bootowania działa tymczasowy instalator (`boot/exception.S:10-18`),
który dodatkowo obsługuje sondy sprzętowe (bus error z odtworzeniem PC) — **nadpisuje wektory
8-63 bez zapisu oryginałów**.

- **Błąd aplikacji jest izolowany**: bramka `kernel_user_validate_entry`
  (`kernel_exception.c:1262-1276`) sprawdza USP, PC w segmencie obrazu i SR; proces kończy się
  statusem `-(0x100 + wektor)` (`:1140`), bez paniki.
- **Błąd jądra = PANIC + STOP** (`:1602-1606`), bez zrzutu rejestrów i bez restartu.
- **Syscall**: `trap #0`, wektor 32 (`kernel_entry.S:265-327`), `D0` = numer, `D1/D2` = argumenty,
  `A0/A1` = wskaźniki.

## 4. Wielozadaniowość

Model **hybrydowy**: preempcja przez IRQ Timer C (50 Hz) **plus** kooperacja przez `yield`/`sleep`
i bramkowanie w pętli supervisora (`kernel_main.c:148`).

- Kontekst: `user_process_t` (`kernel_exception.c:40-65`) — 15 rejestrów, USP/PC/SR, stan,
  `wake_tick`, watchdog, skrzynka 8 wiadomości, 4 pliki, 8 bloków pamięci, cwd/argumenty/nazwa.
- **6 slotów procesów** (`USER_PROCESS_SLOTS=6`), kwota pamięci **1 MB/proces**.
- Stos użytkownika 2 KiB z guardami `0x5354414b`, sterta z guardem `0x48454150`.
- Powrót do supervisora przez sentinele; `kernel_user_saved_sp[4]` ogranicza zagnieżdżenie do 4.
- **Brak MMU**: ochrona to wyłącznie programowa walidacja zakresów
  (`user_range`, `user_write_range`, wariant per-proces w `:1168-1175`). Dokumentacja przyznaje
  to wprost (`docs/memory-and-multitasking.md:6-11`) — na 68000/68010 twarda izolacja nie jest
  możliwa.

## 5. Moduły `.SYS` i ABI

- Nagłówek 32 B: `"ASYSMOD0"`, ABI **3**, rozmiar, `load_addr = 0`, offset wejścia, rozmiar
  pamięci, suma kontrolna. **Obrazy muszą być PIC — brak relokacji.**
- Moduł dostaje `kernel_module_api_t` (`include/kernel_module.h:21-69`) — ok. 38 wskaźników
  (alokacja, ACSI, rejestracja/znajdowanie usług, pliki, konsola, procesy, CPU).
- `register_service` (`kernel_module.c:286-295`) wymaga wskaźnika **wewnątrz obrazu modułu**,
  limit 16 usług, **zakaz duplikatu ID i brak wyrejestrowania**.
- Wersje usług: CORE 1, VFS 3, PROC 5, TIMER 1, EVENT 1, INPUT 1, SHELL 2, KGFX 2.
  Kolejność startu: ACSI → CORE → VFS → PROC → TIMER → EVENT → INPUT → SHELL.

## 6. Warstwa usług

**VFS** — AHDI + łańcuch XGM (max 14 wolumenów), FAT16 z odrzucaniem `<4085` i `≥65525`
klastrów, łańcuch klastrów z detekcją pętli, ścieżki 8.3, max 8 poziomów, cache 8 sektorów.
**Wyłącznie odczyt** — brak create/write/delete, brak LFN/FAT32.

**PROC** — dwa byty: `PROC.SYS` to 2 stałe zadania kooperatywne (nośnik testów), a właściwe
procesy użytkownika żyją w `kernel_exception.c` (spawn/wait/exit, syscalle 21-25, 31-34).

**TIMER** — MFP Timer C, ~49,75 Hz, **wyłącznie odpytywanie** (bez przerwań), ticki liczone
z zawinięć 8-bitowego licznika → przy długim obiegu pętli mogą być gubione.

**EVENT** — 2 skrzynki po 32 wpisy; pełna skrzynka to błąd.

**INPUT** — dekoder IKBD nad EVENT+TIMER, jeden make na break, osobna kolejka myszy,
kwarantanna 2 ticków po overrun, fallback na ACIA.

**SHELL** — ok. 25 komend (`help`, `dir`, `ls -l`, `cd`, `type`, `cat`, `ps`-podobne `tasks`,
`memory`, `start`, `kill`, `path`, `watchdog`, `fg`, `bg`, …), historia 8 wpisów, TAB-uzupełnianie.
PATH trzyma jądro: domyślnie `\SYSTEM;\SYSTEM\COMMAND;\SYSTEM\APP;\SYSTEM\TEST`, max 8 wpisów.

> **Luka:** `AUTOEXEC.BAT` istnieje i README (linia 246) twierdzi, że shell go czyta, ale
> **w kodzie nie ma żadnego odczytu tego pliku** (0 trafień `AUTOEXEC` w `system/`, `include/`,
> `tools/`, `Makefile`). Co więcej, druga linia pliku (`+C:\SYSTEM\APP;...`) nie odpowiada żadnej
> komendzie — shell nie ma handlera `'+'`.

## 7. Grafika i GUI

Trzy poziomy: backend jądra `kgfx_*`, broker `SYSTEM.GFX`, polityka `SYSTEM.GUI`; klientami są
`DESKTOP.APP` i nowszy menedżer okien w C (`mono_wm.c`, `mono_desktop.c`).

Działa: planarne piksele/wypełnienia/ramki/linie (Bresenham + Cohen–Sutherland), clipping,
czcionka 8×16, kursor XOR, wypełnianie Blitterem, powierzchnie z guardem właściciela, okna mono.
Uwaga: `emutos_vdi_mono.c` **nie jest VDI** — to 5 funkcji prostokątnych.

Ograniczenia: tryby tylko 0/1/2, batch GUI zawsze pełne 32000 B bez vsync (tearing),
**2 okna na sztywno**, brak Z-order i clippingu okien.

## 8. Aplikacje testowe (72 pliki `*_app.S`)

Świadomie pokrywają całe ABI: smoke (`hello`, `sysinfo`, `memtest`), VFS (`filetest`, `seektest`,
`dirtest`, `cwdtest`, `iotest`, `path`), procesy (`proctest`, `spawntest`, `pidwait`, `orphan`,
`procstress`), kooperacja/IPC (`coopa`, `coopb`, `async`, `ipctest`, `svctest`), kontekst
(`ctxswitch`, `ctxrr`, `ctxframe`), **izolacja błędów** (`busfault`, `addressfault`, `divfault`,
`privfault`, `linea`, `linef`, `stackfault`, `heapguard`, `quota`) oraz GUI (`gfxtest`,
`surtest`, `guitest`, `guimouse`).

To najdojrzalsza część projektu: każdy mechanizm jądra ma odpowiadający mu program-test.

## 9. Build, testy, wydanie

**Toolchain** kompletny: `m68k-elf-gcc/ld/objcopy/as/ar` (GCC 16.2.0), Hatari 2.6.1,
GNU Make 4.4.1 (`gmake`), Python 3.14.

> **Krytyczne:** `Makefile` używa grupowanych celów `&:` (linie 827, 830), których **domyślny
> `/usr/bin/make` (GNU Make 3.81) nie obsługuje** — buduje błędnie i emituje ostrzeżenia
> „overriding commands for target `&'”. **Budować wyłącznie przez `gmake`.**

Wykonany rebuild: `gmake -B -j4` → **exit 0**, 0 błędów, 53 ostrzeżeń (wszystkie to
`LOAD segment with RWX permissions`). `checksums.txt` po rebuildzie **identyczny** z przed —
build jest bit-for-bit reprodukowalny.

Testy: `gmake check` → 4 testy OK (narzędzia ROM + maszyna stanów snu + orakl kursora);
`check-graphics` → 30000 wypełnień i 30000 linii vs orakl pod ASan/UBSan; `check-wm` →
10000 operacji drag/resize; `check-gui-apps` → MONODESK/COUNT/NOTE (suma, polityka, PIC).

**Problem pakowania:** katalog `outputs/` (3,0 GB, 99 pozycji) **nie jest podłączony do builda**
(0 wystąpień `outputs/` w Makefile), a jego kopie ROM-ów mają **inne sumy SHA256** niż bieżący
build — to przestarzałe, mylące snapshoty.

## 10. Ryzyka — lista priorytetowa

**Wysokie (realne błędy):**
1. `AUTOEXEC.BAT` niezaimplementowany mimo deklaracji w README; plik wewnętrznie niespójny.
2. `make` zamiast `gmake` po cichu produkuje zły build (grupowane cele `&:`).

**Średnie (martwy kod / niespójność):**
3. Kolizja `call == 46` — `kernel_exception.c:1473` (get PATH) przesłania `:1569`
   (release klawiatury GUI); druga gałąź jest nieosiągalna.
4. Syscall 12 raportuje „najwyższy = 44”, choć działa 45-48 (`kernel_module.c:221`).
5. Kolizja ownera pamięci `0x0101`: `BLOCK_OWNER` i CORE.SYS dzielą tag → słabsza izolacja.
6. Brak mechanizmu wyrejestrowania/zamiany usług; `find_service` nie waliduje magic.
7. `outputs/` wprowadza w błąd co do aktualności ROM-ów.

**Uproszczenia architektoniczne (świadome):**
8. Brak MMU — ochrona wyłącznie programowa; panika jądra bez recovery.
9. TT-RAM zaszyty na `0x01000000`; brak modelu jego braku.
10. Sztywne adresy ładowania BOOT.SYS/KERNEL.SYS, brak relokacji, limit 512 KiB.
11. VFS tylko do odczytu, bez LFN/FAT32; TIMER odpytywany (możliwe gubienie ticków);
    PROC bez priorytetów i bez wywłaszczania w jądrze.
12. GUI: 2 okna, brak Z-order, batch bez vsync.
13. `monitor/` jest pusty (poza milestone 0.01).

## 11. Zalecenia

1. **Wymusić `gmake`** w skryptach i dokumentacji albo przepisać `&:` na składnię przenośną.
2. **Uporządkować `outputs/`** — podłączyć do builda albo usunąć przestarzałe kopie ROM-ów.
3. Zaimplementować `AUTOEXEC.BAT` albo usunąć go i sprostować README.
4. Domknąć kolizje numerów syscalli i ownera pamięci; ujednolicić raportowaną wersję ABI.
5. Dodać agregat `check` obejmujący `check-graphics`, `check-wm`, `check-gui-apps`.
6. Wyczyścić 53 ostrzeżenia o segmentach RWX.
7. Założyć repozytorium git — obecnie wydanie nie ma proweniencji wersjonowania.

## 12. Jak to zweryfikowałem

- Analiza statyczna źródeł z odwołaniami do plików i numerów linii (rdzeń, usługi, ABI, grafika).
- Inspekcja artefaktów: rozmiary ROM-ów, mapy linkera, sumy z `checksums.txt` (przeliczone).
- **Wymuszony pełny rebuild** (`gmake -B -j4`) z kopią zapasową 13 artefaktów w `/tmp/biosbak`;
  nic nie usunięto, `make clean` nie był użyty.
- Uruchomienie zestawów testowych `check`, `check-graphics`, `check-wm`, `check-gui-apps`.

Nie uruchamiałem ROM-u w Hatari w trybie interaktywnym — zachowanie w emulatorze (grafika,
klawiatura, mysz, wielozadaniowość w czasie rzeczywistym) pozostaje do potwierdzenia wizualnie.
