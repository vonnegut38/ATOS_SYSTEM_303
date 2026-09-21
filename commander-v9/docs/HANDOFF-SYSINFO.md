# HANDOFF — SysInfo 8.33 w personie TOS (commander-v9)

Dokument dla nowej sesji DSH. Stan na koniec Sesji 22 (22/40).
Wszystkie liczby/ścieżki zweryfikowane w drzewie, nie z pamięci.

---

## 1. Cel i kryterium

Cel (goal): zaimplementować wielozadaniowy (cooperative) minimalny AES/desktop
w personie TOS tak, aby **SysInfo 8.33** (`SYSINF833.PRG`, 3x HWAM-SOFT + PFX
1.13P) uruchamiał się pod naszym kernelem i **rysował swoje okno w
kompozytorze** (docelowo w oknie, nie fullscreen).

Kryterium: SysInfo widoczne w oknie przy działającym multitaskingu innych sesji.

Krok 0 (dlaczego SysInfo kończyło się bez trapów) — **ROZWIĄZANY** w Sesji 11.

---

## 2. Stan na dziś

**Działa:**
- Kernel bootuje z ST-RAM (0x80000), kompozytor + pulpit + belka statusu działają.
- SysInfo rozpakowuje się (HWAM+PFX), dostaje TPA w TT-RAM, żyje jako task
  i wykonuje ~22 wywołania GEM (AES + pełna inicjalizacja VDI).
- GEMDOS działa poprawnie (Mshrink/Malloc), VDI otwiera stację roboczą.
- Klawiatura: SysInfo reaguje na klawisze (echo), czyli Cconin wraca.
- AES `op=120` ma tymczasowy stub sukcesu.
- `v_gtext` rysuje glify do powierzchni VDI (choć UI SysInfo jeszcze się nie
  pojawia, bo program wychodzi wcześniej).

**Nie działa / blokuje:**
- Po pierwszym wciśnięciu klawisza SysInfo robi VDI `101` (v_clsvwk) +
  AES `19` (appl_exit) i wychodzi — nie dochodzi do `wind_create`/`form_do`
  ani do pętli `evnt_multi`.
- Okno SysInfo jest widoczne w kompozytorze, ale czarne (widać tylko echo
  klawisza, np. `b_`).
- Brakuje większości AES: `wind_*`, `objc_*`, `rsrc_*`, `form_*`, `menu_*`.

---

## 3. Jak zbudować i uruchomić

Budowa (w katalogu `commander-v9`):
```
make build/system/KERNEL.SYS
python3 tools/install_boot_sys.py work/work.img build/system/KERNEL.SYS --directory SYSTEM/KERNEL
```

Uruchomienie Hatari (widoczne okno; BIOS z `files-pasted-by-the-user-projekt`,
bo `commander-v9/build/bios_mste.bin` jest ACSI-only):
```
rm -f /tmp/asf_recon_fifo
NOACSI=""; for id in 0 1 2 3 4 5 6 7; do NOACSI="$NOACSI --acsi $id=none"; done
atari-030-port/hatari-tf536 --machine megast --cpulevel 3 --cpuclock 32 \
  --memsize 4 --ttram 64 --tf536 on --graphics-card vofa --gfx-chip et4000 \
  --mono --zoom 2 --fpu 68882 $NOACSI \
  --ide-master commander-v9/work/work.img \
  --tos files-pasted-by-the-user-projekt/build/bios_mste.bin \
  --cmd-fifo /tmp/asf_recon_fifo
```
Uruchamiać jako **background job** i czytać `job_output`.

**Uwaga:** jeśli w logu jest `ERROR: Locking IDE HD file for writing failed`,
to stary Hatari trzyma `work.img`. Zabić wszystkie joby, odczekać ~4 s,
uruchomić ponownie.

---

## 4. Co już zrobiono

### Sesja 9 — Krok 0: diagnostyka
- Zrzut całego TPA przez `hatari-debug savebin /tmp/tpa.bin 0x1000 0x7f000`.
- Ustalono, że przy małym TPA (`[0x1000,0x80000)`) pakowacz rozpakowuje tylko
  część tekstu, program wchodzi w zera, potem w swoje dane i ginie (Bus/Illegal).
- Dodano `kernel_fault_usp` do raportu błędu.

### Sesja 10 — VDI + pamięć
- **TPA w TT-RAM**: `system/tos_prg.c` `TOS_TPA_BASE=0x04000000`,
  `TOS_TPA_TOP=0x05000000`; `system/kernel_memory.c` obcina pulę TT do
  `0x04000000` (górne 16 MB dla persony TOS). Efekt: SysInfo przestał się
  wykrzaczać i zaczął wołać GEM.
- **Tablica opcodów VDI była błędna** (v_opnvwk było 1 zamiast 100 itd.).
  Poprawiona wg tabeli VDI (jmptb1 1..39, jmptb2 100..134).
- `v_opnvwk` zwraca prawidłowy uchwyt; dołożony minimalny VDI (atrybuty,
  `vqt_attributes`, `vq_extnd`, `vqt_extent`, `vs_clip`, ...).
- **`Malloc(-1)`** ma zwracać rozmiar największego wolnego bloku, nie alokować.

### Sesja 11 — ABI GEMDOS (przyczyna kroku 0)
- **EmuTOS `bdos/rwa.S` `_enter` czyta numer funkcji z wierzchołka stosu
  użytkownika** (`(usp)`), nie z `d0`. `osif()` robi `fn = pw[0]`.
- Nasz `asf_tos_trap1/13/14` brał `d0`. Dla SysInfo `d0=0` przy pierwszym
  `Malloc(-1)`, więc kernel widział `Pterm0` i kończył program od razu.
- Poprawka: `system/tos_entry.S` — trap1/13/14 robią
  `moveq #0,%d0; move.w (%usp),%d0`; `system/tos_prg.c` `tos_args()`
  zwraca `usp+2`.
- Dodano `tos_exit_usp` / `tos_exit_stack[16]` (zrzut stosu przy Pterm).

