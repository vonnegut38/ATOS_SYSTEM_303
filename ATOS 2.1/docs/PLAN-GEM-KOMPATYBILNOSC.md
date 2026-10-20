# Zgodnosc z GEM (droga A: nasz kod, zrodla GPL jako wzorzec)

Cel: desktop o wygladzie FreeMiNT/XaAES, ale na naszym kernelu (nie Linux),
ze skalowalnymi oknami i programami TOS uruchamianymi w oknie.

> **STATUS (2026-09-26) — jedno źródło prawdy; dalsze sekcje to kronika i część
> wniosków jest nieaktualna** (patrz `docs/PRZEGLAD-PRACY.md` §2.3):
> - **trap #2 AES istnieje:** `tos_aes_dispatch` + `appl_init` (10),
>   `wind_create` (100), `objc_draw` (42), `form_*` w
>   `system/tos_prg.c`, wołane z `asf_tos_aesvdi`. Nieaktualne: „Trap #2 AES
>   nie istnieje” (niżej, „Trapy AES — plan”).
> - **Ochrona faultów programu istnieje:** stuby
>   `asf_tos_fault2/3/4/5/6/7/8/11` (`system/tos_entry.S`,
>   `system/tos_prg.c`). Nieaktualne: „Ochrona przed faultem programu: brak”
>   i „double bus error zatrzymuje CPU”.
> - **GEMDOS pamięć:** `TOS_HEAP_SIZE = 4194304` (`tos_prg.c`),
>   `tos_mfree` z koalescencją, `Mfree` w dispatchu. `Mshrink` zwraca 0,
>   ale w GEMDOS **0 = sukces**, nie błąd. Nieaktualne: `TOS_HEAP_SIZE 65536`,
>   „Mfree nieobsłużone”, „Mshrink→0 jako błąd” (niżej, „PRZYCZYNA
>   ZNALEZIONA”).
> - **Rozpoznanie AES/VDI** idzie po niskich opkodach w `asf_tos_aesvdi`,
>   nie po „0xC8+”.

## Co juz mamy (system/tos_prg.c, 1281 linii)
- GEMDOS: 0x3C Fcreate, 0x3D Fopen, 0x3E Fclose, 0x3F Fread, 0x40 Fwrite,
  0x41 Fdelete, 0x42 Fseek, 0x43 Fattrib, 0x48 Malloc, 0x49 Mfree
- konsola i klawiatura: 0x01, 0x02, 0x06, 0x07, 0x08, 0x09, 0x0B, 0x19,
  0x2A-0x2D, 0x39, 0x3A, 0x2F, 0x1A (z tlumaczeniem scancode/ASCII)
- basepage, ramka trapu (68010+), wektory #1 GEMDOS / #2 AES-VDI / #13 BIOS /
  #14 XBIOS, blok Line-A, naglowek .PRG 0x601A, SCREEN_BASE
- rysowanie podstawowe: putpixel, line, rect, polygon

## Czego brakuje (kolejnosc priorytetu)
1. GEMDOS domkniecie: Pexec (0x4B), Fdup, Fforce, Dsetpath (0x3B),
   Fsfirst/Fsnext (0x4E/0x4F), Mshrink, Super, czas/data
2. VDI: v_opnvwk, v_clsvwk, v_bar, v_gtext, v_line, v_pline, v_fillarea,
   vsf_*, vst_*, vq_*; stacja robocza i atrybuty; mapowanie na kgfx + kompozytor
3. AES: appl_init, wind_create/open/close/set, wind_update, wind_get/set,
   evnt_multi/key/button/timer, objc_draw, objc_find, menu_bar, menu_tnormal, form_*
4. Pulpit: ikony, .INF, desk_*, shel_*; programy TOS w oknach
   (wm_resize juz daje skalowalne okna)

## Szybkie rysowanie: czego uczy EmuTOS (zachowanie, nie kod)
- Blitter sprzetowy to sciezka podstawowa (EmuTOS ma osobny vdi_tblit.S),
  petle CPU sa fallbackiem. U nas kblit_* lezy gotowe i nieuzywane.
