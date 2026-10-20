# Przegląd zmian — ATOS 2.1 (cel: 68030)

Stan: 2026-10-09 ok. 08:20. Baza porównawcza: `backup-atos21-20261006-204700` (źródła z 6.10 20:47).
Zakres: ~35 zmienionych plików (`tos_prg.c` ~2100 zmienionych linii, `tos_vdi.c` ~700, `desktop_wm.c`, `tos_entry.S`,
`kernel_main.c`, `kernel_wm.c`, `kernel_graphics.c`, `Makefile`, narzędzia) oraz 12 nowych (`tos_gdos.c`, `tosdir.c`,
`tosgdos.c`, `tosin.c`, `tossup.c`, `toslow.c`, `tosvdi.c`, `tos_gdos.h`, `tos_damage.h`, `wm_chrome.h`, `kgfx_mono.h`,
`tools/img_put.py`) plus dokumenty z `AUDYT_2026-10-06_WDROZENIE/`.

Metoda: diff względem bazy + czytanie kodu w kontekście. **Bez budowania i bez uruchomień Hatari** (brak toolchainu m68k
w środowisku analizy). Numery linii z kopii pobranej ok. 07:55; `tos_prg.c`, `tos_vdi.c`, `tos_entry.S`, `tosdir.c`
były edytowane 9.10 o 07:46, więc numery mogą się przesunąć.

Oznaczenia: **✔** = sprawdzone bezpośrednio w kodzie; **○** = z przeglądu kodu, niezweryfikowane drugi raz (sprawdzić przed poprawką).
Odrzucony fałszywy alarm: `kernel_fault_d0` JEST zdefiniowane (`kernel_exception.c:28`).

## 1. Błędy potwierdzone (✔)

### 1.1 Fałszywy raport „DESKTOP.APP STOPPED" przy każdym starcie — WYSOKI
`kernel_main.c:697-703`: `if(kernel_user_take_completion(kernel_desktop_pid,&desktop_status))`.
`kernel_exception.c:580-584` zwraca **−1**, gdy proces jeszcze żyje, więc warunek jest prawdziwy od pierwszej iteracji.
Skutek: `kernel_desktop_report(0,0)` zamalowuje ekran na czarno, czyści konsolę, wypisuje „DESKTOP.APP STOPPED … TEXT SHELL ACTIVE",
ustawia `kernel_desktop_stopped=1` i `kernel_desktop_pid=0` — czyli prawdziwa awaria pulpitu nie zostanie już zgłoszona.
To unieważnia poprawkę „widoczność awarii" z 6.10.
**Poprawka:** `if(kernel_user_take_completion(kernel_desktop_pid,&desktop_status)==1)`.
Dodatkowo dla `status==0` nie czyścić ekranu i wywołać `redraw_prompt`.

### 1.2 `Pterm(n)` zawsze zwraca kod 0 — WYSOKI
`tos_prg.c:1373-1374` (`tos_ptr_plausible`: `p<0x00C00000`), `:1553`, `:3549` (`usp=mem+total-58`), `:3232-3233` (TPA = 0x04000000–0x05000000).
Stos gościa leży w TT-RAM, więc nigdy nie jest „wiarygodny" i `tos_exit_code=0`. Test akceptacyjny „TOS RC 0" nie wykryje żadnego błędu.
**Poprawka:** wiarygodność względem `[tos_current_basepage, tos_process_limit)` (lub zakresu TPA) zamiast stałej 0xC00000.