### Sesja 12 — wejście i widoczne okno
- **Klawiatura**: `tos_input_pop` (= `kernel_user_terminal_getchar`) zwraca
  już ZDEKODOWANE ASCII, a `tos_key_ascii()` oczekiwał surowego scancode —
  każdy klawisz był porzucany. Teraz `tos_poll_input()` przyjmuje zdekodowany
  znak bezpośrednio (gdy nie ma go w mapie scancode'ów).
- `tos_getkey()` w pętli woła `tos_poll_input()` (program chodzi z IPL 7, więc
  Timer C nie może wypełnić kolejki). Weryfikacja: wstrzyknięte klawisze
  dochodzą do `tos_rawlog`; licznik GEM rośnie 22 -> 24.
- **SysInfo jest widoczne w oknie kompozytora** i reaguje na klawisze.
- AES `op=120` — tymczasowy stub sukcesu (`intout[0]=1`).
- `v_gtext` — rysuje glify (`font_glyph`) do `vdi_target`.

---

## 5. Architektura persony TOS

Pliki:
- `system/kernel_main.c` — gałąź GEM (desktop session 2, okno 0x544f53,
  `kernel_tos_run_program("\SYSTEM\TEST\SYSINFO.PRG")`).
- `system/tos_entry.S` — wejście w tryb użytkownika (`asf_tos_enter`),
  stuby trapów (trap1/2/13/14, Line-A, błędy) i `asf_tos_exit`.
- `system/tos_prg.c` — loader .PRG, TPA, GEMDOS/BIOS/XBIOS, AES/VDI dispatch,
  konsola TOS, kompozycja, okno.
- `system/tos_vdi.c` + `include/tos_vdi.h` — własne VDI (bez kodu GPL).
- `include/font.h`, `ui/font.c` — `font_glyph(char, tall)` (8x8 / 8x16).

ABI (zweryfikowane w źródłach EmuTOS/MagiC — **wzorce, nigdy nie kopiujemy kodu**):
- **trap #1/#13/#14**: `nr_funkcji` = WORD na stosie użytkownika (`(usp)`),
  argumenty wyżej.
- **trap #2 (GEM)**: `d0 = 0xC8` (AES) lub `0x73` (VDI), `d1` = blok:
  - AESPB (6 wskaźników): `contrl, global, intin, intout, addrin, addrout`
  - VDIPB (5 wskaźników): `contrl, intin, ptsin, intout, ptsout`
  - opcody AES i VDI się nakładają (np. 100 = wind_create / v_opnvwk), więc
    o dispatchu decyduje selektor `d0`.
- Wejście programu: `p_hitpa = mem+total`, stos `p_hitpa-58`, basepage
  na `4(sp)`, `a0=0`, `a4=p_bbase`, `a5=p_dbase`, IPL 7.

Stan logowania w `tos_prg.c` (bardzo przydatne):
- `tos_aes_log[64*16]` + `tos_aes_log_n` — każde wywołanie GEM:
  `[0]=is_aes, [1..6]=contrl[0..5], [7..10]=intin[0..3], [11..14]=intout[0..3],
  [15]=result`.
- `tos_call_log[257]` — GEMDOS: `[0]=liczba`, potem `(fn<<16)|r`.
- `tos_exit_usp`, `tos_exit_stack[16]` — stos przy Pterm.
- `tos_rawlog[16]`, `tos_rawlog_n` — ostatnie surowe bajty klawiatury.

---

## 6. Jak diagnozować (Hatari)

Kanał: `--cmd-fifo /tmp/asf_recon_fifo`. **Do debuggera trzeba pisać
`hatari-debug <cmd>`** (samo `d`/`r` jest odrzucane).

Przykłady:
```
printf 'hatari-debug savebin /tmp/x.bin 0x1000 0x7f000\n' > /tmp/asf_recon_fifo
printf 'hatari-debug d 0xc8710\n' > /tmp/asf_recon_fifo      # podglad pamieci
printf 'hatari-debug r\n' > /tmp/asf_recon_fifo              # rejestry/PC
printf 'hatari-debug screenshot /tmp/s.png\n' > /tmp/asf_recon_fifo
printf 'hatari-event keypress a\n' > /tmp/asf_recon_fifo     # wstrzykniecie klawisza
```

**Symbole przesuwają się przy każdym buildzie** — zawsze brać adresy z:
```
m68k-elf-nm -n build/system/kernel.elf | grep -E 'tos_aes_log|tos_call_log|tos_exit|kernel_fault'
```
Orientacyjnie z ostatniego builda (113922 B):
`tos_aes_log_n=0xc670c`, `tos_aes_log=0xc6710`, `tos_call_log=0xc84ea`,
`tos_exit_usp=0xc7016`, `tos_exit_stack=0xc6fd6`, `kernel_fault_pc=0x9d268`,
`tos_active=0xc8414`.

Zrzut TT-RAM (obraz SysInfo): `savebin /tmp/si.bin 0x04000000 0x40000`.
Basepage: `0x04000000` (p_lowtpa), `0x04000100` (p_tbase).

---

## 7. Co zostało — jak ugryźć resztę (priorytety)

### Krok A — ustalić, dlaczego SysInfo wychodzi po pierwszym klawiszu
To najważniejsze. Konkret:
1. Dump `tos_aes_log` + `tos_call_log` po klawiszu (mamy już: 22 -> 24,
   ostatnie to VDI 101 v_clsvwk i AES 19 appl_exit).
2. Zdemontować binding AES SysInfo w obrazie TT-RAM wokół
   `0x04010120-0x040101E4` (`moveq #120,d0`, `moveq #124,d0`) — to funkcja
   biblioteki, która woła op=120 z dwoma buforami `addrin`. Ustalić, czym jest
   op=120 (kandydat: rsrc_load/attach) i co ustawia `intout[0]`.
3. Sprawdzić, czy SysInfo pokazuje `form_alert`/komunikat błędu
   (może brak zasobu). Jeśli tak — dostarczyć zasób / zwrócić sukces.
4. Celem jest dojście do `wind_create` + `wind_open` + `evnt_multi`.

### Krok B — dokończyć minimalny AES (żeby SysInfo narysowało UI)
Kolejność wg tego, co SysInfo faktycznie woła (log!):
- `graf_handle` (już jest), `wind_create`=100, `wind_open`=101,
  `wind_get`=104, `wind_set`=105, `wind_close`=102, `wind_delete`=103.
- `wind_update`=107 (BEGIN/END UPDATE — można no-op).
- `form_do`=50, `form_center`=54, `form_dial`=51, `form_alert`=52.
- `objc_draw`=42 (chodzi po drzewie obiektów i woła VDI!), `objc_find`=43,
  `objc_offset`=44, `objc_edit`=46, `objc_change`=47, `objc_add`=40.
- `rsrc_load`=90, `rsrc_free`=91, `rsrc_gaddr`=92, `rsrc_saddr`=93,
  `rsrc_obfix`=94.
- `menu_bar`=30, `menu_ienable`=32, `menu_tnormal`=33, `menu_text`=34.
- `evnt_multi`=25 / `evnt_mesag`=23 / `evnt_keybd`=20 / `evnt_button`=21 /
  `evnt_timer`=24 (pętla zdarzeń pod multitaskingiem).
- `appl_write`=12, `appl_find`=13 (jeśli używa).

Wzorzec implementacji AES: `asf_tos_aesvdi()` w `tos_prg.c`
(`tos_aes_dispatch`), plus `tos_vdi_call()` w `tos_vdi.c`. Geometrię okien
już trzyma `aes_geom[8][4]`.

### Krok C — powiązać AES okna z kompozytorem
Obecnie persona TOS dostaje jedną powierzchnię przez
`kernel_tos_use_surface(kernel_wm_app_surface(0x544f53), 80)`. Docelowo:
`wind_create/wind_open` powinno tworzyć okno WM (nadawać mu powierzchnię i
pozycję), a rysowanie VDI ma trafiać w tę powierzchnię. API kompozytora:
`kernel_wm_app_surface(pid)` w `system/kernel_wm.c` (rejestruje okno
8,8,400,294 i zwraca 32000-bajtowy bufor). Trzeba to rozszerzyć o tworzenie
okien per-handle.

### Krok D — multitasking i weryfikacja
- Gdy SysInfo czeka w `evnt_multi`, kernel ma obsługiwać inne sesje (pętla
  główna + belka już działają). Sprawdzić drugą sesję (shell) w trakcie.
- Zrobić zrzuty: okno SysInfo z widocznym UI + druga sesja żywa.

### Zasady (od użytkownika)
- **Backup przed masowymi zmianami.**
- **Jedna zmiana na raz, potem pomiar** (build + zrzut z Hatari).
- EmuTOS/MagiC to GPL — używamy tylko jako wzorca zachowania, **nigdy nie
  kopiujemy kodu**.
- Hatari zawsze uruchamiany widocznie.

---

## 8. Pułapki

- `moveq` jest tylko dla Dn — `move.l #0,%a0` (nie `moveq #0,%a0`).
- 68000-safe: `move.l %a0,%usp`; odczyt USP tylko przez rejestr adresowy
  (`move.l %usp,%a0`).
- Zmiana `tos_entry.S` bez przebudowy `tos_entry.o` potrafi zostawić stary
  obiekt — `make` to ogarnia, ale sprawdzać `ls -la build/system/tos_entry.o`.
- Pamięć: TPA persony TOS jest w TT-RAM; `_membot=0x04000000`,
  `_memtop=0x05000000`, `p_hitpa=0x05000000`.
- Gdy Hatari nie reaguje na fifo — sprawdzić `tail /tmp/runXX.log` (często
  blokada `work.img` przez inny proces).

---

## 9. Backupy

- `backup-stram-20260921-1330/`, `backup-recon-20260921055136./`,
  `backup-ttram-20260921-1245/`, `commander-v9/work/work.img.bak-recon`.
- Plik `commander-v9/docs/PLAN-GEM-KOMPATYBILNOSC.md` zawiera dziennik
  Sesji 9-12 z pełną narracją i zrzutami.


---

## 10. Sesja 13 — przełamanie: SysInfo tworzy okno w kompozytorze

### Kluczowa korekta: tablica opcodów AES była błędna
Źródło prawdy: `atari-030-port/hatari-src/src/vdi.c`, tablica `AESName_10`.
Poprawne numery (nie te z sekcji 7!):
- `fsel_input`=90, `fsel_exinput`=91
- `wind_create`=100 ... `wind_new`=109
- `rsrc_load`=110, `rsrc_free`=111, `rsrc_gaddr`=112, `rsrc_saddr`=113,
  `rsrc_obfix`=114, `rsrc_rcfix`=115
- `shel_read`=120, `shel_write`=121, `shel_get`=122, `shel_put`=123,
  `shel_find`=124, `shel_envrn`=125
- `appl_getinfo`=130

Op=120 to więc `shel_read`, a nie żadne "create/attach". Op=124 to `shel_find`.

### Cztery realne błędy naprawione (jedna zmiana → pomiar)
1. **trap #2 AES nie przekazywał `global`/`addrin`/`addrout`.**
   `system/tos_entry.S` teraz czyta cały AESPB {contrl,global,intin,intout,
   addrin,addrout} i podaje go do `asf_tos_aesvdi` (nowa sygnatura z
   `global,addrin,addrout`). `appl_init` (op 10) zapisuje blok AESGLOBAL:
   `ap_version=AES_VERSION`, `ap_count`, `ap_id`, `ap_planes`.
   Bez tego SysInfo czytał `global[0]==0` i wychodził z komunikatem
   "DON'T PUT SYSINFO IN THE AUTO FOLDER".
2. **XBIOS `Supexec` (fn 38) nie był zaimplementowany.**
   SysInfo czyta `global[0]` przez Supexec z callbackiem w d3. Dodano
   `asf_tos_supexec(fn, user_d3)` w `tos_entry.S` i `case 38` w
   `asf_tos_xbios` (zapisany d3 użytkownika jest pod `frame-48`).
   Bez tego Supexec zwracał 0 i check znów wypadał.
3. **`Mfree` był pusty (no-op), a `Malloc` to bump-allocator.**
   SysInfo robi `Malloc(-1)`, potem `Malloc(free-X)`, potem zwalnia i
   realokuje. Zaimplementowano pierwszy-dopasowany stertę z nagłówkami
   bloków i scalaniem (`tos_malloc`/`tos_mfree`/`tos_largest_free`;
   `TOS_HEAP_SIZE`=4 MB). Bez tego SysInfo pokazywał
   "Help not enough Memory !! Use a Screen mode with less Screenmemory."
4. **`shel_read`(120)/`shel_find`(124)** wypełniają bufory `addrin[]`
   ścieżką programu (`tos_cmdline`, ustawiane w `kernel_tos_run_program`).

### Wynik (Hatari, ten sam config)
Trace GEM pokazuje teraz 30 wywołań:
`appl_init, graf_handle, v_opnvwk + pełny zestaw atrybutów VDI, vqt_attributes,
shel_read, graf_handle, wind_get(0,4), graf_mouse(0), wind_create,
wind_open, wind_update(3), graf_mouse(0x100), graf_mouse(0x101)`.
- **Okno SysInfo jest tworzone i otwierane w kompozytorze** — widoczny pasek
  tytułu na pulpicie + belka statusu (zrzut: `docs/screenshots/`).
- Klient okna jest nadal czarny; po `graf_mouse(0x101)` SysInfo przestaje
  wołać GEM i `tos_active` wraca do 0 (okno zamarza, widać echo klawisza).
- `tos_memlog[128]`/`tos_memlog_n` logują rozmiary Malloc (diagnostyka).

### Co dalej (priorytety)
1. Ustalić, czy SysInfo po `graf_mouse(0x101)` **wychodzi, faultuje, czy
   blokuje się** (breakpoint na `asf_tos_exit`, `asf_tos_fault`, stan taska).
2. `wind_get(0,4)` w naszym AES zwraca 0 (handle 0 odrzucany) — sprawdzić, czy
   SysInfo nie oczekuje tu sensownej odpowiedzi.
3. Zaimplementować `objc_draw` (42) + `rsrc_*` + `v_gtext` w pętli, żeby
   klient przestał być czarny.
4. `wind_get`/`wind_set` pól (WF_WORKXYWH itd.), `evnt_multi`/`evnt_mesag`
   + wysyłanie komunikatów WM_REDRAW do aplikacji.
5. Dalej: Krok C (okna AES ↔ powierzchnie kompozytora) i Krok D (multitasking).

### Pliki zmienione w Sesji 13
- `system/tos_entry.S` — pełny AESPB w trap #2, `asf_tos_supexec`.
- `system/tos_prg.c` — AESGLOBAL w appl_init, shel_read/shel_find, heap z
  free, XBIOS 38, `tos_cmdline`, `tos_memlog`.
- Backup przed zmianami: `backup-20260921-201041/`.


---

## 11. Sesja 14 — SysInfo dochodzi do pętli zdarzeń; rysuje (niewidocznie)

### Kolejne błędy naprawione
5. **Divide-by-zero w kalibracji CPU zabijał program.** SysInfo kopiuje
   procedurę kalibracji do sterty i uruchamia przez XBIOS Supexec; mierzy
   `_hz_200` (0x4BA) wokół pętli opóźnienia. Nasz timer jest **pollingowy**
   (TIMER.SYS, "no interrupt"), więc `_hz_200` w ogóle nie chodzi → dzielenie
   przez zero → `asf_tos_fault` kończył program. Teraz vector 5 (divide-by-zero)
   jest **nie-fatalny**: CPU zostawia PC na następnej instrukcji, więc `rte`
   wznawia kalibrację z nieaktualnym wynikiem (commit w `asf_tos_fault`).
   (Docelowo: utrzymywać `_hz_200` i przerwać maskowanie IPL 7.)
6. **Brak resourców/obiektów AES.** Dodano w `tos_prg.c`:
   - `rsrc_gaddr` (112): R_TREE → `ap_ptree[index]` (AESGLOBAL+10),
   - `rsrc_obfix` (114): `addr_in[0]`=tree, `int_in[0]`=obj; `aes_obfix()`
     zamienia spakowane współrzędne (x×8,y×16,w×8,h×16) na piksele,
   - `form_center` (54), `form_dial`/`form_alert` (51/52),
   - `objc_draw` (42): `aes_obj_draw()` chodzi po drzewie OBJECT (24 B),
     rysuje G_BOX/BOXTEXT/BUTTON/TITLE i tekst przez nowe
     `tos_vdi_fill_rect`/`tos_vdi_text` (`tos_vdi.c`, w `tos_vdi.h`).
   - Walk ma **budżet 4096 węzłów** — drzewo było cykliczne i bez tego
     `aes_box_walk` rekursywnie się zapętlał.
7. **v_justified (GDP opcode 11 / subcode 10) był traktowany jako v_bar.**
   Opcode 11 to dispatcher GDP; `v_bar` to subcode 1. SysInfo rysuje **cały
   tekst** przez `v_justified`, a jego biblioteka podaje string jako **jedną
   16-bitową literę na słowo**, po dwóch słowach nagłówka. Naprawione w
   `tos_vdi.c`; powierzchnia aplikacji ma teraz treść (1303 niezerowych
   bajtów), wcześniej 0.

### Stan obecny (zweryfikowany)
- Trace: ~3721 wywołań AES; ostatnie to pętla `evnt_multi` (25) +
  `graf_mkstate` (79). SysInfo **żyje i mruga** w swojej pętli zdarzeń.
- `app_surface` (0xacb96) zawiera narysowany tekst (np. "MEMORY").
- **ALE okno nie jest widoczne**: `tos_window_active`=0 i **`wm.count`=0**;
  na ekranie widać shell. Przyczyna: `kernel_tos_set_video(..., blit_mono=0)`
  → `tos_open_window()` zwraca 0 (wymaga `vid_blit_mono`), a dodatkowo
  `kernel_wm_console_present()` przy ponownej rejestracji konsoli robi
  `wm.count=1` (kernel_wm.c:327), **kasując okno aplikacji** z listy WM.
- `app_surface_used[0]=1`, pid=0x544F53, więc powierzchnia jest zarejestrowana;
  brakuje jej okna w WM i blitowania.

### Następny krok (Krok C)
1. `kernel_tos_set_video` ma dostać realny `blit_mono` (albo `tos_open_window`
   ma nie wymagać go, gdy powierzchnia jest wprost w kompozytorze), żeby
   `tos_window_active`=1 i `tos_composite_tick` blitował powierzchnię.
2. `kernel_wm_console_present` nie może nadpisywać `wm.count`/wpisów innych okien
   (zachować istniejące okna przy rejestracji konsoli). To jest właściwy
   "Krok C": okno AES ↔ powierzchnia kompozytora.
3. Potem `evnt_multi`/WM_REDRAW, żeby SysInfo rysował po odsłonięciu okna,
   i zrzut UI + żywy shell (Krok D).

### Pliki zmienione w Sesji 14
- `system/tos_prg.c` — vector 5 nie-fatalny, rsrc_*/form_*/objc_draw,
  `aes_obfix`/`aes_obj_draw`/`aes_box_walk`.
- `system/tos_vdi.c`, `include/tos_vdi.h` — `tos_vdi_fill_rect`,
  `tos_vdi_text`, v_justified (GDP 11/10) + diagnostyka `tos_vdi_just_*`.


---

## 12. Sesja 15 — SysInfo WIDOCZNE w oknie kompozytora (Krok C przełamany)

### Co naprawiono
8. **Widok pulpitu był schowany.** Menedżer sesji
   (`ksessions_pump` → `kgfx_session_view(kernel_user_desktop_visible())`)
   chował pulpit, gdy bieżącą sesją był shell, więc wyświetlany był bufor
   konsoli, a nie kompozytora. W `kernel_main` (gałąź GEM) po
   `kgfx_enter_desktop()` dodano `kgfx_session_view(1)`.
9. **Kompozytor nie chodził w trakcie działania persony.** TOS program działa
   synchronicznie wewnątrz `asf_tos_enter`, więc pętla główna jądra (a z nią
   `kernel_wm_frame_tick`) jest zablokowana przez cały czas życia programu.
   W `asf_tos_aesvdi` po `evnt_multi` (AES 25) co 16. wywołanie robimy
   `kernel_wm_damage_all()` + `kernel_wm_frame_tick(++tos_wm_tick)`, więc okno
   aplikacji faktycznie trafia na ekran, gdy program mruga w pętli zdarzeń.

### Wynik (zweryfikowany zrzutem)
- Okno SysInfo (8,8,400,294) **jest rysowane w kompozycji**: pasek tytułu,
  belka statusu systemu, treść strony tytułowej SysInfo
  ("COPYRIGHT (C) 1992/93 HWAM, VERSION", "MODE: 24 BIT", "MEMORY").
- Widać realną treść UI, nie czarny ekran. Zrzut:
  `docs/screenshots/session15-sysinfo-ui.png`.
- Tekst tytułu jest częściowo zniekształcony (prawdopodobnie efekt cienia /
  podwójnego rysowania i uproszczonego `v_justified`), a wstrzyknięte
  klawisze nie zmieniają jeszcze strony.

### Co dalej
1. Dopracować `v_justified`: justowanie/spacings i ewentualny podwójny rys
   (cień) — stąd zniekształcony tytuł.
2. Wejście: klawisze nie zmieniają strony — sprawdzić, czy SysInfo czeka na
   `evnt_multi` z `MU_KEYBD`/`MU_MESAG`; dostarczyć zdarzenia i WM_REDRAW,
   żeby przeszedł do głównej strony (F1..F10).
3. Zweryfikować multitasking: shell (sesja 3) żyje równolegle; zrobić zrzut
   przełączenia sesji (Krok D).

### Pliki zmienione w Sesji 15
- `system/kernel_main.c` — `kgfx_session_view(1)` w gałęzi GEM.
- `system/tos_prg.c` — pompowanie kompozytora z `evnt_multi`.


---

## 13. Sesja 16 — tekst czytelny, realny zegar, zdarzenia

### Naprawione
10. **Tekst był lustrzany i odwrócony.** `font_8x8`/8x16 są w kolejności
    **LSB-first**, a `v_justified` rysowało `0x80>>col`. Dodatkowo biblioteka
    SysInfo podaje string do `v_justified` w odwrotnej kolejności (jej
    origin jest na prawym brzegu). Po ustawieniu `1u<<col` i rysowaniu od
    ostatniego znaku tekst jest czytelny:
    "SYSINFO COPYRIGHT (C) 1992/93 HWAM, VERSION 8.36".
11. **`evnt_multi` (25) dostarcza klawisze.** Wypełniane są oba pola
    (`int_out[1]` i `int_out[5]`), bo różne bindingi czytają różnie.
    Klawisz faktycznie dochodzi do persony (`tos_rawlog=0x61` po 'a').
12. **`Tgettime` (0x2C) było zamrożone** — SysInfo odpytuje je w pętli
    (call_log: setki `0x2C -> 0x6000`). Teraz zegar jest syntezowany z
    licznika **MFP Timer C** (`0xfffffa23`, 200 Hz, działa niezależnie od
    IPL 7), odpytywanego przy każdym GEM/GEMDOS trap. Start 12:00:00.
13. Do `evnt_multi` dołożony jednorazowy **WM_REDRAW** do bufora
    `addr_in[0]` (MME_PBUFF), żeby aplikacja przemalowała stronę.

### Stan
- Okno SysInfo widoczne, tekst **czytelny** (zrzut:
  `docs/screenshots/session16-readable-ui.png`).
- SysInfo żyje w pętli zdarzeń (~3900 wywołań GEM; `evnt_multi`,
  `graf_mkstate`, `Tgettime`); `tos_active=1`.
- Klawisze dochodzą, ale nie zmieniają strony; powierzchnia nadal pokazuje
  tylko nagłówek/2 linie — trzeba ustalić, czy SysInfo czeka na konkretny
  komunikat (WM_REDRAW z właściwym prostokątem / WM_NEWTOP) czy na
  `wind_get`/`graf_mkstate` z sensowną odpowiedzią.

### Następny krok
1. `graf_mkstate` (79): zwrócić pozycję/przyciski myszy w intout.
2. `wind_get`/`wind_set` z prawdziwym handle i polami (WF_WXYWH...).
3. Sprawdzić, czy WM_REDRAW trafia do SysInfo (breakpoint na handlerze
   `evnt_multi` i podgląd `addr_in[0]`), ew. wysyłać go cyklicznie.
4. Zrzut pełnego UI + żywy shell (Krok D).

### Pliki zmienione w Sesji 16
- `system/tos_vdi.c` — bit order i kolejność znaków w `v_justified`.
- `system/tos_prg.c` — klawisze i WM_REDRAW w `evnt_multi`, zegar MFP.


---

## 14. Sesja 17 — wind_get/graf_mkstate; diagnoza „dlaczego nie ma pełnej strony"

### Dodane
14. **`graf_mkstate` (79)** zwraca `intout[0..3]` = x,y,buttons,shift (na razie
    stałe 320,200,0,0) — SysInfo woła je w każdej iteracji pętli.
15. **`wind_get` (104)** obsługuje pola: `WF_KIND`(1), `WF_NAME`(2),
    `WF_INFO`(3), `WF_WXYWH`/`WF_CXYWH`(4..7), `WF_HSLIDE/VSLIDE`(8/9),
    `WF_TOP`(10) oraz **`WF_FIRSTXYWH`(11)/`WF_NEXTXYWH`(12)** (kursor
    `aes_wg_first`: najpierw prostokąt klienta, potem zero). Handle 0 =
    desktop (0,0,640,400); handle 1..8 = `aes_geom`.

### Diagnoza (trace, 3419 wywołań)
Rozkład AES: `graf_mkstate` 784, `evnt_multi` 389, `rsrc_obfix` 74,
`graf_mouse` 11, `rsrc_gaddr` 4, `form_center` 4, po jednym
`wind_create/open/update/get`, `shel_get`, `appl_init`.
**Brak `objc_draw` (42), `form_do` (50), `menu_*`** — SysInfo siedzi w pętli
zdarzeń i nie rysuje głównej strony. Klawisze i kliknięcia myszą (wstrzykiwane)
nie zmieniają ani powierzchni, ani zrzutu.

### Wniosek / następny krok
SysInfo czeka na konkretne zdarzenie lub podsystem, którego nie dostarczamy:
- **mysz**: `graf_mkstate` i `evnt_multi` nie raportują przycisków; podłączyć
  `kernel_user_mouse_event`/`ksessions_mouse_*` do `intout` (MU_BUTTON).
- **timer**: `evnt_multi` z MU_TIMER powinno po zadanym czasie zwrócić
  MU_TIMER; policzyć od `_hz_200`/MFP.
- **WM_REDRAW**: zweryfikować, czy trafia (podgląd `addr_in[0]`), i ew.
  wysyłać go cyklicznie z prostokątem klienta.
- Sprawdzić, czy główna strona SysInfo nie jest rysowana poza widocznym
  klientem 400x294 (powierzchnia 640x400 vs okno) — ewentualnie dać większe
  okno.

`wind_get`/`graf_mkstate` są już w `system/tos_prg.c`.


---

## 15. Sesja 18 — ile zdarzeń faktycznie dostarczamy

### Pomiar (liczniki w `tos_prg.c`)
Po ~16 s pracy i wstrzyknięciu 'a', space, F1:
- `tos_evmulti_n` = 325 (wywołań `evnt_multi`),
- `tos_evmes_n` = 1 (jednorazowy WM_REDRAW),
- `tos_evkbd_n` = **1** (tylko jeden klawisz trafił do aplikacji, mimo 3
  wstrzyknięć).

Czyli mechanizm dostarczania działa, ale SysInfo **nie reaguje** na klawisz —
nie zmienia powierzchni ani zrzutu. Powierzchnia nadal pokazuje nagłówek
("SYSINFO COPYRIGHT ... 8.36") + etykiety stron przy dolnej krawędzi, środek
pusty.

### Hipotezy do sprawdzenia (w kolejności)
1. **Format słowa klawisza.** Kolejka TOS trzyma `(scancode<<16)|ascii`; dla
   klawiszy zdekodowanych (nasza ścieżka `kernel_user_terminal_getchar`)
   scancode=0, więc `evnt_multi` dostaje np. 0x20 zamiast 0x3920. GEM oczekuje
   `(scancode<<8)|ascii`. Dorobić mapę ASCII→scancode i pakować.
2. **Brak zdarzeń myszy/timera**: MU_BUTTON (graf_mkstate/evnt_multi nie
   raportują przycisków) i MU_TIMER. Podłączyć stan myszy z jądra.
3. **Środek strony może być rysowany poza klientem** (powierzchnia 640x400 vs
   okno 400x294) albo dopiero po konkretnym WM_REDRAW z prostokątem klienta.
4. Sprawdzić, czy SysInfo nie czeka na `form_do`/`objc_draw` — na razie ich
   nie woła (trace: brak 0x2A/0x32/0x33).

### Pliki zmienione w Sesji 18
- `system/tos_prg.c` — liczniki `tos_evkbd_n/tos_evmes_n/tos_evmulti_n`
  (diagnostyka).


---

## 16. Sesja 19 — SysInfo rysuje GŁÓWNĄ stronę

### Obserwacja
Po ok. 80 s od startu (skan startowy SysInfo) okno pokazuje **główną stronę
SysInfo** — listę wektorów: kolumna etykiet "01:".."10:" i kolumna wartości
("0A:", "9B:", "8A:", ...), plus nagłówek i stopka pomocy. Powierzchnia ma
~6 tys. niezerowych bajtów. Zrzut:
`docs/screenshots/session19-main-page.png`, pełny render:
`/tmp/surf21_full.png`.

Czyli: okno żyje, kompozytor je pokazuje, a SysInfo przeszedł z ekranu
tytułowego do właściwej strony. Wcześniejsze "brak objc_draw" nie było
blokadą — SysInfo rysuje tekst bezpośrednio przez VDI.

### Uwagi
- Tekst jest w większości czytelny, ale część wierszy nadal wygląda
  lustrzanie (dwie kolumny, część etykiet). To kwestia `v_justified`:
  biblioteka SysInfo podaje string w odwrotnej kolejności i/lub wyrównuje
  w prawo. Do dopracowania: wykryć kierunek per-string zamiast
  bezwarunkowego odwracania.
- Doszły syntetyczne zdarzenia myszy (graf_mkstate/evnt_multi) — być może
  to one (lub zegar MFP) pozwoliły przejść dalej; do potwierdzenia.

### Następny krok
1. Dopracować kierunek tekstu w `v_justified` (per-string), żeby etykiety
   były czytelne.
2. Potwierdzić reakcję na klawisze/menu (F1..F10 / right mouse) i zrobić
   zrzut innej strony.
3. Zweryfikować multitasking: shell (sesja 3) żyje równolegle (Krok D).


---

## 17. Sesja 20 — kierunek tekstu: pomiar intin

### Pomiar
Zrzut pierwszego wywołania `v_justified` (tytuł) w `tos_prg.c/tos_vdi.c`:
`fpts=[3,11]`, słowa: `00 00 0e 0f 20 'S' 'Y' 'S' 'I' 'N' 'F' 'O' ...`
czyli string w intin jest w **normalnej kolejności** ("SYSINFO ..."), origin
(3,11), a mimo to rysunek „wprost" wychodzi **lustrzany** (render:
"OFNISYS..."). Odwrócenie kolejności znaków daje tekst czytelny.