- Kopie slowowe z swap zamiast bajtowych (na 68000/030 jedno slowo = jeden cykl).
- Operacje rastrowe (AND/OR/XOR) raz dla mono, potem reuzywane dla wzorcow.
- w_rlist: lista widocznych prostokatow per okno - my mamy dokladnie to (clip[]).

## Zasada drogi A
- Zrodla GPL (EmuTOS, FreeMiNT/XaAES) czytamy dla algorytmow i struktur.
- Implementacje piszemy wlasne w commander-v9/; zaden plik GPL nie wchodzi do kernela.
- Kazdy etap: najpierw testy na hoscie, potem self-test na maszynie, potem Hatari.

## Zrodla referencyjne (do pobrania)
- EmuTOS: work/emutos-reference (aes/, vdi/, desk/, bdos/, bios/) - jest w drzewie
- FreeMiNT/XaAES: github.com/freemint/freemint, katalog xaaes/src - do pobrania

## Pierwszy krok
Domkniecie GEMDOS i .PRG tak, aby zwykly program TOS startowal i dzialal w naszym systemie.

## Launcher w Commanderze (wymaganie uzytkownika)

Zachowanie: najechanie na plik + Enter albo dwuklik myszy uruchamia program.

Rozgalezienie po rozszerzeniu (zasada: rozszerzenie mowi, czego uzyc):
| Rozszerzenie | Runner |
|---|---|
| .PRG / .TOS | kernel_tos_run_program (persona TOS, trapy #1/#2/#13/#14) |
| .APP / .EXE | natywny spawn ASF (gui_call op 34) |
| inne | komunikat, nie uruchamiamy |

Stan (2026-09-26): **ZROBIONE (Etap 0)**.
- Enter na pliku: .APP/.EXE -> gui_call(34) (natywny spawn);
  .PRG/.TOS -> gui_call(65) -> kernel_tos_run_program (persona TOS).
  Pasek statusu Commandera pokazuje 'TOS RC <kod>'.
- shell: komenda 'tos <PLIK.PRG>' (API modulu v5, offset 188).
- syscall: **65** (60 bylo juz zajete przez kfat_save_path).
- USP: asf_tos_enter/asf_tos_exit zapisuje i odtwarza %usp wolkajacego
  (wejscie z procesu przez syscall inaczej konczylo sie
  USER_STACK_POINTER_FAULT i konczylo proces).
- Commander NIE zdejmuje ekranu przez rpc(gui,7/6) wokol TOS - ta podroz
  tam i z powrotem konczyla proces Commandera; persona sama rzadzi konsola.
- dwuklik myszy: nadal do podpiecia.

Bledy programu nie zatrzymuja systemu: persona ma stuby faultow, ktore koncza
program i wracaja do wywolujacego.

Start systemu: AUTOEXEC.PRG (jeden stat) zamiast skanowania katalogu - koszt
nie rosnie z liczba programow na dysku.

## Trapy AES - plan (to blokuje prawdziwe programy TOS)

Stan: trap #1 GEMDOS (pelny) i trap #2 VDI (v_opnvwk, v_bar, v_line,
atrybuty, powierzchnia okna, blitter) dzialaja. Trap #2 AES nie istnieje.
SYSINF83.PRG wlasnie na tym pada (zapis pod $0 -> bus error -> CPU halt).

Kolejnosc wdrazania (kazdy krok testowalny osobnym .PRG):
1. appl_init + graf_handle + graf_mouse  - aplikacja wstaje, dostaje ID
2. wind_create/open/set/get/close/delete - okno AES = okno kompozytora
   (mamy juz wm_move/wm_resize/wm_raise i liste przyciecia w_rlist)
3. evnt_multi z MU_KEYBD / MU_BUTTON / MU_TIMER
4. objc_draw + drzewo obiektow (najwiekszy kawalek, VDI juz jest)
5. menu_bar / menu_tnormal / form_* (na koncu)

Protokol: asf_tos_aesvdi(contrl,intin,ptsin,intout,ptsout).
Rozpoznanie: contrl[0] w przestrzeni VDI (0x73=v_opnvwk...) albo AES
(0xC8+). Wynik do intout[0].