### 1.3 Przekazywanie wyjątku do handlera gościa jest błędne — WYSOKI
`tos_entry.S:254-277` (makro `tos_fault_stub`) i `:322-344` (Line-F).
* Makro: po `move.l %a0,-(%sp)` / `movem.l 4(%sp),…` / `rts` SP wskazuje 16 B poniżej ramki CPU, nie na ramkę — `rte` handlera zdejmie śmieci (komentarz w :273 jest nieprawdziwy).
* Line-F: `movem.l (%sp)+,…` (:338) przywraca stare `a0`, więc :339 wrzuca na stos to stare `a0`, a nie adres docelowy; potem :340 ładuje rejestry z ramki wyjątku.
* Blok wykona się tylko gdy stub zostanie wywołany łańcuchowo (`Setexc` zwraca jako „stary" handler `*slot` = nasz stub); skok do handlera gościa daje wtedy pętlę nieskończoną.
**Poprawka:** usunąć oba bloki forward. Dla łańcuchowania zwracać z `Setexc` adres osobnego „domyślnego" stuba, który nie sprawdza slotu.

### 1.4 `Setexc`: dziury w zapytaniu i w przywracaniu — WYSOKI
`tos_prg.c:1588-1598`: dla `Setexc(v,-1)` na pustym slocie zwracane jest 0 (podstawienie `asf_tos_just_rte` leży wewnątrz `if(newh!=-1)`); dla wektora spoza zakresu zwracane −1 (program zrobi `jmp` pod 0xFFFFFFFF).
`:119-151`: snapshot obejmuje wektory 2..47 i 0x60..0x7F, ale nie 48..95 (w tym MFP 0x100–0x13C), ani `etv_*` (0x400–0x408), mimo że `tos_vec_slot` (:137) pozwala je zapisać przez `Setexc(0x100+)`. Po wyjściu zostają wskaźniki do zwolnionego TPA.
**Poprawka:** przenieść `if(!old)old=(uint32_t)asf_tos_just_rte;` przed `if(newh!=-1)`; dla nieznanych wektorów zwracać stub i nie instalować; dopisać do snapshotu wektory 48..95 oraz 3 długie słowa `etv_timer/etv_critic/etv_term`.

### 1.5 `Fdatime` nie istnieje, choć dokument twierdzi inaczej — WYSOKI (zgodność)
`SLAD_KOBOLD_2026-10-08.md` pkt 5 pisze, że `Fdatime` (0x57) zwraca datę i godzinę. W `tos_prg.c` nie ma `0x57` ani `Fdatime`; wywołanie wpada w `default` i zwraca −32.
**Poprawka (szkic, nietestowany):**
```c
case 0x57u: { /* Fdatime(timeptr, handle, wflag) */
    uint32_t p=((uint32_t)a[0]<<16)|a[1];
    if(a[3]==0&&p){wr16((uint8_t *)(uintptr_t)p,tos_time);wr16((uint8_t *)(uintptr_t)p+2u,tos_date);}
    return 0; }
```
(`tos_time`/`tos_date` istnieją: `tos_prg.c:154`; `wr16` analogicznie do `wr32`.) Dla zapisu (`wflag!=0`) zwracać 0 lub EACCDN.

### 1.6 GDP 11: koło, elipsa, łuk, rbox rysują prostokąt — WYSOKI
`tos_vdi.c:540-603`: opcode 11 obsługuje tylko `contrl[5]==10` (`v_justified`); każde inne `contrl[5]` kończy w `vdi_fill_area(ptsin[0..3],…)`. `v_circle` (ptsin = xc,yc,0,0,r,0) rysuje więc prostokąt od środka do (0,0).
**Poprawka:** `switch(control[5])`: 1 = bar, 2/3 = łuk/wycinek, 4 = koło (`r=ptsin[4]`), 5 = elipsa (`rx=ptsin[2]`, `ry=ptsin[3]`), 8/9 = rbox/rfbox, 10 = justified. Dla niezaimplementowanych: `tos_note_unsupported` i **nie rysować**.

### 1.7 `vro_cpyfm`: szybka ścieżka kopiuje do złej kolumny — ŚREDNI
`tos_vdi.c:452-473`: indeks grupy `g` (z `sx0>>4`) służy jednocześnie do `srow[g]` i `drow[g]`. Warunek wejścia wymaga tylko `(ptsin[0]&15)==(ptsin[4]&15)`, więc przy `dx0!=sx0` blok jest kopiowany „w miejscu". Przewijanie pionowe działa, przesunięcie poziome o wielokrotność 16 px nie.
**Poprawka:** `drow[(dx0>>4)+(g-g0)]`, maski brzegowe liczone względem celu, a gdy `backwards` i ten sam wiersz — pętla `g` od końca.

### 1.8 TPA ze stałego adresu 0x04000000 — ŚREDNI (cel 68030)
`tos_prg.c:3232-3240`: baza i szczyt TPA są stałe, bez żadnego fallbacku. Dla 68030 nie jest to problem architektury 24-bitowej, ale zakłada konkretną kartę (TF536) z pamięcią pod tym adresem; na innej maszynie 68030 `tos_tpa_alloc` zwróci błąd (-6/-7) albo trafi w nieistniejący obszar.
**Poprawka:** baza/szczyt TPA wyprowadzić z wykrytej pamięci (`g_system.tt_ram_bytes`, pula `kmem`), z fallbackiem do ST-RAM. Poprawić też komentarz w :3363 („first free ST-RAM block").

### 1.9 KBDVECS/IOREC — ŚREDNI
`tos_prg.c:3342-3362`: blok alokowany przed `tpa_mark` (:3362) — wyciek ok. 0,8 KB na każde uruchomienie (również na wczesnych wyjściach −6/−7). `tos_iorec_t` (:417-422) ma 12 B zamiast 14 (brak low/high water), a kolejki nigdy nie są zasilane klawiszami (tylko zerowanie `head/tail`). KBDVECS ma według przeglądu 5 z 9 pól, więc `joyvec`/`midisys` nakładają się na pierwszy IOREC (rozmiaru struktury nie sprawdzałem osobno).
**Poprawka:** `tpa_mark` przed alokacją; struktura z 9 wskaźników; IOREC 14 B; zasilanie `Iorec(1)` z klawiatury.

### 1.10 Licznik uchwytów okna TOS — ŚREDNI
`tos_prg.c:2661, 3136, 3145` i `:440`: `static uint8_t tos_window_seq` rośnie bez zerowania, a `kernel_tos_window_is` rozpoznaje tylko `BASE..BASE+63`. Od 64. uruchomienia w sesji zamknięcie okna krzyżykiem przestaje kończyć program.
**Poprawka:** `tos_window_seq=(uint8_t)((tos_window_seq+1u)&63u)`.

### 1.11 Bramka PIC nie obejmuje `DESKTOP.APP` — ŚREDNI
`tools/check_gui_apps.py:6` sprawdza tylko SYSINFO, MONODESK, COUNT, NOTE, COMMAND, FATWRITE, COMTEST, CACHETST, TRASHTST, TERMTST, i to wyłącznie operandy `jsr`/`jmp`. „Zielona dla 10 aplikacji" z `PRZEGLAD §0a` nie dotyczy pulpitu, czyli aplikacji, która się pierwotnie zepsuła, i nie złapie absolutnych odwołań do danych.
**Poprawka:** dodać `desktop_wm` (oraz AC, HW, SETUP, TASKMGR…) i sprawdzać `readelf -r`/`objdump -r` (zero `R_68K_32`).

### 1.12 `toscolour.c` — odbita maska bitu — NISKI
`toscolour.c:72-73`: `bit=15-(x&15); mask=0x8000u>>bit` daje `1<<(x&15)` (LSB-first). Powinno być `mask=0x8000u>>(x&15)`. Test przechodzi po znacznikach pamięci, więc obrazu nie sprawdza.

## 2. Ustalenia do potwierdzenia (○)

**WYSOKI**
* VDI w trybach kolorowych pisze 1-bitowe słowa do bufora 8 bpp (`tos_vdi.c:96`, `tos_prg.c:461`, `kernel_main.c:629`).
* `vdi_fill` ignoruje kolor, tryb zapisu i interior; `wrt_mode` startuje jako 0 (`tos_vdi.c:108-145, 604-620`).
* Pętle wielokąta, `v_bar` i `vdi_cpyfm` bez przycięcia do ekranu mogą zawiesić jądro przy współrzędnych ±32767 (`tos_vdi.c:286, 313, 354-356, 474-484`).
* Uchwyty `Fopen/Fcreate` zaczynają się od 0 (GEMDOS rezerwuje 0–5), limit 8 uchwytów (`tos_prg.c:1203`, `TOS_MAX_HANDLES`).
* `Pexec(0)` z programu TOS: dziecko nie dostaje pamięci (`tos_tpa_alloc(total)` zajmuje całe TPA), KBDVECS rodzica jest zerowane, snapshot wektorów jest jeden globalny (`tos_prg.c:3343-3364`).

**ŚREDNI**
* `vrt_cpyfm` używa tablicy ROP od `vro_cpyfm` zamiast trybów 1–4 (`tos_vdi.c:433-437, 481-482`).
* Brak opcodów `vs_color`(14), `vst_point`(107), `vqin_mode`(115), `v_pmarker`(7); `v_get_pixel`, `vqt_width`, `vsm_height` zwracają błędne wartości.
* Uchwyt workstation z `contrl[6]` nie jest czytany po `v_opnvwk`; stan VDI nie jest zerowany między programami (`tos_vdi.c:22, 505-529`).
* `vst_font` zapisuje ID fontu, a `vqt_fontinfo` traktuje je jako indeks (`tos_vdi.c:648, 871`).
* `form_center`: brak centrowania i minimum 1×1 (regresja, `tos_prg.c:2519-2529`).
* `wind_set(WF_NAME)` czyta znaki z `intin[3+k]` zamiast ze wskaźnika (`tos_prg.c:2352-2360`).
* `Cconis`/`Bconstat` zwracają 1 zamiast −1; `Crawio` blokuje (`tos_prg.c:590-596, 1417-1419`); testy sprawdzają `>0`.
* Klawisze launchera na pulpicie działają przy otwartych oknach dialogowych (`desktop_wm.c:2316-2327`).
* `Dsetpath/Dgetpath`: zły kod błędu (−1 zamiast −34), brak normalizacji ścieżki; `Fsfirst/Fsnext` trzymają stan w jednym statycznym `tos_dir` (zagnieżdżone wyszukiwania się nakładają), limit 384 wpisów obcina listing bez błędu.
* `Malloc`: limit 128 żywych bloków (`TOS_ALLOC_SLOTS`).
* `asf_tos_exit` nie przywraca SR (`tos_entry.S:51-68`); `mousevec` gościa jest wołany z ISR Timer C (`tos_prg.c:2866-2896`).

**NISKI**
* `DESKTOP.CFG` zapisuje `RES=0`, gdy bieżący tryb nie jest na liście pięciu (`desktop_wm.c:2141`); klucz ikony obcięty do 27 znaków (`:1620`).
* ROM: `boot_delay` z SETUP jest martwy (sztywno 2 s, `main.c:55-66`); failsafe Alt zjada pierwszy klawisz inny niż Alt (`main.c:31-49`).
* `install_boot_sys.py` zwalnia stary łańcuch FAT przed sprawdzeniem wolnych klastrów (:230-236); `img_put.py` nie skraca łańcucha przy krótszej treści (:82) i po cichu przycina nazwy 8.3; `mk_tosprg.py` używa stałych `/tmp/mk_tosprg_*.bin` (konflikt przy `make -j`) i nie filtruje relokacji po sekcji.
* Programy testowe: `toslow.c:112` wywołuje `Iorec` z argumentem long zamiast word; `tosgdos.c:68-73` uzbraja wartownika `i<33`, a sprawdza `[33]`; `tosin.c:21` ma `global` o 4 B zamiast 15 słów AES.
* `kernel_wm_resize_window` nie przycina ramki do ekranu (`kernel_wm.c:2035-2052`).
* Licencja: `emutos_vdi_mono.c` ma nagłówek GPL-2.0+, a `tos_vdi.h`/`tos_vdi.c` deklarują „no GPL code" — wymaga decyzji, nie poprawki.

## 3. Dla celu 68030

* **Obniżone:** nieparzyste adresy przy rzutowaniu na `uint16_t*/uint32_t*` (Address Error tylko na 68000; na 68030 kara czasowa). Nadal warto zachować bajtowe ścieżki dla danych gościa (MFDB, `Setscreen`).
* **Dotyczy 68030:** ramki wyjątków formatu 0/2/A/B — stuby czytające SR/PC na +0/+2 są poprawne dla wszystkich, ale zrzut „frame" w diagnostyce zakłada krótką ramkę.
* **Do sprawdzenia (nie analizowane):** unieważnianie cache instrukcji (CACR) po relokacji i załadowaniu `.PRG` oraz po modyfikacji kodu przez `Supexec`; interakcja `Supexec`/SR=0x2500 (IPL 5 maskuje VBL i HBL) z kolejką VBL.

## 4. Rozbieżności dokumentacja ↔ kod

* `SLAD_KOBOLD` pkt 5: `Fdatime` „zwraca poprawną datę" — brak w kodzie (1.5).
* `PRZEGLAD-ZMIAN-2026-10-06 §0a`: „bramka zielona dla 10 aplikacji" — lista nie zawiera `DESKTOP.APP` (1.11).
* Nadal otwarte z `ANALIZA_LUKI_LAUNCHERA`/`ANALIZA_STARTU_TOS`: zmienne VBL (`_vblsem` 0x452, `_nvbls` 0x454, `_vblqueue` 0x456, `_vbclock` 0x462, `_frclock` 0x466), niezerowane dziury w basepage (0x30-0x35, 0x60-0x67, 0x6C-0x6F, 0x74-0x77, 0x7C-0x7F), brak wywołania `etv_term`.
* Potwierdzone jako utrzymane z 6.10: `GUI_APP_CFLAGS` z `-mpcrel` (`Makefile:1161`), bufor `DESKTOP.CFG` 512 B w jądrze, `setup_window_height()`, wąskie zamykanie okna po (pid, uchwyt), centrowanie okna TOS wg `kvideo_mode_*` (przegląd ○).

## 5. Kolejność napraw

1. 1.1 (jedna linia, przywraca nadzór nad pulpitem).
2. 1.2 i 1.11 (żeby testy zaczęły cokolwiek dowodzić), potem asercje na niezerowy kod wyjścia.
3. 1.3 i 1.4 (wektory i łańcuchowanie).
4. 1.6, 1.7 oraz przycinanie w pętlach VDI.
5. 1.5 (`Fdatime`), 1.9, 1.10.
6. 1.8 (TPA z wykrytej pamięci), po decyzji, które maszyny 68030 są celem.

## 6. Czego nie sprawdzono

Build, testy hostowe i uruchomienie w Hatari; `kernel_module.c` po zmianach z 6.10 (retry TOSC); `kernel_exception.c` (tylko wersja bazowa); katalogi `boot/`, `hw/`, `loader/`, `ui/`, `mint/`, `video/`; pliki testów Pythona (`tests/test_tos_*.py`) i harness `atos2-tools/`; `ATOS-AUDYT-2026-09-21.md` (nie czytany).