Wniosek: nasz `vdi_fill` trafia w powierzchnię tak, że oś X jest odbita
względem intencji VDI (font po `1u<<col` jest poprawny, więc odbity jest
kierunek biegu tekstu). Odwrócenie kolejności to kompensuje dla większości
stringów, ale część wierszy SysInfo (etykiety listy) wychodzi nadal
lustrzanie — biblioteka miesza kierunek (prawdopodobnie zależnie od
wyrównania pola).

### Stan końcowy tej sesji
- Główna strona SysInfo (lista wektorów) widoczna w oknie kompozytora;
  `docs/screenshots/session19-main-page.png`.
- Tekst w większości czytelny, część wierszy odbita.
- `tos_active=1`, pętla `evnt_multi`/`graf_mkstate`/`Tgettime`; brak reakcji na
  F1/F2 (strona się nie zmienia).

### Następny krok
1. Ustalić oryginalny kierunek X w VDI (porównać `v_bar`/`v_pline`: jeśli
   prostokąty też są odbite, to naprawić `vdi_fill`/mapowanie zamiast
   odwracać stringi).
2. Reakcja na klawisze/menu (F1..F10) — sprawdzić, czy `evnt_multi` zwraca
   MU_KEYBD i czy SysInfo czyta właściwe pole.