Ochrona przed faultem programu: brak. Program TOS siegajacy w niezaplikowane
wywolanie zapisuje pod $0 i zatrzymuje CPU (double bus error). Docelowo:
wektory bus/address pod wlasciwym VBR + niepowracajace wyjscie z faultu,
zeby blad zamykal PROGRAM, a nie system (wymaga wariantu -m68020).


## Ustalenie: SysInfo (KONKRETNIE)

Ten sam kernel, ta sama sciezka, ten sam katalog:
- PROG1.PRG (382 B)  -> pelny boot, LD:enter/stat/alloc-file/alloc-mem, AUTOEXEC: OK
- SYSINF83.PRG (58648 B) -> czarny ekran + double bus error

Proba: wgrany od nowa pod NOWA nazwa (SYSNEW.PRG, swiezy wpis FAT, bez
nadpisywania) -> IDENTYCZNY crash. To obala hipoteze uszkodzonego obrazu.
Wniosek: program uruchamia sie i pada na naszym kernelu.

Podejrzenia (w kolejnosci):
1. program sam czysci ekran (stad czern) i pada pozniej - trzeba przechwycic
   jego wywolania; nasz trace #nr=wynik/log LD: idzie na konsole BIOS, ale
   program moze ja zamazac,
2. brak ochrony faultu: double bus error zatrzymuje CPU, wiec nie widzimy
   zadnej diagnostyki. To jest priorytet (wektory bus/address pod VBR),
3. program uzywa czegos, czego nie mamy (Malloc/Mxalloc/Pexec lub XBIOS).

Nastepny krok: nie dopuszczac do double faultu - niepowracajace wyjscie
z faultu + wektory pod wlasciwym VBR, zeby zamiast haltu dostac kod bledu
i moc odczytac, gdzie program sie wysypal.


## MagiC - co dokladnie robi (zrodla)

1. VBR: MagiC ustawia "movec d0,vbr" z d0=0 (MAGIBIOS.S:571,594) - czyli
   wektory wyjatkow POD ADRESEM 0, klasycznie jak 68000. My robimy teraz to
   samo (kernel_main.c, raw .word bo kernel budowany jako 68000). To jest
   konieczne, inaczej faulty w programie nie trafiaja do naszych handlerow.
2. Basepage (KERNEL.INC): pelny uklad pol - p_lowtpa..p_cmdlin, w tym
   p_dta(0x20), p_parent(0x24), p_procid(0x28), p_env(0x2c), p_flags(0x36),
   p_defdrv(0x37), p_drvx(0x40), p_mem(0x68), p_mflags(0x70), p_ssp(0x78).
   Uzupelnione w system/tos_prg.c.
3. Stos: MAGIDOS.S:7414 "move.l p_hitpa(a5),a1 ; usp liegt am Ende der hitpa",
   7425 "move.l sp,p_ssp(a5)" - stos uzytkownika na koncu hitpa, p_ssp zawsze
   zapisane. Zrobione.

Wynik: SysInfo nadal pada (double bus error). VBR=0 nie zmienil zachowania.
Hipoteza: fault powstaje zanim program dojdzie do swojego kodu, albo w trybie
supervisora (wtedy stub user-mode nie pomoze), albo program uzywa czegos, co
w MagiC dziala inaczej niz u nas (XBIOS/GEMDOS szczegoly).


## PRZELOM: debugger Hatari potwierdzil ze faulty DZIALAJA

Odczyt debuggerem (hatari-debug info vbr / m 0 64 / r) podczas SysInfo:
  VBR  00000000                      <- nasze VBR=0 dziala
  wektor 2 (bus error) = 0x000401A2  <- NASZ kernel (__image_start=0x00040000)
  Bus error at 0xe001d0  move.w (a0)==$ffff8282   <- program SONDUJE SPRZET
  Bus error at 0xe001d0  move.w (a0)==$ffff8400   <- i IDZIE DALEJ
  Bus error at 0xe00228  reading $5000000         <- i IDZIE DALEJ