3. Krok D: zrzut żywego shella (przełączenie sesji F10+shift lub
   `ksessions_control`).


---

## 18. Sesja 21 — test odbicia osi X i stan końcowy

### Eksperyment
Zamiast odwracać stringi, odbiłem X raz w `vdi_fill`
(`x0'=w-1-x1; x1'=w-1-x0`) i zdjąłem odwrócenie w `v_justified`.
Wynik: tekst **przesunął się na prawą stronę** (tytuł przy x≈270 zamiast
x≈3), więc VDI jednak używa normalnej osi X, a lustrzany obraz brał się z
czegoś innego. Wróciłem do odwracania stringów w `v_justified` (daje
poprawną pozycję i czytelny nagłówek/stopkę).

Wniosek: pozycja jest dobra przy normalnym `vdi_fill`; odwrócenie stringu to
jedyna zmiana potrzebna dla tekstu, ale biblioteka SysInfo część wierszy
(etykiety listy) podaje w drugą stronę — stąd resztkowe lustro.

### Stan końcowy (stabilny, zrzut identyczny po 90 s)
- Okno SysInfo (8,8,400,294) w kompozytorze pokazuje główną stronę SysInfo
  (lista wektorów), belkę statusu i pasek tytułu.
- Zrzuty: `docs/screenshots/session19-main-page.png` (=
  `session21-final.png`), pełny render powierzchni
  `session19-main-page-surface.png`.
- `tos_active=1`, ~3900 wywołań GEM, pętla `evnt_multi`+`graf_mkstate`+`Tgettime`.
- Shell (sesja 3) działa równolegle; brak zrzutu przełączenia (F10 shift+alt
  nie przełączył w próbie — wymaga wejścia przez kolejkę IKBD jądra).

### Pozostało
1. Kierunek tekstu per-string (etykiety) — najlepiej wykryć z układu
   `intin`/`ptsin` zamiast bezwarunkowego odwracania.
2. Reakcja na F1..F10 / menu.
3. Zrzut żywego shella (przełączenie sesji).


---

## 19. Sesja 22 — kierunek per-wywołanie: brak różnicy

Dodałem w `v_justified` wybór kierunku zależny od wywołania:
`rev = (ptsin[2]==0 && ptsin[3]==0)`. Render powierzchni wyszedł
**identyczny** jak poprzednio (ten sam `sha256`), czyli **wszystkie** wywołania
SysInfo są w trybie baseline (`ptsin[2..3]==0`). Heurystyka jest równoważna
bezwarunkowemu odwracaniu, więc nie zmienia obrazu.

Wniosek: różnica "część wierszy czytelna, część lustrzana" nie wynika z
`ptsin`, tylko z tego, że biblioteka SysInfo część stringów wkłada do
`intin` w normalnej, a część w odwróconej kolejności (prawdopodobnie
right-aligned pola). Aby to rozstrzygnąć, trzeba zdemontować miejsce w
bibliotece, które wypełnia `intin` dla rysowania etykiet/listy.

### Stan (bez zmian)
- Główna strona SysInfo widoczna w oknie, stabilna.
- Zrzut powierzchni: `docs/screenshots/session22-main-page-surface.png`.
- `evnt_multi`/klawisze: aplikacja nie zmienia strony na F1/F2.


---

## 20. Sesja 23 — lustro obalone, tekst poprawny, okno pełnoekranowe

### Główny wniosek: "lustro" było artefaktem pomiaru
Przez kilka sesji zrzuty `app_surface` wyglądały na odbite. Prawda:
adres `app_surface` **zmienia się przy każdej przebudowie**, a zrzuty robiono
pod starą wartością z wcześniejszego builda (`0xad05e`), podczas gdy bieżąca
powierzchnia leżała np. pod `0xad0c2`. Odczyt spod złego adresu dawał
przesunięte wiersze, które wyglądały jak odbicie. **Zawsze brać adres z
`m68k-elf-nm -n build/system/kernel.elf | grep ' app_surface$'` w tym samym
buildzie, który działa w emulatorze.**

Weryfikacja metodyczna:
1. Tabela `font_8x8` (`ui/font.c`) jest **MSB-first** — dowód na
   niesymetrycznych znakach: `C` [7e,66,60,60,66,7e], `F`, `P`, `R`,
   `G`, `J`, `L`, `T` rysują się jako właściwe litery przy bicie 7 =
   lewa kolumna.
2. Dekoder glifów czyta powierzchnię komórkami 8x8 MSB-first i dopasowuje
   bitmapy do tabeli fontu. Na żywym zrzucie dostał czytelne stringi, np.
   `'?? ?YSINFO COPYRIGHT (C) 1992/93 HWAM, VERSION 8.36'` oraz
   `' PRESS RIGHT MOUSE-BUTTON OR SPACE FOR MENU !'`.
3. Render predykcyjny (string rysowany tabelą fontu, przód, bit 7 = lewa
   kolumna) zgadza się z powierzchnią **piksel w piksel** (18 z 6720 pikseli
   różnicy, wyłącznie w pierwszych dwóch komórkach glifów).
4. Zrzut ekranu Hatari == zrzut powierzchni (kompozytor wiernie kopiuje).

### Co faktycznie było zepsute (i naprawione)
- `v_justified` (GDP 11/10) w `system/tos_vdi.c` rysował `1u<<col` i szedł
  po znakach od końca (`rev=(ptsin[2]==0&&ptsin[3]==0)`) — to dawało
  prawdziwe odbicie. Zmienione na `0x80u>>col` i marsz znaków w przód (ta
  sama konwencja co `v_gtext` i `tos_vdi_text`).
- Długość stringu: SysInfo **skleja stringi w puli bez NUL-i**
  (`...SYSTEM INFO:MEMORY:MEMORY POINTE...`), więc skanowanie do zera
  dorysowywało ogon sąsiada. Teraz długość = `contrl[3]-2` (int_in ma dwa
  słowa nagłówka, znaki od `int_in[2]`, jedno na słowo).
- Siatka znaków jest 8 px, ale origin bywa na dowolnej współrzędnej (np.
  x=3, x=407) — dekodery muszą szukać offsetu 0..7.

### Okno: pełny ekran
`kernel_wm_app_surface()` (system/kernel_wm.c) dawał okno 8,8,400,294, a
SysInfo układa UI pod raportowane przez `wind_get` 640x400 — prawa kolumna
wartości i menu F1..F10 były obcinane. Teraz ramka i klient to
`0,0,640,400` (src 0,0), a `flags=3`: bit 0 = widoczne, **bit 1 = bez
dekoracji jądra** (pasek tytułu SysInfo rysuje sam). Dekorator w
`kernel_wm_frame_tick` pomija okna z bitem 1 (`if(flags&2u)continue;`).
Zrzut: `docs/screenshots/session23-fullscreen-ui.png`.