Wniosek (odwraca poprzednia hipoteze): NIE jest tak, ze handler nie wchodzi.
Handler wchodzi i OBSLUGUJE sondowanie sprzetu - program kontynuuje po kazdym
bus errorze. Czyli fault containment DZIALA dla programow dotykajacych sprzetu.

Prawdziwy problem jest pozniejszy: program dochodzi do $1018a66 i PISZE pod $0,
czyli dostal 0 z jakiejs funkcji systemowej i uzyl go jako wskaznika. To jest
brak zgodnosci GEMDOS/AES/XBIOS (praca przyrostowa), a nie awaria faultow.

Narzedzie: log wywolan w RAM pod 0x0007e8dc (tos_call_log, wpis = fn<<16|wynik,
[0] = licznik) do odczytu debuggerem. Uwaga: po halcie CPU debugger nie odpowiada,
wiec dump trzeba zrobic PRZED crashem albo zwiekszyc czas.

Status: PROG1 (i programy korzystajace z GEM) dzialaja. SysInfo dochodzi do
sondowania sprzetu (obslugiwane) i pada pozniej na brakujacym wywolaniu.


## DEKODOWANIE INSTRUKCJI (debugger) - klucz do SysInfo

Z logu Hatari (CPU=$1018a66):
  01018a66  0079 9f01 0000 0000   ori.w #$9f01,($00000000).L   <- zapis pod 0
  0101886c  48e7 7ffe             movem.l d1-d7/a0-a6,-(a7) == $fffffffc
                                                            ^ A7 = 0xFFFFFFFC

Druga instrukcja to PROLOG FUNKCJI (odlozenie rejestrow) z A7 = 0.
Czyli program ma zerowy wskaznik stosu i kazdy prolog funkcji pisze pod 0-4.

Sprawdzone: nasz asf_tos_enter ustawia stos poprawnie (caller daje mem+total-16,
czyli koniec p_hitpa; offsety argumentow 52/56/60 sa wlasciwe przy 11 rejestrach).
Wiec to PROGRAM zeruje sobie A7 - najpewniej uzywa wskaznika, ktory jakas
funkcja systemowa zwrocila jako 0 (np. Malloc na stos albo Mshrink).

Narzedzie gotowe: tos_call_log pod 0x0007e8dc (wpis = fn<<16|wynik, [0]=licznik).
Uwaga: po halcie CPU debugger Hatari nie odpowiada - dump robic NA ZYWO
(hatari-debug m 7e8dc 40) i dac mu kilka sekund na wypisanie wyniku.

Status: PROG1 dziala. SysInfo dochodzi do sondowania sprzetu (obslugiwane),
potem zeruje sobie A7 i pada. Naprawa = znalezc wywolanie zwracajace 0.


## PRZYCZYNA ZNALEZIONA: GEMDOS zarzadzanie pamiecia

Przeglad system/tos_prg.c:
  #define TOS_HEAP_SIZE 65536u          <- stos zaledwie 64 KB
  static uint32_t tos_heap_used;        <- bump allocator, BEZ zwalniania
  case 0x48u: Malloc  -> tos_malloc()   <- zwraca 0 gdy heap sie skonczy
  case 0x49u: Mfree   -> NIE OBSLUZONE
  case 0x4au: Mshrink -> return 0       <- ZAWSZE zero (a to sygnal bledu!)

Skutek: SysInfo (58 KB, alokuje bufory) wyczerpuje 64 KB, Malloc zwraca 0,
program uzywa tego zera jako wskaznika/stosu -> A7=0 -> prolog funkcji pisze
pod 0-4 -> bus error -> (brak powrotu) koniec.

To wyjasnia wszystko:
- PROG1 (382 B, zero alokacji) dziala,
- SysInfo pada zawsze w tym samym miejscu (prolog funkcji z A7=0),
- fault containment dziala (sondowanie sprzetu obslugiwane),
- "ori.w #$9f01,($0).L" i "movem.l ...,-(a7) == $fffffffc" to skutki A7=0.