### Inwentaryzacja ścieżek rysowania (nowe diagnostyki)
- Dodany pierścień `v_gtext`: `tos_vdi_gtext[8][12]`,
  `tos_vdi_gtext_pts[8][2]`, `tos_vdi_gtext_ctrl[8][6]`,
  `tos_vdi_gtext_i` (`system/tos_vdi.c`).
- Wynik: **SysInfo w ogóle nie woła `v_gtext`** (`tos_vdi_gtext_i==0`), a
  `v_justified` woła dokładnie 8 razy (`tos_vdi_just_n==8`): tytuł
  (3,11), `PAGE`+cyfra (585/621,33), dwie stopki (8,395)/(400,395),
  `SYSTEM INFO:` (8,37), `MEMORY:` (336,37), `MEMORY POINTERS:` (336,262).
  Reszta strony (wiersze wartości, F1..F10, `HELP:`) idzie inną ścieżką —
  najpewniej przez AES `objc_draw` → `tos_vdi_text` (`aes_dbg_tree` != 0).
- `tos_aes_log` (64 pierwsze wywołania AES/VDI) pokazuje sekwencję startu:
  `appl_init(10)`, `graf_handle(77)`, `v_opnvwk(100)`, zestaw
  `vst_*/vsl_*`, `shel_read(120)`, `wind_get(104)→640x400`,
  `wind_create(100)`, `wind_open(101)`, `wind_update(107)`,
  `graf_mouse(78)`, `rsrc_obfix(114)` x30.

### Multitasking — co działa, co nie
- Kooperacja działa: gdy SysInfo siedzi w pętli `evnt_multi` (32/32
  ostatnich wywołań AES, `tos_aes_trace_n` ~3536), licznik klatek
  kompozytora `wm_frames` rośnie (0x16a→0x17c w ~8 s) — jądro i kompozytor
  żyją, gdy aplikacja TOS czeka.
- **Przełączanie sesji Shift+Alt+F10 nie działa, gdy TOS ma klawiaturę**:
  `ksessions_key()` jest dostępne wyłącznie przez
  `module_api.terminal_key` (moduł SHELL.SYS), więc w sesji pulpitu TOS nie
  ma kto obsłużyć skrótu. `hatari-event keydown shift/alternate + F10` nie
  zmienił sesji. Do zrobienia: hak w jądrze (ścieżka IKBD albo obsługa
  zdarzeń klawiatury AES), który woła `ksessions_control(3,0)` niezależnie
  od modułu shella.

### Następny krok
1. Hak przełączania sesji w jądrze (Shift+Alt+F10) + zrzut żywego shella i
   powrót do SysInfo = dowód multitaskingu w obie strony.
2. Reakcja SysInfo na klawisze (F1..F10): `tos_evkbd_n==0`, czyli
   `evnt_multi` nie dostarcza jeszcze zdarzeń klawiatury z IKBD do aplikacji
   TOS — sprawdzić mapowanie scancode → AES i kolejkę klawiszy.
3. Ewentualnie potwierdzić ścieżkę `objc_draw` przez `tos_aes_log` /
   `aes_dbg_tree`.


---

## 21. Sesja 23b — czym SysInfo faktycznie maluje UI

### Twardy pomiar układu
Zrzut ekranu porównano z **oficjalnym zrzutem SysInfo HWAM 8.34** z Atari Wiki
(`docs/screenshots/session23-reference-vs-live.png`, po lewej referencja,
po prawej nasz ekran). Układ zgadza się 1:1:
- tytuł `SYSINFO COPYRIGHT (C) ... HWAM, VERSION 8.36` u góry po lewej,
- `PAGE 1` u góry po prawej,
- nagłówki `SYSTEM INFO:` (x=8) i `MEMORY:` (x=336) w drugim wierszu,
- `PRESS RIGHT MOUSE-BUTTON OR SPACE FOR MENU !` (x=8) i
  `ADDRESS DISPLAY MODE: 24 BITS` (x=400) u dołu.
Dekoder pozycyjny (dopasowanie bitmap 8x8 do `font_8x8`) czyta te stringi
**w przód i do góry nogami poprawnie** (`'YSINFO COPYRIGHT'`,
`'PRESS RIGHT MOUSE-BUTTON ...'`), a predykcyjny render zgadza się z
powierzchnią piksel w piksel. `app_surface` == zrzut ekranu (0 różnych
pikseli). Wniosek: to, co rysuje nasze VDI, jest poprawne i nieobrócone.

### Kluczowe odkrycie: SysInfo maluje UI sam
Histogramy opkodów (nowe diagnostyki `tos_vdi_opcount[256]`,
`tos_aes_opcount[256]`, `tos_xbios_opcount[256]`) po ~60 s:
- **VDI: tylko 63 wywołania** — `v_pline`(6)=23, `v_justified`(11/10)=8,
  reszta to settery atrybutów + `v_opnvwk`(100)=1, `vro_cpyfm`(109)=**2
  (NIE zaimplementowany!)**, brak `v_gtext`(8).
- **XBIOS: 51 wywołań** — `Getrez`(4)=3, `Supexec`(38)=**47**, `fn 64`=1.
  **Ani razu `Physbase`(2)/`Logbase`(3).**
- **AES**: `evnt_multi`(25)=2016, `rsrc_obfix`(114)=74, `graf_mkstate`(79)=106,
  `objc_offset`(43)=39, `objc_order`(44)=43, `rsrc_gaddr`(112)=4, okna/wind_*.
- Zliczone przez nas ścieżki tekstowe (`v_gtext`, `v_justified`,
  `tos_vdi_text` przez `objc_draw`) mogą narysować najwyżej kilka stringów,
  a ekran jest pełen znaków. **Czyli SysInfo rysuje większość UI sam** — przez
  `Supexec` (własny kod na CPU) i/lub Line-A i/lub bezpośrednie zapisy do
  pamięci ekranu. To dokładnie ten obszar, który wskazałeś (GEM/NVDI/XBIOS).
- Line-A (`asf_tos_linea_op`, tos_prg.c:449) obsługuje op 1/2 (piksel),
  3 (linia), 4 (hlinia), 5 (prostokąt), 6 (wielokąt), 0x0E, 0x0F.
  **Brak op 7/8 (bitblt/blit)** — głównych operacji blitowania, których
  programy GEM używają do szybkiego tekstu. Brak też `vro_cpyfm` (VDI 109).

### Poprawka modelu ekranu
`kernel_tos_use_surface()` ustawiał `tos_screen`/VDI, ale NIE aktualizował
`_v_bas_ad` (0x44E) ani `_screensize` (0x44C) — te wskazywały na framebuffer
jądra. Teraz są spójne z `Physbase()` (`_v_bas_ad=app_surface`,
`_screensize=32000`). Weryfikacja: zrzut przed/po jest identyczny, więc
SysInfo nie czyta `_v_bas_ad` bezpośrednio — ale model jest teraz poprawny.

### Następny krok (konkretnie)
1. Dodać histogram Line-A (op → licznik) i policzyć, czy SysInfo blituje przez
   `0xA000`; jeśli tak — zaimplementować Line-A op 7/8 (bitblt) z poprawną
   semantyką (form/base, maska, tryb) i sprawdzić orientację.
2. Zaimplementować VDI `vro_cpyfm` (109) — SysInfo woła go 2 razy.
3. Sprawdzić, czy SysInfo nie zapisuje bezpośrednio po `Supexec` do wskaźnika,
   który bierze z Line-A/globali — porównać zrzut VRAM z app_surface.
4. Analiza kolejności bring-up MagiC (zlecona podagentowi) — porównać
   inicjalizację ekranu/VDI/AES i uzupełnić braki.


---

## 22. Sesja 23c — raport MagiC, naprawy G1/G5, odbicie okna TOS

### Wyniki pomiarów z sesji 23b/23c
- `A_INIT` (Line-A op 0) jest **wołane raz i dostaje 0**; Line-A nie maluje ani
  piksela (`tos_linea_pixels==0`). Czyli Line-A **nie** jest ścieżką malowania,
  ale brak `A_INIT`/​strony Line-A to realny brak (G3).
- Nasze VDI wykonało **121 220 wypełnień pojedynczych pikseli**; rozbicie po
  opkodach: **op 109 = 108 632 px**, op 6 (`v_pline`) = 9 213, op 11
  (`v_justified`) = 3 375. Op 109 (`vro_cpyfm`) **nie ma case'a**, więc
  atrybucja 108k px jest najpewniej „przypisana" przez nieodświeżony
  `tos_vdi_cur_op` (wypełnienia z `tos_vdi_text`/`tos_vdi_fill_rect` wołanych
  z AES). Do rozstrzygnięcia: osobne liczniki w `tos_vdi_text` i
  `tos_vdi_fill_rect`.
- Po naprawach G1 (work_out) i G5 (0x44C) zrzut jest **identyczny** — SysInfo
  nie używa `work_out[0]/[1]` ani `_v_bas_ad` do układu strony.

### Raport podagenta (MagiC bring-up) — najważniejsze różnice
- **G1 (krytyczne, NAPRAWIONE)**: `v_opnvwk` wpisywało handle do `intout[0]`,
  przesuwając całe `work_out` (program czytał maxx=1, maxy=640). Teraz
  `intout[0]=width-1`, `[1]=height-1`, `[13]=colors-1`, handle do `contrl[6]`.
- **G5 (wysokie, NAPRAWIONE)**: 32-bitowy zapis pod 0x44C nachodził na
  `_v_bas_ad` (0x44E). 0x44C to `sshiftmd` (bajt trybu shiftera), nie
  `_screensize`. Teraz zapis bajtowy `sshiftmd=tos_mode`.
- **G2**: `vq_extnd` zwraca same jedynki; brak `vq_scrninfo` (102,1), które
  zwraca `BYTES_LIN`=80 i `v_bas_ad`. Program rysujący sam nie pozna skoku
  ani bazy ekranu.
- **G3**: brak strony Line-A i `A_INIT` (zwraca 0).
- **G4**: brak VDI 109 (`vro_cpyfm`) i Line-A 7/8 (bitblt/textblt).
- **G6**: `wind_get` `WF_WORKXYWH(4)/CURR(5)/PREV(6)/FULL(7)` zwracają
  prostokąt z `wind_create`; `wind_open` to no-op; brak `wind_calc`.
- **G7**: syntetyczny `WM_REDRAW` ma złe pola (`w=0,h=640`).
- **G8**: brak `_hz_200` (0x4BA) inkrementacji, `_frclock`, `_drvbits`,
  `_p_cookies`, prawdziwego `syshdr` w `_sysbase`.
- **G10**: `graf_handle` nie ustawia `intout[3]=gr_hwbox`, `[4]=gr_hhbox`.

### Odbicie okna TOS (na wyraźną prośbę użytkownika)
Użytkownik widzi okno SysInfo jako **lustrzane odbicie**. Nasze pomiary mówią,
że zawartość `app_surface` jest poprawna (zgodna z referencją), więc dodano
**transform w kompozytorze**: `wm_app_flip` (`kernel_wm.c`), bit 0 = lustro X
(wokół osi pionowej), bit 1 = lustro Y. Iteracje z użytkownikiem:
flip 1 (lustro X) — zła oś; flip 3 (180°) — trzeba zdjąć poziom. **Aktualnie
`2` (tylko pion).** Działa tylko na okna aplikacji (`wm_surface[1..]`), konsola
jądra bez zmian. Weryfikacja pomiarowa: flip 1 = 100% lustro X, flip 3 = 100%
obrót 180°, flip 2 = 100% lustro Y (256000/256000 w każdym przypadku).
Zrzuty: `session23b-flip-x-640.png` (1), `session23c-flip-180.png` (3),
`session23d-flip-y-640.png` (2, aktualny).

### 23d — orientacja i kolory (potwierdzone przez użytkownika)
- `wm_app_flip` = **6**: bit 1 (pion/lustro Y) + bit 2 (inwersja kolorów).
  Użytkownik potwierdził: „teraz jest ok". W TOS jest białe tło i czarne litery
  — inwersja w kompozytorze daje to bez ruszania konsoli jądra.
- Zrzut: `docs/screenshots/session23e-inverted-640.png` (białe tło, czarny
  tekst); różnica względem poprzedniego = 255908/256000 px (pełna inwersja).

### 23e — rozstrzygnięta ścieżka malowania (pomiar)
- Po implementacji VDI 109/110 licznik `tos_vdi_pixels_per_op` **nadal**
  pokazuje 108 632 px przy op 109, a op 6 spadł z 9 213 do 509. Identyczna
  liczba 108 632 przed i po = to **nie** jest wykonanie op 109, tylko
  **nieodświeżony `tos_vdi_cur_op`** w `vdi_fill` wołanym z
  `tos_vdi_text`/`tos_vdi_fill_rect` (AES `objc_draw`).
- Wniosek: **całe widoczne UI rysuje jedno wywołanie AES `objc_draw`**
  (`aes_obj_draw`), a nie VDI. Brakujące wartości/przyciski/ramki trzeba
  dorysować w `aes_obj_draw` (obsługa typów obiektów, wypełnienia G_BOX,
  G_IMAGE/G_ICON przez `vro_cpyfm`, justowanie TEDINFO).
- Przy okazji naprawione/normalne: `v_pline` bierze liczbę punktów
  z `contrl[1]` (fallback `intin[0]`), `v_fillarea` robi prawdziwy
  scanline-fill wielokąta, dodane VDI 109 `vro_cpyfm` / 110 `vrt_cpyfm`
  (z pełnym `addrin` w `tos_vdi_call`) — infrastruktura dla bitmap/ikon.

### 23f — czcionki: pełny zestaw znaków Atari
Użytkownik zasugerował problem czcionek. Ustalenia:
- `ui/font.c` (`font_8x8`) i `include/font8.h` (`emutos_font8[256][8]`) to
  **ten sam font Atari w zakresie ASCII** — porównanie glifów A/S/0/7/M/m
  wypadło identycznie piksel w piksel.
- Różnica: `emutos_font8` ma **256 znaków** (pełny zestaw TOS: strzałki,
  symbole, znaki ramek), a `font_glyph` mapował wszystko poza 32..126 na
  `'?'`. Teraz dla tekstu 8x8 używamy `emutos_font8[code&0xff]` — symbole
  renderują się jak w TOS (widać np. `ØF1:`, `^0:` zamiast `7F1:`).
  Zmiana: 212 px różnicy. Zrzut: `session23f-atari-charset-640.png`.
- Atari ma też font **6x6** (drobny) i **8x16**; VDI wybiera 8x8 gdy
  `V_REZ_VT<400`, inaczej 8x16. Nasz `font_8x16` pokrywa na razie tylko
  ASCII (poza tym `'?'`).
- Żeby SysInfo mógł rysować **własnym** tekstem, musi dostać wskaźnik do
  fontu przez stronę Line-A (`V_FNT_AD`) z `A_INIT` (G3) — tego jeszcze nie ma.

### 23g — strona Line-A + `A_INIT` (G3) — WDROŻONE
- `system/tos_prg.c`: statyczna strona Line-A (`tos_linea_page`, baza 0x400
  w buforze, więc ujemne offsety są adresowalne), wypełniana przez
  `tos_linea_init()` wołane z `kernel_tos_use_surface()`. Pola wg MagiC
  `INCLUDE/LINEA.I`: `PLANES`=1, `WIDTH`=80, `BYTES_LIN`=80,
  `V_REZ_HZ`=640, `V_REZ_VT`=400, `V_CEL_HT`=8, `V_CEL_MX/MY`=79/24,
  `V_COL_BG/FG`=0/1, `V_FNT_AD`→`emutos_font8`, `V_FNT_ND/ST/WD`=32/126/1,
  `V_HID_CNT`=1, `CUR_X/Y`=320/200, `CONTRL/INTIN/PTSIN/INTOUT/PTSOUT`
  → własne bufory, `FBASE/FWIDTH`, `CLIP`=0 + prostokąt 0,0,639,399 oraz
  `DEV_TAB`/`SIZ_TAB`/`INQ_TAB` (max x/y, kolory, planes, CLUT).
- `system/tos_entry.S`: stub Line-A po `A_INIT` wpisuje w zapisane rejestry
  `a0`=base, `a1`=tablica fontów, `a2`=tablica funkcji (C zwraca tylko d0).
  Flaga `kernel_linea_ainit` ustawiana w `asf_tos_linea_op` (op 0).
- `include/font.h` + `ui/font.c`: `font_system_8x8()` zwraca obraz fontu
  (`emutos_font8`) dla `V_FNT_AD`.
- **Weryfikacja na żywo**: `A_INIT` wołane raz (`tos_linea_opcount[0]==1`),
  `kernel_linea_base=0x000cbf4c`, `font=0x0009d3bd`, `tab=0x000cba24`.
  Ekran zmienił się o **3999 px** względem poprzedniego builda — SysInfo
  faktycznie czyta stronę. Zrzut: `session23g-linea-640.png`.
- Uwaga: `tos_linea_pixels==0` — SysInfo nadal **nie** rysuje przez Line-A
  (żadnego op 1..15), więc brakująca treść idzie przez AES `objc_draw`.

### Następny krok
1. Rozbudować `aes_obj_draw` (`system/tos_prg.c`): wypełnienia
   G_BOX/G_IBOX/G_BUTTON, G_IMAGE/G_ICON przez `vro_cpyfm`, justowanie
   i kolory TEDINFO.
3. `wind_get` WF_WORKXYWH/CURRXYWH/PREVXYWH/FULLXYWH (G6) + poprawne pola
   `WM_REDRAW` (G7).
4. `vq_extnd`/`vq_scrninfo` (G2) + reszta sysvarów (G8).

### 23h — co dokładnie brakuje: drzewo obiektów SysInfo
Zrzut drzewa spod `aes_dbg_tree` = `0x0101a066` (42 obiekty):
- obiekt 1..42: **`G_STRING` (typ 28)** co 24 B, x=12, y=40,64,88,...,
  w=32,h=16, `ob_spec` = wskaźniki do stringów co 5 B (etykiety F1..F10).
- obiekt 0 (korzeń): w=436,h=296 (panel!), ale `ob_type` czyta się jako
  **0x7114** — nieprawidłowy typ — więc `aes_obj_draw` nie rysuje ramki panelu.
To najpewniej dokładnie „brak ramek przycisków i ich opisów", o których mówi
użytkownik: same stringi się rysują, ramki nie.
Do ustalenia następnym krokiem: czy `objc_draw` dostał `start`>0 (wtedy
korzeń w ogóle nie jest rysowany przez nas), czy `addrin[0]` jest przesunięte
o kilka bajtów, czy struktura obiektu SysInfo ma inny układ. Warto też
zalogować `intin[0]`/`intin[1]` (start, depth) dla `case 42`.

### 23i — poprawki G6/G7/G10/G8 (wdrożone, bez zmian na ekranie)
- `WM_REDRAW` (komunikat AES `[type,handle,x,y,w,h]`) miał `w=0,h=640`;
  teraz `x=0,y=0,w=640,h=400` (`tos_prg.c`, `case 25`).
- `graf_handle` zwraca teraz `wbox=11,hbox=19` (`intout[3]/[4]`) — z tego GEM
  liczy rozmiary ramek przycisków.
- `_hz_200` (0x4BA) jest teraz **inkrementowane** w `tos_clock_tick()`
  (200 Hz) — wcześniej stało na 0 (stąd dzielenie przez zero w benchmarku).
- Zrzut po zmianach identyczny z poprzednim (0 px), więc to nie one rysują
  brakującą treść.

### 23j — RAMKI PRZYCISKÓW SĄ (naprawione)
- Zalogowane argumenty `objc_draw`: **start=0, depth=99**, tree=`0x0101a066`
  — czyli SysInfo rysuje całe drzewo od korzenia. Korzeń (panel 436×296) ma
  `ob_type = 0x7114` (spoza zakresu GEM), więc `aes_obj_draw` go pomijał.
- Poprawka: w `aes_obj_draw` gałąź `default` rysuje ramkę, gdy typ jest
  **spoza 20..32**, a prostokąt ma sensowny rozmiar (w,h ≥ 16). Teraz panel
  i ramki pól/przycisków są widoczne — zmiana **8766 px**.
- Zrzut: `docs/screenshots/session23j-frames-640.png`.
- Co jeszcze brakuje: **tekst wewnątrz pól/opisów** (puste prostokąty po
  etykietach F1..F10 i w kolumnie wartości). To następny krok — albo
  `G_FTEXT`/`TEDINFO` rysowany z innego miejsca, albo wartości SysInfo
  dopisuje sam.

### 23k — próba przełączania sesji (Krok D) i co ją blokuje
- Klawisze TOS **nie** przechodzą przez `kernel_user_keyboard_event()`
  (sprawdzone: jedyny caller to `kernel_user_gui_keyboard_wake`); trafiają do
  `tos_poll_input()` w `tos_prg.c` (raw scancodes z `tos_next_scancode()`).
  Hak dodany najpierw w `kernel_module.c` był więc martwy — przeniesiony.
- W `tos_prg.c` dodany detektor Shift+Alt+F10 (`tos_kb_shift`/`tos_kb_alt`,
  scancodes 0x2a/0x36/0x38 + break 0xaa/0xb6/0xb8; F10=0x44) wołający
  `ksessions_hotkey_cycle()` (nowa funkcja w `kernel_sessions.c`).
- Zmierzone: po `hatari-event keydown shift/alternate + keypress F10`
  stan sesji **się nie zmienia** (`request` dalej 0, `current` bez zmian),
  czyli sam skrót nie dociera jako scancodes do ścieżki TOS (do zbadania:
  czy Hatari wysyła scancode 0x38 dla "alternate", czy keydown w ogóle ląduje
  w kolejce IKBD).
- Drugi, ważniejszy bloker Krok D: `kernel_tos_use_surface()` ustawia
  `kernel_tos_desktop_visible=1`, a `apply_request()`/
  `ksessions_pump()` wołają `kgfx_session_view(kernel_user_desktop_visible())`
  — więc **dla każdej sesji** widok pulpitu (z oknem SysInfo) pozostaje
  włączony. Nawet udane przełączenie nie pokazałoby shella. Trzeba rozdzielić
  „widok pulpitu" od „aktywnej sesji" (np. czyścić flagę przy przełączeniu na
  sesję shella).

### Następny krok
1. Zdiagnozować, czemu Shift+Alt+F10 nie dochodzi do `tos_poll_input`
   (zrzut `tos_rawlog` po wysłaniu klawiszy) i rozdzielić widok pulpitu od
   aktywnej sesji, żeby Krok D (zrzut SysInfo + żywy shell) był możliwy.

### 23l — ROOT CAUSE przełączania: `hatari-event` nie wstrzykuje klawiszy
- `src/control.c:87` (`Control_InsertKey`): jeśli string ma >1 znak, jest
  parsowany jako **keycode** (scancode 0..255); jeśli 1 znak — jako znak.
  Czyli `keydown shift`/`keydown alternate` **nie działają** (strtol na
  "shift" się nie udaje) — to był błąd w naszych testach przez wiele sesji.
- Poprawna forma to scancodes: Shift=42, Alt=56, F10=68.
- ALE nawet `hatari-event keypress 42` / `keypress a` **nie dociera** do
  jądra: `tos_rawlog_n==0` (pierścień surowych scancodów ścieżki TOS) oraz
  `ikbd_raw_head==ikbd_raw_tail==0` (pierścień IRQ ACIA). Emulowana maszyna
  nie widzi żadnego bajtu klawiatury.
- Wniosek: blokerem Kroku D nie jest nasz kod sesji/hotkeya, tylko wejście
  klawiatury w tej konfiguracji Hatari. Sprawdzić `IKBD INPUT: POLLING READY`
  w logu startowym i czy moduł INPUT.SYS faktycznie czyta ACIA.
2. Uzupełnić tekst w polach: `TEDINFO` (`te_ptext`, justowanie `te_just`),
   `G_BUTTON` — sprawdzić, gdzie SysInfo trzyma wartości (drzewo ma 42
   `G_STRING`, więc wartości są chyba w `TEDINFO`/`G_FTEXT`).
3. `vq_scrninfo`/`vq_extnd` (G2), `form_do`/menu (G11) dla dialogów.
3. Ewentualnie dokończyć `G_IMAGE`/`G_ICON` przez `vro_cpyfm`.

### 23m — klawiatura: działa w BIOS, nie pod jądrem
- `hatari-event keypress 30` (scancode 'A') na ekranie BIOS **przerwał
  auto-boot** → wstrzykiwanie klawiszy działa sprzętowo.
- Pod jądrem (SysInfo na ekranie) ten sam scancode **nie dochodzi**: ani przez
  usługę INPUT (`tos_rawlog_n==0`), ani przez bezpośredni odczyt ACIA.
- Dodany w `kernel_module.c`: fallback w `kernel_user_keyboard_event()` —
  gdy usługa INPUT jest nieobecna/cicha, czyta ACIA bezpośrednio
  (`kernel_ikbd_irq_capture()` + `ikbd_raw_pop()`, klasyfikacja
  presja/zwolnienie po bicie 7 scancode). Nadal jednak `ikbd_raw_head==tail`,
  czyli **ACIA nie dostarcza** w stanie, w jakim jest po inicjalizacji jądra
  (prawdopodobnie konfiguracja ACIA/IKBD albo bit RDRF). Następny krok:
  porównać stan rejestru kontrolnego ACIA BIOS-u i jądra.

### 23n — G2 wdrożone; nowa hipoteza klawiatury
- `vq_extnd`/`vq_scrninfo` (VDI 102) w `tos_vdi.c`: `intin[0]==0` → standardowa
  tablica rozszerzona (planes/CLUT/kolory); `intin[0]>=1` → `vq_scrninfo` z
  **prawdziwym `BYTES_LIN`=`vdi_pitch` (80)** i **fizyczną bazą ekranu**
  (`vdi_target`), plus clip w `ptsout`. Wcześniej zwracał same jedynki
  (program rysujący sam dostawał skok 1 bajtu).
- Weryfikacja: zrzut identyczny z poprzednim (0 px) — SysInfo nie woła 102,
  więc to poprawka pod inne programy (i pod NVDI).
- Jądro **nie zapisuje** rejestru kontrolnego ACIA (tylko czyta) — więc sprzęt
  nie jest psuty przez inicjalizację. Nowa hipoteza blokady klawiatury:
  klawisze odbiera **sesja shella** (druga sesja) zanim dojdą do sesji TOS
  (shell blokuje się na `kernel_user_terminal_getchar`). Do sprawdzenia:
  czy `kernel_user_terminal_getchar`/shell pump czyta zdarzenia niezależnie
  od tego, która sesja jest aktywna.
- Uwaga proceduralna: `hatari-event keydown shift`/`alternate` **nie
  działają** (to nie scancode); używać liczb: Shift=42, Alt=56, F10=68.
