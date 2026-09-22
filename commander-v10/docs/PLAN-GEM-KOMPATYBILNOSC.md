# Zgodnosc z GEM (droga A: nasz kod, zrodla GPL jako wzorzec)

Cel: desktop o wygladzie FreeMiNT/XaAES, ale na naszym kernelu (nie Linux),
ze skalowalnymi oknami i programami TOS uruchamianymi w oknie.

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

Stan:
- Enter na pliku: czesc dziala (run_program dla .APP przez gui_call(34)),
- dwuklik myszy: do podpiecia,
- wywolanie TOS z aplikacji uzytkownika: potrzebny syscall, np. gui_call(60,path)
  -> kernel: kernel_tos_run_program(path).

Bledy programu nie zatrzymuja systemu: persona ma stuby faultow, ktore koncza
program i wracaja do shella (widoczne jako 'program closed').

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

NAPRAWA (konkretna, priorytet):
1. Malloc z prawdziwego heapu - TOS_HEAP_SIZE co najmniej kilka MB (mamy TT-RAM),
2. zaimplementowac Mfree (0x49) i Mshrink (0x4A) - bez nich programy nie dzialaja,
3. Mxalloc (0x2C) dla AltRAM.


## Heap powiekszony (4 MB) - SysInfo nadal pada w tym samym PC

Zmiany: TOS_HEAP_SIZE 64 KB -> 4 MB (TT-RAM), wyrownanie 4 B, Mxalloc.
Wynik: crash identyczny (PC=$1018a66, op_e3=79). Czyli to NIE wyczerpanie heapu.

Co pozostaje - dwie twarde dane z debuggera:
  01018a66  0079 9f01 0000 0000   ori.w #$9f01,($00000000).L
  0101886c  48e7 7ffe             movem.l d1-d7/a0-a6,-(a7) == $fffffffc   (A7=0)

Operand $00000000 w instrukcji absolutnej jest podejrzany: wyglada na WPIS
RELOKACYJNY, ktory nie zostal zastosowany (powinien byc base+offset), albo na
adres $FFFF8xxx wstawiany przez program w runtime, ktory nie zostal wstawiony.
Priorytet nastepnej sesji: sprawdzic relokacje - czy tabela reloc dla SysInfo
jest w ogole przetwarzana i czy offsety sa < tlen+dlen. Nasz loader:
  reloc=28+tlen+dlen; while(reloc+4<=loaded){r=rd32(file+reloc); if(!r)break;
  reloc+=4; if(r<tlen+dlen) wr32(text+r, rd32(text+r)+mem); }
Dla SysInfo: reloc=0xE514, loaded=0xE518 -> przetwarzany JEDEN wpis.
Jesli SysInfo ma wiecej wpisow relokacji (a ma absolutne odwolania), to znaczy
ze loaded/file length jest zle raportowany albo tlen/dlen zle odczytane.


## FINAL: prawdziwy SysInfo dziala (zamyka sie czysto, system zyje)

Ustalenie kluczowe: SYSINF83.PRG byl USZKODZONY (sam tekst, zero danych/BSS,
zero relokacji, marker 12345678 na koncu). Prawdziwy SYSINFO.APP (5.02, 320 KB,
2625 relokacji) wczytuje sie i uruchamia:

  tts1 C:> AUTOEXEC: \SYSTEM\TEST\SYSINFO.PRG
  LD:enter / LD:stat / LD:alloc-file / LD:alloc-mem     <- 320 KB wczytane
  TOS FAULT vec=11                                      <- nasz handler
  TOS PROGRAM FAULT
  AUTOEXEC: program closed                              <- SYSTEM ZYJE

Czyli zasada "program pada, system zyje" jest spelniona. vec=11 = Line-F
(brak FPU na TF536) - TOS/MagiC pomijaja instrukcje i mowia "brak FPU".
Proba pominięcia (addq.l #2 / #4 na 2(sp) w asf_tos_fault11) nie zadzialala:
asf_tos_fault nadal jest wywolywany dla vec 11 (prawdopodobnie wektory
instaluje tez kernel_exception.c, zanim persona je podmieni).
Nastepny krok: ustalic kto obsluguje vec 11 (persona vs kernel_exception.c)
i sprawic, by Line-F byl POMIJANY, a nie konczyl program.


## Line-F: stan i co dalej

Kod jest poprawnie podmieniony (tos_entry.S:136):
  asf_tos_fault11:
      addq.l #2,2(%sp)
      rte
i instalowany na vec 11 (tos_prg.c:1355 hnd[8]). Mimo to zrzut nadal pokazywal
"TOS FAULT vec=11", czyli asf_tos_fault zostal wywolany. Możliwe przyczyny:
1. ramka Line-F na 68030 ma inny uklad niz zalozone SR(2)+PC(4) -> zapis pod
   2(%sp) psuje ramke i program pozniej pada gdzie indziej (a wtedy which
   vector moze byc 11 ponownie albo inny),
2. dlugosc instrukcji nie wynosi 2 -> skok w srodek instrukcji,
3. wektory sa przywracane/reinstalowane.

NASTEPNY KROK (pewny): w debuggerze Hatari zlapac moment Line-F
(hatari-debug b pc=<addr> lub breakpoint na vec 11), zrzucic stos
(hatari-debug m <sp> 32) i ZOBACZYC uklad ramki oraz opcode instrukcji.
Wtedy dlugosc i offset PC sa znane, a nie zgadywane.

Alternatywa (bez emulatora FPU, zgodnie z TOS na ST): zostawic czyste
zamkniecie programu (obecne zachowanie) i wrocic do tego, gdy jakis program
naprawde bedzie wymagal emulacji FPU.


## Line-F w JADRZE (kernel_entry.S) - gdzie naprawde trzeba

Zrzut tablicy wektorow (debugger, m 0 48):
  vec 2 (bus)   = 0x000401A2
  vec 4 (illegal) = 0x000401C2
  vec10 (line A)  = 0x0004024C
  vec11 (line F)  = 0x0004025A     <- to NIE asf_tos_fault11 (0x54030)!

Czyli wyjatki obsluguje JADRO (kernel_entry.S), a nie persona. Dlatego moje
zmiany w tos_entry.S/tos_prg.c nie mialy efektu (breakpoint na asf_tos_fault11
NIE trafial, a komunikat "TOS FAULT vec=11" pochodzi z kernel_user_fault).

Wlasciwe miejsce: system/kernel_entry.S, etykieta kernel_line_f:
  kernel_line_f:
      moveq #11,%d0
      bsr kernel_try_user_fault     <- konczy program TOS
      move.l #11,-(%sp)
  kernel_fault: ...

Proba: zastapic to przez "addq.l #N,2(%sp) ; rte" (pominiecie instrukcji).
Testy z N=2 i N=4 nie daly potwierdzonej poprawy (zrzuty nieodtwarzane /
identyczne). WYMAGA: odczytac opcode instrukcji Line-F z debuggera
(breakpoint na 0x4025A, potem m <pc> 8) i z N = dlugosc instrukcji.
Stan przywrocony do bezpiecznego: program zamyka sie czysto, SYSTEM ZYJE.


## Warstwa "system zarejestrowany w sobie" (uwaga uzytkownika - trafna)

Brakuje nie tylko reakcji na wywolania, ale REJESTRACJI systemu:
  Getcookie (GEMDOS 0x14)  - ZAIMPLEMENTOWANE (cookie jar w tos_prg.c)
  _GEM_ 1.03, _VDI_ 3.00, _MCH, _CPU, _FPU (0 = brak FPU), _AKP, _VDO
Cel: _FPU = brak, zeby programy nie sondowaly instrukcja Line-F.
Jeszcze do dodania:
  - _sysbase / sysvars (v_bas_ad, v_nex) - SysInfo czyta stan VDI,
  - _VDI_ z prawdziwym adresem wejscia VDI (programy czesto wolaja VDI
    przez wskaznik, nie przez trap #2),
  - Sversion (XBIOS 0),
  - _MCH wg realnej maszyny, _CPU 0x1E = 68030.

Wynik testu: Getcookie wgrane, ale SysInfo nadal pada z vec=11 (Line-F)
zanim zdazy skorzystac z cookies - czyli sonduje FPU sprzetowo.
Nastepny krok: (a) dopisac _sysbase/sysvars, (b) pominac Line-F w
kernel_entry.S: kernel_line_f (0x4025A) z DLUGOSCIA ODCZYTANA z opcode,
(c) sprawdzic, czy program w ogole wola Getcookie (log wywolan 0x7e8dc).


## ADRESY SYSTEMOWE ZE ZRODEL MAGIC (OSBIND.INC) - klucz do SysInfo

KAOS/UTILS/OSBIND.INC:
  _sysbase  EQU $4f2    ; wskaznik do naglowka SYSHDR (sysvars)
  _v_bas_ad EQU $44e    ; ADRES EKRANU (longword)  <- Physbase()
  _hz_200   EQU $4ba    ; licznik 200 Hz

Dlaczego to jest przyczyna: SysInfo czyta *(long*)0x44E i *(long*)0x4F2.
U nas sa ZERA -> program podaza pod NULL -> zapisy pod $0 (dokladnie nasz objaw).
To jest ta "uruchomiona kompatybilnosc", o ktorej mowil uzytkownik.

Do wdrozenia w kernel_main po inicjalizacji grafiki:
  *(uint32*)0x44E = framebuffer (z kgfx_framebuffer)
  *(uint32*)0x4F2 = &tos_syshdr (blok 512 B, dolaczony do tos_prg.c)
  *(uint16*)0x4BA = 0 (potem zwiekszany przez timer)
Uwaga implementacyjna: zapis przez *(volatile uint32_t*)0x44E konczy sie
bledem -Werror=array-bounds (kompilator traktuje staly adres jako tablice
rozmiaru 0). Nalezy uzyc inline asm (move.l) albo skompilowac ten fragment
z -Wno-array-bounds. Blok tos_syshdr[512] jest juz dodany w tos_prg.c.


## ZMIERZONE FAKTY (koniec sesji) - sprzecznosc do rozstrzygniecia

1. Tablica wektorow PO uruchomieniu SysInfo (debugger, m 0 48):
     vec11 (0x2C) = 0x0004025A = kernel_line_f (kernel_entry.S)
   a NIE 0x54030 (asf_tos_fault11 z persony).
2. kernel_line_f ma zapisywac PC/opcode/6 slow ramki. Zrzut pokazal:
     LINE-F PC=0 OP=0
     FRAME: 0 0 0 0 0 0
   czyli handler NIE wykonal ani jednego zapisu.
3. A jednoczesnie na ekranie jest 'TOS FAULT vec=11', ktore wypisuje
   asf_tos_fault (tos_prg.c:96) - a to jest wolane TYLKO przez stuby
   tos_fault_stub z tos_entry.S.
4. Ale asf_tos_fault11 w tos_entry.S to SKIP:
     asf_tos_fault11: addq.l #2,2(%sp) / rte   (nie wola asf_tos_fault!)
5. kernel_user_fault (kernel_exception.c:1195) ustawia status = -(0x100+vector),
   czyli dla 11 = -0x10B - i to jest kod wyjscia programu.

WNIOSEK: sprzecznosc. Albo aktywny jest INNY wektor niz w tablicy pod 0,
albo 'TOS FAULT vec=11' pochodzi z innej sciezki (np. persona sama konczy
program kodem bledu 11 przy zamykaniu). Nowy kernel NA PEWNO dziala - nowe
linie LINE-F/FRAME sie wypisuja.

NASTEPNY TEST (jeden): hatari-debug b pc=4025a - jesli NIE trafi, to
kernel_line_f nie jest wolany i szukamy w personie; jesli trafi, to uklad
ramki na 68030 jest inny niz SR(2)+PC(4).

Zrodla do analizy: magic-src/ (MagiC + KAOS) oraz Hatari Falcon EasyMint
256C HDD.zip (EasyMiNT) - porownac, jak MagiC/FreeMiNT traktuja Line-F
(TOS/MagiC: pominac instrukcje i zwrocic 'brak FPU').


## Zasady pracy (ustalone z uzytkownikiem)

1. HATARI ZAWSZE WIDOCZNE. Nie uzywac trybu headless (SDL_VIDEODRIVER=dummy)
   do testow, ktore uzytkownik ma oceniac. Uruchamiac okno i zostawiac je
   otwarte, zeby uzytkownik mogl patrzec i pomagac.
2. KOLEJNOSC: najpierw DESKTOP ma dzialac, potem dopiero SysInfo.
   Desktop to srodowisko; program TOS jest jego konsumentem.
3. Komenda "gem": shell tekstowy -> desktop -> menu "Return to shell".
   Hook juz istnieje: kgfx_enter_desktop() / kgfx_restore_console()
   (kernel_tos_set_video(enter, leave, ...) w kernel_main.c).


## PRZELOM: EmuTOS dziala - kompletne srodowisko GEM

Ustalenie uzytkownika: "trzeba odpalic magic/emutos komenda gem od poczatku do konca,
ma wczytac wszystkie potrzebne procedury i odpalic wszystko lacznie z desktopem".

CO MAMY:
  atari-030-port/disk/etos512us.img      - EmuTOS 512K US  (DZIALA - pulpit GEM!)
  atari-030-port/disk/etos512pl.img      - EmuTOS 512K PL
  atari-030-port/disk/emutos-us-256k.img emutos-pl-192k.img emutos-pl-256k.img
  magic-src/KAOS/IMAGE/KAOS_142B-de.img  - KAOS/MagiC 1.4.2 (192 kB, desktop+DESKTOP.INF)
  files-pasted-by-the-user-projekt/work/emutos-prg256-1.4.zip - EmuTOS jako PRG

DOWÓD (zrzut ekranu, Hatari):
  Desk/File/View/Options + menu, ikony DISK A/B, TRASH, PRINTER,
  TLO PULPITU = SZACHOWNICA 50% (dither - dokladnie ten, ktory probowalem rysowac),
  status: "4MB MegaST, EmuTOS 1.4.0, MONO 71 Hz, A: B: HD:"
  -> EmuTOS widzi nasz work.img jako HD (jest tam SYSTEM/TEST/SYSINFO.PRG)

KAOS/MagiC: startuje (boot sektor + loader + test pamieci), ale 4 BOMBY =
  ILLEGAL INSTRUCTION, bo KAOS 1.4.2 patchuje TOS 1.04, a my dajemy 2.06.
  -> rozwiazanie: TOS 1.04 albo EmuTOS (EmuTOS jest calym systemem, wiec nie ma
     tego problemu).

KLUCZOWE NAPRAWY TEJ SESJI (bez nich kompletny system nie wstalby u nas):
  1. kernel_video_boot.c: usuniety warunek "cpu!=0" - wymagal 68000, odrzucal
     68030 -> ET4000 NIGDY nieaktywowany -> card=0 -> kvideo_flush nie kopiowal
     -> EKRAN ZAMROZONY. To uniewaznialo WSZYSTKIE wczesniejsze poprawki.
     Dowod: log "VOFA-MODE: drive=1, 640x480" + migajacy kursor.
  2. kgfx_framebuffer: zwraca ZYWY graphics.framebuffer (bylo: zawsze
     desktop_framebuffer) - kompozytor i kvideo_flush musza uzywac tego samego.
  3. Paleta mono: 0=czarny, 1=bialy (inaczej tekst i dither = czarne na czarnym).
  4. kernel_wm: odswiezanie powierzchni co klatke + flush NA KONCU klatki.
  5. Diagnostyka vid=active/calls/copies - dzieki niej znalazlem drive=0.

PLAN (komenda "gem" w naszym kernelu):
  nasz kernel (ET4000 drive=1, ekran zywy, GEMDOS, loader PRG+relokacje,
  fault containment, cookies, sysvars) -> komenda "gem" -> zaladuj kompletny
  system (EmuTOS) i przekaz mu kontrole -> DESKTOP GEM -> SysInfo dziala
  -> wyjscie -> powrot do naszego shella (kgfx_restore_console).

Zasady pracy: Hatari ZAWSZE widoczne; kolejnoSC: najpierw desktop, potem SysInfo.


## 2026-09-21 — ODBUDOWA PO AWARII (WAZNE)

BLAD: skrypt czyszczacy filtrujacy linie WYZEROWAL system/kernel_entry.S (0 B)
      i nadpisal system/kernel_main.c niepasujaca kopia z innej wersji kodu.

RATUNEK (wykonany):
  1. kernel_entry.S przywrocony z files-pasted-by-the-user-projekt/system/ (16262 B)
  2. kernel_main.c NIE mial kopii z sesji, ALE build/system/kernel_main.o ja zawiera
     -> kernel zlinkowany z GOTOWEGO .o (build=0, KERNEL.SYS 110910 B, zainstalowany)
  3. braki linku uzupelnione definicjami w tos_prg.c:
     kernel_linef_pc, kernel_linef_op, kernel_linef_frame[6]
     volatile uint32_t kernel_wall_ticks = 0u;
  4. kopia zapasowa: commander-v9/backup-20260921-073856/ (120 zrodel + .o + .elf)
  5. Aby build nie przebudowal zepsutego zrodla: touch -t 202501010000 system/kernel_main.c

ODTWORZENIE kernel_main.c — LITERALY Z kernel_main.o (kolejnosc bootu):
  ATARI SYSTEM KERNEL 0.03
  EXCEPTION VECTORS: INSTALLED / MEMORY MANAGER: READY|FAILED
  GRAPHICS BACKEND: SHIFTER  / CLIP+DIRTY / BLITTER READY / BLITTER CPU FALLBACK
  SURFACE MANAGER: ... SELF-TEST FAILED / SELFTEST OK
  MEMORY SELF-TEST: ...  ST-RAM POOL: ... KB FREE  TT-RAM POOL: ... KB FREE
  ATARI SYSTEM KERNEL 0.03 / TEXT CONSOLE 80x25
  ACSI DMA DRIVER (ABI 3): ... / BIOS FALLBACK / INVALID IMAGE / NO ST-RAM / I/O FAILED
  BLOCK LAYER / BLOCK CACHE: ... SECTORS / TT-RAM / OFF / LOW MEMORY / NO MEMORY
  AHDI VOLUMES: N / MORE DEFERRED / BOOT / DEFERRED / ... MORE VOLUMES
  FAT LAYER: FAT16 MOUNTED / PARTITION INVALID / BPB INVALID
  BOOT VOLUME: <name> BPS / VOLUME FLAG: CLEAN
  CORE MODULE/SERVICE: READY / VFS MODULE/SERVICE: READY
  PROCESS MANAGER: 2 TASKS / FULL CTX RR OK  TIMER/SLEEP QUEUE: 40 / WAKE OK
  EVENT QUEUE: WAIT / WAKE OK  IKBD INPUT: POLLING READY
  KERNEL STATUS: RUNNING / PREEMPTIVE / SHELL READY | SHELL UNAVAILABLE
  STARTUP SERVICES: SYSTEM.INIT READY / SYSTEM.GFX READY / SYSTEM.GUI READY
  8 HANDLES / OWNER GUARDS
  EXCEPTION VECTORS / [PID ... KILLED / WATCHDOG / FAULT / EXIT
  AUTOEXEC: \SYSTEM\TEST\SYSINFO.PRG / AUTOEXEC: OK / program closed / AUTOEXEC: none
  GEM: DESKTOP STARTED     <- komenda gem
  SYSINFO: OK / SYSINFO: closed
  PRESS RESET

ZMIANY SESJI w kernel_main.c (znane co do linii):
  - kgfx_init(2u,...) mono 640x400; kconsole_init(1,...); kconsole_set_grid(80u,18u) PO init
  - paleta mono: kgfx_set_palette(0,0x0000); kgfx_set_palette(1,0x0fff)
  - sysvars przez lm_wr32/lm_wr16: 0x44E=framebuffer, 0x4F2=&tos_syshdr, 0x4BA=0
  - AUTOEXEC: const char *volatile autoexec="gem";
      if gem: h->console_puts("GEM: DESKTOP STARTED"); ksessions_control(3u,0u);
              kernel_wm_set_desktop_session(2); kgfx_enter_desktop();
              kernel_tos_run_program("\SYSTEM\TEST\SYSINFO.PRG");
  - linia DBG: front= kgfx_framebuffer / desktopfb / active / wmc=a/b/c/d / vid=
  - petla glowna: ksessions_pump; if(!kernel_wm_console_visible())kconsole_flush();
      kernel_wm_frame_tick(ticks); if(!kgfx_desktop_active())ksessions_paint(); kvideo_flush();
      (UWAGA: kvideo_flush() na koncu — jego brak zerwal nawiasy klamrowe)

TEST ZGODNOSCI po odtworzeniu:
  strings build/system/KERNEL.SYS | grep -c "DESKTOP STARTED"   -> 1
  strings build/system/KERNEL.SYS | grep -c AUTOEXEC             -> 4
  rozmiar KERNEL.SYS ok. 110910 B

REGUŁY (od teraz obowiazujace):
  1. KOPIA przed kazda masowa edycja (backup-<data>/).
  2. NIGDY nie czyscic zrodla filtrowaniem linii po zawartosci.
  3. .o to kopia zapasowa kodu — link z gotowych .o ratuje sesje.
  4. Handler wyjatku MINIMALNY: tylko move.l do statycznej zmiennej;
     ZADNYCH wypisow, WM, grafiki. Wypis — z NORMALNEGO kontekstu.
  5. Pomiar bezpieczniejszy od zmiany "na wyczucie".

## 2026-09-21 — SESJA REKONSTRUKCJI (DSH): kernel_main odtworzony, zadania 2-4

Stan wyjsciowy: build/system/kernel_main.o (kod sesji) linkowany, a
system/kernel_main.c to niepasujaca kopia (18713 B, nie kompiluje sie z
aktualnymi naglowkami).

### Zadanie 1 — odtworzenie system/kernel_main.c  [UKONCZONE, ZWERYFIKOWANE]
- Zrodlo odtworzone z disasm .o + stary commander/system/kernel_main.c + literaly.
- Zgodnosc obiektu: tos_text/tos_fill/tos_enter_screen 42/16/18 B (identyczne),
  kernel_print_completion.constprop.0 136 B, rodata.str1.1 83 B, table.0 36 B,
  rodata.kernel_main.str1.1 1703 vs 1704, .text.kernel_main 6034 vs 6136 B
  (roznica register allocation). Sygnatury symboli identyczne.
- KERNEL.SYS = 110846 B; "DESKTOP STARTED" x1; "AUTOEXEC" x4.
- Handoff kopiowany petla bajtowa (bez memcpy), VBR=0 przez raw .long 0x4e7b0801,
  sysvars przez inline asm lm_wr32/lm_wr16 (obejscie -Werror=array-bounds).

### Bootstrap uzyty do testu (wazne)
- commander-v9/build/bios_mste.bin jest ACSI-only (loader/bootstage.c) i szuka
  SYSTEM/BOOT.SYS — nie obsluguje IDE.
- files-pasted-by-the-user-projekt/build/bios_mste.bin obsluguje IDE, ale szuka
  SYSTEM/BOOT/BOOT.SYS. Handoff pierwszych 52 B zgadza sie z v9, wiec tym BIOS-em
  udalo sie wystartowac v9 kernel: dodano SYSTEM/BOOT/BOOT.SYS i KERNEL.SYS w
  SYSTEM/KERNEL/KERNEL.SYS. ACSI: --acsi 0=work.img.
- Test Hatari (widoczne okno): pulpit/sesja [3] rysuje sie, zgodnie z oryginalem.

### Zadanie 2 — bezpieczna diagnostyka faultu  [UKONCZONE]
- kernel_exception.c: volatile uint32_t kernel_fault_vector/pc/sr/frame[6]
  + kernel_fault_report(puts,put_u32) wolane z NORMALNEGO kontekstu
  (kernel_main po powrocie programu TOS).
- kernel_entry.S: makro kernel_capture_fault zapisuje ramke wylacznie move.l,
  ZERO wypisow/WM/grafiki w wektorze; wektory bus(2)/address(3)/line_f(11).
- Wymuszone rozszerzenie bra.s -> bra (makro zwiekszylo odleglosc do kernel_fault).
- Test: wariant Task1+2 bootuje identycznie jak oryginal (sesja [3] widoczna).
- Odczyt ramki (symbole z build/system/kernel.elf):
  kernel_fault_vector=0x5c7fa, pc=0x5c7f6, sr=0x5c7f2, frame=0x5c7da.

### Zadanie 3 — odczyt PC/addr -> brakujaca funkcja  [DO DOKONCZENIA]
- Ramka jest przechwytywana; trzeba ja odczytac po faultcie (Hatari debugger,
  'hatari-debug m <addr> <n>') i na tej podstawie dopisac brakujaca funkcje
  XBIOS/VDI-stan/AES/sysvars.

### Zadanie 4 — okna programow w kompozytorze  [CZESCIOWO]
- DODANE: kernel_wm_app_surface rejestruje okno takze gdy pulpit jest aktywny
  (console_hidden || kgfx_desktop_active); kernel_tos_use_surface() w tos_prg.c
  daje programowi TOS powierzchnie okna zamiast wspolnego framebufferu;
  kernel_main (GEM) ustawia surface + tos_vdi_set_surface + sysvar 0x44E.
- REGRESJA (wycofana): zmiany w kernel_wm_frame_tick (wczesny powrot + wymuszone
  damage) zamieniaja ekran w czern bez sesji [3]. Wracamy do stabilnej petli klatki.
- SysInfo nadal nie pojawia sie w oknie; wymaga debugowania frame_tick i tego,
  czy pulpit GEM jest aktywny (kgfx_desktop_active) w tym stanie.

### Kopie / materialy
- backup-recon-20260921055136./ : kopia zrodel przed zmiana i oryginalny
  110910 B KERNEL.SYS + kernel_main.o.
- commander-v9/work/work.img.bak-recon : obraz przed instalacja.
- recon/ : disasm dobrego .o, porownania rodata, warianty task12/task24, kopia
  oryginalnego kernel_main.c.
- Zmienione pliki: system/kernel_main.c (odtworzony), system/kernel_entry.S,
  system/kernel_exception.c, system/tos_prg.c, system/kernel_wm.c.

## 2026-09-21 — SESJA DSH, CZESC 2: okna kompozytora dzialaja; fault SysInfo zlokalizowany

### Diagnoza stanu (debugger Hatari) i naprawy
Pomiary pamieci (adresy z build/system/kernel.elf) wykazaly:
- desktop_is_active=1, desktop_hidden=1 -> kgfx_desktop_active()=0.
  Przyczyna: kernel_user_desktop_visible() znal tylko natywne GUI (gui_desktop_pid),
  a persona TOS nie byla reprezentowana, wiec ksessions_pump ciagle ukrywal pulpit.
- console_registered=1, console_hidden=0 -> na ekranie byl konsola/sesja [3].

Naprawy (Task 4):
1. kernel_exception.c: dodane volatile uint8_t kernel_tos_desktop_visible i OR w
   kernel_user_desktop_visible(); kernel_tos_use_surface() (tos_prg.c) ustawia flage.
2. kernel_wm.c: kernel_wm_console_hide() zeruje console_registered, wiec warunek
   wczesnego powrotu zmieniony na (!console_registered && !wm.count) - inaczej
   kompozytor nie dochodzil do rysowania okien.
3. kernel_wm.c: w trybie pulpitu z oknami wymuszane kernel_wm_damage_all() na klatke,
   zeby bezposredni dither (linia 143) nie zjadal okna aplikacji.
Wynik (zrzut Hatari): widoczny pulpit 50% dither + okno aplikacji (biały pasek
tytulu 400x294 w (8,8), czarny klient). Okno jest puste, bo SysInfo nie zdazyl
nic narysowac - pada wczesniej.

### Task 3: fault SysInfo zlokalizowany
- tos_exit_code = -4 => asf_tos_fault(vector 4) = ILLEGAL INSTRUCTION.
- Stuby persony (tos_entry.S, makro tos_fault_stub) przechwytuja teraz surowa ramke
  w kernel_fault_frame[6] (move.l only), a asf_tos_fault zapisuje PC/opcode do
  kernel_linef_pc/kernel_linef_op (odczyt z normalnego kontekstu/debuggera).
- Zmierzona ramka: 25 08 00 02 00 00 00 10 01 01 89 c4 00 00 00 00 ...
  kernel_linef_pc = 0x010189C4 (blisko PC z poprzedniej sesji $1018a66/$101886c).
  Odczyt opcode pod tym adresem zwrocil 0 - uklad ramki 68030 wymaga weryfikacji
  (format/vector vs SR+PC); nastepny krok: zlamac ramke na 68030 i odczytac opcode
  w handlerze albo breakpointem Hatari na asf_tos_fault4.
- app_surface (0x6b6da) jest zerowa -> SysInfo nie narysowal nic przed faultem.

### Stan plikow (najnowszy build KERNEL.SYS = 111912 B)
- kernel_main.c (odtworzony) + kernel_entry.S + kernel_exception.c (Task 2 + flaga TOS)
- tos_entry.S (przechwyt ramki persony) + tos_prg.c (kernel_tos_use_surface,
  asf_tos_fault zapisuje PC/op)
- kernel_wm.c (okna aplikacji w trybie pulpitu)
Kopie: backup-recon-20260921055136./, work.img.bak-recon, recon/.

## 2026-09-21 — SESJA DSH, CZESC 3: Task 3 — dokladna przyczyna faultu SysInfo

### Trace wyjatkow CPU (Hatari --trace cpu_exception)
Kluczowy wpis:
  cpu exception 4 vector 10 currpc 20000 newpc 548fc SR 2508
=> ILLEGAL INSTRUCTION (exception 4) pod PC = 0x00020000, obsluzony przez
   asf_tos_fault4 (newpc 548fc), SR=0x2508. Potwierdza to wczesniejszy breakpoint:
   stub asf_tos_fault4, na stosie vector=4 i ramka 25 08 00 02 00 00 00 10 ...

### Sciezka w kodzie SysInfo (odczyt debuggerem, user code ~0x010189a0)
  010189a0  movel a6,$0004b8a4
  010189a6  clr.w $0004b8c4
  010189ac  clr.l $0004b8c6
  010189b2  clr.l $0004b8ca
  010189b8  ...
  010189be  jsr   $0000316a          ; callgate TOS w niskiej pamieci
  010189c4  move.w d0,-(a7)
  010189c6  move.l $0004b8c6,d0     ; <-- wskaznik wypelniony przez 0x316a
  010189cc  beq.b  +4
  010189ce  movea.l d0,a0
  010189d0  jsr   (a0)              ; <-- tu skok do 0x00020000 -> illegal
  010189d2  move.l $0004b8ca,d0
Po faultcie $0004b8c6 = 0 (program/inne wyczyscilo), a $00020000 nadal zawiera
stale "ASYSBOOT" (obszar obrazu BOOT.SYS, juz nieuzywany).

WNIOSEK (Task 3): brakujaca funkcja to CALLGATE w niskiej pamieci pod 0x0000316a
(wolany przez SysInfo) oraz struktura pod 0x0004B8C4/0x4B8C6/0x4B8CA, ktora ma
dostarczyc prawdziwy wskaznik wejscia (najprawdopodobniej _VDI_/AES albo Line-A
callgate). Nasz kernel nie wystawia jej, wiec do 0x4B8C6 trafia smieciowy
0x00020000 i program skacze w nieistniejacy kod.
Nastepny krok: ustalic co dokladnie TOS/MagiC trzyma pod 0x316a i 0x4B8C6
(zrodla magic-src/OSBIND.INC) i wystawic to w personie/memory_init.

### Potwierdzenie dzialania okien (Task 4)
Zrzut Hatari: pulpit 50% dither + okno aplikacji (pasek tytulu + czarny klient).
Okno jest puste, bo SysInfo pada (powyzej) zanim cokolwiek narysuje do app_surface.

## 2026-09-21 — SESJA DSH, CZESC 4: Task 3b — crash usuniety, ale SysInfo nie rysuje

### Pomiar geometrii zaladowania (nowe zmienne C)
Dodane w tos_prg.c: kernel_tos_dbg_basepage/entry/total. Odczyt debuggerem:
  basepage (mem) = 0x01018730   (TT-RAM)
  entry          = 0x01018830   (mem+0x100)
  total          = 0x066E58
Czyli program jest w TT-RAM, a nie w niskiej pamieci.

### Dlaczego byl illegal at 0x00020000
Kod startowy SysInfo (text offset 0x150..0x1b8, plik /tmp/SYSINFO.PRG):
  lea 256(a6); move.l -> 0x4B8A4
  clr.w 0x4B8C4 ; clr.l 0x4B8C6 ; clr.l 0x4B8CA
  move a5,a0 ; move a4,a1
  jsr 0x316A          <-- rutyna TOS/callgate: w naszym kernelu SAME ZERA
  move.l 0x4B8C6,d0 ; jsr (a0)     <-- wskaznik main, u nas 0 -> pominiety
  move.l 0x4B8CA,d0 ; jsr (a0)
  jsr 0x20720         <-- druga rutyna: u nas SAME ZERA
  move.w #76,-(a7) ; trap #1         (Pterm)
Sprawdzone debuggerem: 0x316A i 0x20720 = 00 00..., 0x20000 = stary "ASYSBOOT".
Zera wykonuja sie jako ciag ori.b i CPU dochodzi do 0x20000 -> illegal (vec 4).

### Wykonany krok (Task 3b, workaround)
tos_poke16() w tos_prg.c wpisuje 0x4E75 (rts) pod 0x316A i 0x20720 przed startem.
Wynik: kernel_linef_pc = 0 (brak faultu), tos_exit_code = 0 (czyste wyjscie).
ALE: wskazniki 0x4B8C6/0x4B8CA zostaja 0, wiec program pomija main i tylko
wychodzi - okno aplikacji pozostaje puste.

### Wniosek / nastepny krok
Prawdziwa naprawa: zaimplementowac callgate 0x316A tak, by wypelnial
0x4B8C4/0x4B8C6/0x4B8CA wskaznikami main/exit programu. Dodatkowo 0x4B8C4
lezy POWYZEJ bazy naszego kernela (0x40000) - program zaklada uklad pamieci,
w ktorym rom/OS nie zajmuje 0x40000..0x68E58. To wymaga decyzji o mapie
pamieci (przeniesienie kernela lub load programu wg jego oczekiwan).
Do czasu implementacji callgate: zostaje bezpieczny rts (brak crashu).

## 2026-09-21 — SESJA DSH, CZESC 5: co jest pod 0x316a i opcja TT-RAM

### 0x316a NIE jest rutyna TOS
Debugger (po starcie): 0x316A = 00 00..., 0x20720 = 00 00..., 0x20000 = stary "ASYSBOOT".
Analiza pliku SYSINFO.PRG (Pure C, 319937 B):
- text start file 0x1c; naglowek tlen=0x3384C, dlen=0x18052, blen=0x174BA.
- W tablicy startowej runtime'u (text+2) sa m.in. 0x0004B8AC, 0x00002000,
  0x00002008 oraz ASCII "Pure C". To sygnatura startupu Pure C.
- Kod startowy uzywa STALYCH, nierelokowanych adresow: 0x316A, 0x20720,
  0x4B89E, 0x4B8A2, 0x4B8A4, 0x4B8C4, 0x4B8C6, 0x4B8CA.
- Zadna z tych operacji nie jest w tablicy relokacji (i w ogole obszar
  0x170..0x1b0 nie ma relokacji). To program ZALEZNY OD ADRESU (staly basepage).
- Wartosci 0x2000/0x2008 w tablicy startowej + zasieg danych wskazuja na
  oczekiwany basepage ok. 0x2000 (text=0x2100, data=0x3594C, BSS do ~0x64E58).
  Wtedy 0x316A/0x20720/0x4B8xx leza w kodzie/danych programu.

### Dlaczego to peka u nas
Nasz loader daje mem=0x01018730 (TT-RAM). Stale odwozania trafiaja w wolna
pamiec (zera) i w kernel -> ucieczka do 0x20000 -> illegal (vec 4).

### TT-RAM: TAK, to wlasciwa naprawa (ale wieloetapowa)
Kernel zajmuje 0x40000..0xB0000 i koliduje z ukladem programu
(text 0x2100..0x3594C, data 0x3594C..0x4D99E, BSS do ~0x64E58).
Kroki:
  1. linker/kernel-sys.ld: . = 0x01000000 (kernel w TT-RAM)
  2. BIOS start_kernel: load_image(..., 0x01000000, 0x01000000, ...)
     (zrodlo files-pasted-by-the-user-projekt/loader/bootstage.c:62)
  3. przebudowa BIOS (bios_mste.bin) i kernela
  4. loader TOS (kernel_tos_run_program): alokowac w ST-RAM i ladowac program
     pod jego staly basepage (~0x2000), a nie w TT-RAM
Uwaga: kroki 2-3 zmieniaja lancuch bootowania (BIOS+BOOT.SYS+kernel), wiec
wymagaja kopii i testu; TT-RAM jest juz wykrywany przez BIOS (hw/memory.c,
fast_base=0x01000000), wiec technicznie wykonalne.

### Stan
Kernel 111956 B z bezpiecznym rts na 0x316A/0x20720 (brak crashu, exit 0),
ale SysInfo pomija main (wskazniki 0x4B8C6/CA = 0) dopoki nie ma wlasciwego
basepage. Hatari widoczny (bash-182).

## 2026-09-21 — SESJA DSH, CZESC 6: KERNEL W TT-RAM (dziala) + program pod stala basepage

### Przeniesienie kernela do TT-RAM — WYKONANE I DZIALA
Zmiany:
  1. linker/kernel-sys.ld: . = 0x01000000
  2. kernel_entry.S: naglowek .long 0x01000000 + offsety -0x01000000
  3. kernel_memory.c kmem_init: st_pool od 0x00070000 (ST-RAM wolny),
     tt_pool od kernel_end (= __image_memory_end)
  4. files-pasted loader/bootstage.c: load_image dostaje 0x01000000,
     a limit adresu to teraz ram_bytes+tt_ram_bytes (TT-RAM dozwolony)
  5. przebudowa BIOS (build/bios_mste.bin) i KERNEL.SYS
Wynik: kernel startuje z 0x01000000, __image_memory_end=0x0105a5ea,
pulpit dziala, w oknie widac tekst (tts1/tts3 C:\>, memory:
ST-RAM FREE 3580 KB / TT-RAM FREE 63037 KB). ST-RAM 0x0-0x70000 jest wolny.

### Program TOS ladowany pod staly basepage
kernel_tos_run_program: mem = (uint8_t*)0x00003000 (zamiast kmem_alloc w TT-RAM).
Debugger potwierdza: kernel_tos_dbg_basepage=0x3000, entry=0x3100,
0x4B89E = 0x3000 (startup zapisal basepage). tos_exit_code=0 (brak faultu).
ALE: 0x4B8C6/0x4B8CA (wskazniki main/exit) zostaja 0, wiec main jest pomijany.
Analiza: startup (text 0x20..0x18e) czyta basepage a3, wylicza SP i woła
'jsr 0x316a' (i potem wskazniki 0x4B8C6/CA). Dla B=0x3000 adres 0x316a trafia
w srodek petli Mshrink; dla B=0x2000 w srodek innej funkcji. Program jest
zalezny od adresu (stale 0x316a/0x20720/0x4B8xx) i wymaga dokladnie tej
basepage, dla ktorej te stale adresy sa poczatkami funkcji.
SYSI502.zip/SYSINFO.APP ma identyczny MD5 jak plik w obrazie (nie jest uszkodzony);
tablica relokacji zaczyna sie od 0x2, a dalej ma dane kodu (program praktycznie
nie ma relokacji), co potwierdza fixed-address.

### Nastepny krok
Znalezc dokladna wartosc basepage (funkcja startowa pod 0x316a). Kandydat:
B=0x2010 (0x316a = text+0x105a, poczatek funkcji), albo odczytac z debuggera
a3 przy starcie. Potem main ruszy i SysInfo narysuje do app_surface.

## 2026-09-21 — SESJA DSH, CZESC 7: analiza startu SysInfo w EmuTOS (zrodla)

### Zrodlo EmuTOS
Pelne drzewo: files-pasted-by-the-user-projekt/work/emutos-reference (z .git).

### Jak EmuTOS laduje .PRG (bdos/proc.c, alloc_tpa; bdos/kpgmld.c)
- Naglowek PGMHDR01 (bdos/pghdr.h): tlen,dlen,blen,slen,res1,flags(LONG),abs(WORD).
  Flagi: PF_FASTLOAD=1, PF_TTRAMLOAD=2, PF_TTRAMMEM=4.
- SysInfo (SYSINFO.APP) flags = 0x00070000 -> bit PF_TTRAMLOAD NIE ustawiony.
- alloc_tpa(): bez PF_TTRAMLOAD alokuje z ST-RAM: ffit(...,&pmd), zwraca m_start.
  Czyli EmuTOS laduje SysInfo w DOLNYM ST-RAM (pierwszy wolny blok), tak jak
  zakladalismy.
- p_lowtpa = blok (basepage), p_tbase = PD+sizeof(PD) (czyli basepage+0x100),
  p_dbase = tbase+tlen, p_bbase = ... - zgodne z naszym loaderem.
- autoexec (bios/bios.c:724): uruchamia \AUTO\*.PRG przez Pexec(PE_LOADGO)
  zanim wstanie desktop; dziala tylko GEMDOS (bez GEM/AES).

### Proby empiryczne pod EmuTOS
- EmuTOS 1.4.0 (etos512us.img) startuje do pulpitu GEM z work.img (HD widziany).
- Wgranie SYSINFO.PRG do \AUTO nie uruchomilo go (SysInfo to aplikacja GEM,
  a AUTO dziala przed AES); 0x4B89E pozostalo 0.
- Mikro-sonda BPAGE.PRG (zapis A0/A3/A5/4(sp) pod 0x300) tez nie dala basepage
  (0xE007F0 = ROM) -> AUTO w tej konfiguracji nie wykonal sie.

### WNIOSEK
1. SysInfo jest programem fixed-address; dziala, gdy jest w dolnym ST-RAM pod
   basepage wybranym przez OS (EmuTOS: pierwszy wolny blok ST-RAM).
2. Nasz kernel (po przeniesieniu do TT-RAM) moze go tam zaladowac - brakuje
   tylko DOKLADNEJ wartosci basepage. Pod EmuTOS trzeba ja zmierzyc z desktopu
   (mysz niedostepna przez FIFO) albo znalezc w zrodle (bios/biosmem.c,
   pierwszy wolny ST-RAM po rezerwacjach OS).
3. Alternatywa: nie zgadywac, tylko zaimplementowac w kernelu to, co robi OS:
   przydzielic programowi dokladnie pierwszy wolny blok ST-RAM i przekazac go
   jako basepage (tak jak EmuTOS).

## 2026-09-21 — SESJA DSH, CZESC 8: relokacja naprawiona; kernel w ST-RAM + TPA

### Wykryty blad: loader nie relokowal programow
Analiza /tmp/SYSINFO.PRG (md5 6e6a5f74a044fc2b751e404bf3f88a85 = plik z obrazu)
wykazala, ze naglowek to PGMHDR01: flags=0x00000007 (PF_FASTLOAD|PF_TTRAMLOAD|
PF_TTRAMMEM), abs=0 => program JEST relokowalny. Tablica relokacji zaczyna sie
na flen+0x1c (0x4B8BA): LONG relst=2, a dalej strumien bajtowych delt
(1 = +0xFE), dokladnie jak EmuTOS pgfix01. Stary loader czytal ten strumien jako
liste 32-bitowych offsetow, wiec prawie nic nie relokowal - to byla prawdziwa
przyczyna skoku SysInfo w niska pamiec. Poprawiono kernel_tos_run_program:
  relst = rd32(file+0x1c+slen+tlen+dlen);
  cp = text+relst; *(u32*)cp += text;  potem cp += delta (1 => +0xFE);
  kazdy trafiony longword += p_tbase.
Dekoder w Pythonie daje 10230 poprawek i wszystkie trafiaja w operandy
instrukcji (0x4b89e, 0x4b8a4, 0x4b8c6, 0x316a, 0x20720, ...), co potwierdza
poprawnosc.

### Kernel z powrotem w ST-RAM + TPA
- linker/kernel-sys.ld: . = 0x00080000 (zamiast 0x01000000)
- system/kernel_entry.S: naglowek 0x00080000 (4 stale)
- files-pasted loader/bootstage.c: start_kernel laduje KERNEL.SYS pod 0x00080000
- system/kernel_memory.c: st_pool od kernel_end, tt_pool od 0x01000000
- files-pasted build/bios_mste.bin przebudowany (0x80000)
- system/tos_prg.c: wlasny TPA [0x1000, __image_start), pierwszy wolny blok od
  dolu (EmuTOS alloc_tpa), LIFO release po powrocie; sysvars _membot (0x432)
  i _memtop (0x436) publikowane slowami (68000-safe).
Wynik zbudowania: __image_start=0x00080000, kernel_entry=0x00080020,
__image_memory_end=0x000da70e, KERNEL.SYS=112104 B.

### Wynik testu (Hatari, ET4000 mono, IDE)
kernel startuje z ST-RAM, pulpit + okno aplikacji kompozytuja sie poprawnie.
Debugger: kernel_tos_dbg_basepage=0x1000, entry=0x1100, total=0x66E58,
_membot=0x1000, _memtop=0x80000. Czyli TPA dziala i program jest w TPA.
ALE tos_exit_code=-4 (illegal), kernel_fault_pc=0x4288 = tbase+0x3188.
Czyli identycznie jak przy kazdym innym basepage: 'jsr $316a' (po relokacji
tbase+0x316a) trafia w region danych i wykonywany kod dochodzi do 'INFA'
(0x494E4641) -> illegal.

### Dlaczego SYSINFO.PRG nie rusza (analiza pliku)
W tekscie tego pliku segmenty maja NAPRZEMIENNA parzystosc instrukcji:
  0x0000-0x2000  : 4EB9 na offsetach parzystych
  0x2000-0x10000 : 4EB9 wylacznie na offsetach NIEPARZYSTYCH (189/189 itd.)
  0x10000-0x1A000: parzyste
  0x1D000-0x1F000: nieparzyste
  0x22000-0x3384C: parzyste
Na 68000/030 instrukcje musza byc wyrownane do 2 bajtow wzgledem adresu
bezwzglednego, wiec region 'nieparzysty' nie moze byc wykonywany - to przesuniete
o 1 bajt dane/skompresowany blok. Wlasnie w tym regionie leza cele trzech 'jsr':
0x316a, 0x360a, 0x3e04. Zaden basepage nie naprawi obu parzystosci jednoczesnie,
wiec plik jest wewnetrznie niespojny (skompresowany/sformatowany niestandardowo
albo uszkodzony). To nie jest problem loadera ani mapy pamieci.
Dodatkowo maly SYSINF83.PRG jest zapakowany (stub '** CODE BY HWAM-SOFT !! **',
skok do dekompresora 0xE412) i pod naszym kernelem robi Bus Error przy zapisie do
adresu $0 (PC=0x1336) - czyli tez wymaga dekompresora dzialajacego w TOS-owym
srodowisku.

### Nastepne kroki
1. Zdobyc CZYSTY, niepakowany SYSINFO.PRG (lub odpalic go pod EmuTOS/MagiC przez
   DESKTOP.INF i zmierzyc jego faktyczne zachowanie), zamiast pliku z obrazu.
2. Alternatywa: uzyc natywnego \SYSTEM\APP\SYSINFO.APP (dziala w kompozytorze) i
   traktowac TOS-owy SYSINFO.PRG jako test zgodnosci, a nie cel.
3. TPA/relokacja sa juz poprawne - kazdy prawidlowy .PRG (Pure C/GCC) zaladuje
   sie pod basepage 0x1000 i dostanie _membot/_memtop jak w TOS.

## Sesja 9 - Krok 0 ROZWIAZANY: dlaczego SYSINFO.PRG "konczy sie bez trap"

### Metoda
1. Hatari + --cmd-fifo: do emulatora wysylane sa polecenia debuggera przez
   `hatari-debug <cmd>` (samo 'd'/'r' jest odrzucane przez kontroler FIFO).
2. Zrzut calej TPA: `hatari-debug savebin /tmp/tpa.bin 0x1000 0x7f000`.
3. Emulacja warstwy 0 HWAM w Pythonie i porownanie z pamiecia.

### Ustalenia (twarde dane)
- `tos_call_log[0]=0`, `tos_aes_trace_n=0` => program NIGDY nie wykonuje
  trap #1/#2 (GEMDOS/AES/VDI). Wiec nie ma "wyjscia z kodem 0" - to nie exit.
- Basepage (0x1000) po fakcie: p_tbase=0x1100, p_tlen=0x13268, p_dbase=0x14368,
  p_dlen=0x6826, p_bbase=0x1AB8E, p_blen=0x67C6 - te pola nadpisal sam
  samo-rozpakowujacy sie program (nasz loader wpisal tlen=0xE4F8).
- Pamiec TPA po awarii: 0x1000-0xF000 gesty kod, 0xF000-0x17000 ZERA,
  0x17000-0x1B000 teksty SysInfo ("INVALID ONLY STBANK", "ENVIRONMENT
  VARIABLE MANAGER"...), 0x1B000-0x71D70 zera, 0x71D70-0x80000 drugi obraz.
- Kod pod p_tbase=0x1100 to krotki stub:
      1100: 41fa fefe   lea (pc),a0      ; a0 = 0x1000 (basepage)
      1104: 4fe8 0100   lea 256(a0),sp   ; sp = 0x1100
      1108: 4eb9 0000 faaa  jsr 0xfaaa
  a 0xFAAA leza SAME ZERA (0xfaaa > konca zaladowanego tekstu 0xF5F8 i > konca
  zdeszyfrowanego obszaru ~0xF000).
- Skutek: CPU wykonuje dlugi ciag zer (`ori.b #0,d0` = 0x0000), dochodzi do
  danych programu i tam sie wywraca:
    - przebieg 1: PC=0x17D04, `neg.w (a2)` z a2=0 -> BUS ERROR (vec 2,
      tos_exit_code=-2), USP=0x10FC (stos tuz pod wejsciem - nic nie zdazylo
      sie wykonac),
    - przebieg 2 (nowy kernel): PC=0x1795C, ILLEGAL (vec 4, exit=-4), USP=0x5D44.
  Awaria jest NIEDETERMINISTYCZNA (zalezy od smieci w pamieci), co dodatkowo
  potwierdza, ze wykonuje sie niezainicjalizowany obszar.

### Wniosek
SYSINFO.PRG (HWAM-SOFT x3 + pakowacz) rozpakowuje sie tylko CZESCIOWO: tekst
deklarowany w naglowku wewnetrznym ma 0x13268 B, a wypelnione zostaje jedynie
~0xF000-0x1100 = 0xDF00 B; obszar [0xF000, p_tbase+tlen) zostaje zerowy (albo
"skompresowany" obraz ladnie w [0x71D70,0x80000)). Program robi skok do swojej
wlasnej, jeszcze nie rozpakowanej czesci tekstu (jsr 0xFAAA) i ginie. To nie jest
brak trap-a ani blad mapy pamieci - to niekompletne samo-rozpakowanie.

### Wzorzec z EmuTOS (referencja TPA)
EmuTOS 1.4 (ten sam Hatari: MegaST+TF536+VOFA+64MB TT):
  _membot (0x432) = 0x00023CA0
  _memtop (0x436) = 0x003F8000   (~4 MB, TPA ~3.7 MB)
Nasz kernel: _membot=0x1000, _memtop=0x80000 (TPA ~520 kB, bo kernel stoi w
ST-RAM pod 0x80000). Pakowacz liczy bufor rozpakowania z geometrii TPA, wiec
7x mniejszy TPA to glowny podejrzany.

### Zmiana wykonana w tej sesji (male, bezpieczne, zweryfikowane buildem)
- system/tos_entry.S: stuby bledow zapisuja teraz takze USP
  (`move.l %usp,%a0; move.l %a0,kernel_fault_usp`) - pelny slad stosu uzytkownika
  przy awarii bez potrzeby grzebania w Hatari.
- system/kernel_exception.c: definicja i raport `kernel_fault_usp` w
  `kernel_fault_report()` (" usp=").
- KERNEL.SYS = 112450 B, zainstalowany w work.img (SYSTEM/KERNEL/KERNEL.SYS).
  Nowy symbol: kernel_fault_usp=0x9cc88, kernel_fault_pc=0x9cca8.

### PRZELOM: TPA w TT-RAM odblokowuje SysInfo
Zmiana (nadal w tej sesji):
- system/tos_prg.c: TOS TPA przeniesiony do TT-RAM:
    TOS_TPA_BASE = 0x04000000, TOS_TPA_TOP = 0x05000000  (top 16 MB z 64 MB)
  _membot/_memtop publikowane jako 0x04000000/0x05000000, p_hitpa=0x05000000.
- system/kernel_memory.c: koniec puli TT obciety do 0x04000000 (zostawia gorna
  czesc TT-RAM dla persony TOS); reszta pamieci jadra bez zmian.

Efekt (Hatari, ten sam config):
- kernel_tos_dbg_basepage = 0x04000000, entry = 0x04000100
- kernel_fault_pc = 0, kernel_fault_usp = 0  -> ZADNEGO bledu
- tos_call_log[0] = 1 (pierwsze GEMDOS), tos_aes_trace_n = 21
- SysInfo zyje jako task (USP=0x0401E07E w TT-RAM), pulpit + okno sie kompozytuja.

Czyli teza o geometrii TPA byla trafna: maly TPA (0x1000..0x80000) lamal
samo-rozpakowanie, duzy TPA jak w TOS je naprawia.

### PRZELOM 2: poprawny ABI trap #2 (GEM)
Zdemontowany kod SysInfo w TT-RAM pokazal (i potwierdza to MagiC
LIB/MT_AES/SRC/MT_AESS.S oraz LIB/MAGIXLIB/SRC/AES.S):
    move.w #200,d0        ; 0xC8 = AES   (0x73 = 115 = VDI)
    move.l #aespb,d1      ; d1 = wskaznik na blok parametrow
    trap #2
  AESPB: {contrl, global, intin, intout, addrin, addrout}   (6 wskaznikow)
  VDIPB: {contrl, intin, ptsin, intout, ptsout}             (5 wskaznikow)
Stary asf_tos_trap2 blednie traktowal d0..d4 jako piec tablic, wiec kazde
wywolanie GEM widzialo smieci (contrl wskazywalo na mala liczbe) - dlatego nic
sie nie rysowalo, a opcody w trace byly bezsensowne (0x00E0...).

Poprawka:
- system/tos_entry.S: asf_tos_trap2 czyta blok z d1; rozgalezienie wg d0
  (0xC8 AES / 0x73 VDI / inne = zwrot 0), szosty argument is_aes.
- system/tos_prg.c: asf_tos_aesvdi(...,int is_aes); dla is_aes==0 idzie prosto
  do tos_vdi_call (opcody AES i VDI sie nakladaja, np. 100 = wind_create =
  v_opnvwk), dla AES obsluguje przelacznik AES i default zwraca 0.

Efekt: trace pokazuje PRAWDZIWE opcody AES:
  10 appl_init, 77 graf_handle, 100 wind_create, 39 wind_calc, 129, 104
  wind_get, 32, 17, 15, 16, 108, 25 evnt_multi, 23 evnt_mesag,
  20 evnt_keybd, 18, 19, ... 
Zadnego bledu (kernel_fault_pc=0), SysInfo zyje. Okno kompozytora nadal czarne -
brakuje odpowiedzi AES (wind_get/objc_*/graf_*) i rysowania VDI, ktore SysInfo
potrzebuje, zeby narysowac interfejs.

### Nastepny krok (konkretny)
1. Rozszerzyc minimalny AES o to, o co SysInfo pyta w pierwszych 21 wywolaniach
   (wind_get pola, wind_calc, graf_handle, menu_*, rsrc_*, objc_*), tak aby
   przeszlo do form_do/evnt_multi i narysowalo drzewo obiektow.
2. Podlaczyc VDI (contrl[0]=100 v_opnvwk itd.) do powierzchni okna kompozytora
   (kernel_wm_app_surface/TOS pid 0x544f53) - rysowanie ma trafiac do okna.
3. Sprawdzic multitasking: druga sesja (shell) ma dzialac, gdy SysInfo czeka.
4. Zaktualizowac wpis w planie po kazdym dzialajacym kroku.

## Sesja 10 - minimalny VDI/AES dziala, SysInfo dochodzi do konca inicjalizacji

### Co naprawiono (zweryfikowane buildem + zrzutem z Hatari)
1. Tablica opcodow VDI byla bledna. SysInfo wola opcode 100 = v_opnvwk, a
   nasza stala VDI_V_OPNVWK byla 1 (czyli v_opnwk), wiec otwarcie stacji
   roboczej spadalo do default i zwracalo 0 -> SysInfo nie mial VDI.
   Poprawione wg tabeli VDI (jmptb1 1..39, jmptb2 100..134):
   v_opnvwk=100, v_clsvwk=101, vst_height=12, vst_rotation=13,
   vsl_type=15, vsl_width=16, vsl_color=17, vsm_type=18, vsm_height=19,
   vsm_color=20, vst_font=21, vst_color=22, vsf_interior=23, vsf_style=24,
   vsf_color=25, vswr_mode=32, vqt_attributes=38, vst_alignment=39,
   vq_extnd=102, vsf_perimeter=104, v_get_pixel=105, vst_effects=106,
   vsl_ends=108, vqt_extent=116, vqt_width=117, vq_mouse=124, vq_key_s=128,
   vs_clip=129. (Poprzednio 12/19 byly pomylone z alignment/marker.)
2. v_opnvwk zwraca teraz prawidlowy uchwyt (intout[0]=contrl[6]), a nie
   szerokosc ekranu; uzupelniona tablica mozliwosci (intout[1..]).
3. Dolozony minimalny VDI: settery atrybutow, vqt_attributes (font/kolor/
   obroty/wyrownanie + komorka 8x15/8x16), vql/vqm/vqf_attributes, vq_extnd,
   vqt_extent, vqt_width, vs_clip, v_get_pixel, vq_mouse, vq_key_s.
4. Malloc(-1): GEMDOS Malloc(-1) nie alokuje, tylko zwraca rozmiar
   najwiekszego wolnego bloku (tak dziala PFX). Nasz kod traktowal -1 jako
   zadanie 4 GB i zwracal 0. Poprawione.

### Wynik (Hatari, ten sam config)
SysInfo dziala jako task w TT-RAM, bez bledu (kernel_fault_pc=0), i wykonuje
DOKLADNIE 21 wywolan GEM:
   AES 10 appl_init, AES 77 graf_handle,
   VDI 100 v_opnvwk (uchwyt=1),
   VDI 39 vst_alignment, 129 vs_clip, 104 vsf_perimeter, 32 vswr_mode,
   17 vsl_color, 15 vsl_type, 16 vsl_width, 108 vsl_ends, 25 vsf_color,
   23 vsf_interior, 24 vsf_style, 22 vst_color, 106 vst_effects,
   13 vst_rotation, 20 vsm_color, 18 vsm_type, 19 vsm_height,
   38 vqt_attributes
...a potem konczy przez Pterm0: tos_dbg_exit_fn=0, tos_dbg_exit_pc=0x0400EB44,
tos_exit_code=0. Czyli inicjalizacja GEM/VDI przechodzi, ale program wychodzi
zanim wykona wind_create/menu/form_do (brak wywolania AES 100+).

### Nowa diagnostyka
- system/tos_prg.c: tos_exit_usp + tos_exit_stack[16] - przy Pterm/Pterm0
  zrzucany jest stos uzytkownika, wiec lancuch powolan do wyjscia da sie
  zdemontowac bez breakpointow.
- Zrzut tego wyjscia: usp=0x0401E07E, na stosie 0x0048, 0xFFFFFFFF oraz
  adres 0x0400010E. Kod pod 0x0400EB3A wyglada jak
  pea -1; movew #0x48,-(sp); trap #1 (Malloc(-1) = rozmiar bufora PFX), a
  tos_dbg_exit_pc=0x0400EB44 wskazuje trap tuz za nia.

### Co zostaje
- Ustalic dokladny warunek wyjscia (dlaczego po vqt_attributes nie ma
  wind_create): zdemontowac kod wokol 0x0400EB00-0x0400EB60 na obrazie TT-RAM
  albo zlamac na Pterm0 i zdjac historie PC.
- Potem: odpowiedziec na wind_*/objc_*/rsrc_*/form_do i podlaczyc v_gtext do
  czcionki, zeby SysInfo narysowalo drzewo obiektow w oknie kompozytora.

## Sesja 11 - PRZELOM: GEMDOS to funkcja na STOSIE, nie w d0

### Zly ABI trap #1/#13/#14 byl prawdziwa przyczyna przedwczesnego wyjscia
EmuTOS bdos/rwa.S `_enter` czyta slowo funkcji z wierzcholka stosu
uzytkownika (uzywa je tez do specjalnego 0x20 = Super); osif() robi
`fn = pw[0]`. Zdemontowany kod SysInfo potwierdza to samo: kazde wywolanie to
`movew #fn,-(sp)` (argumenty wyzej), np. `pea -1; movew #$48,-(sp); trap #1;
addq #6,sp` = Malloc(-1).

Nasz asf_tos_trap1/13/14 bral funkcje z d0. Dla SysInfo d0 bylo 0 w miejscu
pierwszego Malloc(-1), wiec kernel widzial Pterm0 i konczyl program od razu -
stad `tos_exit_code=0` i brak wywolan, ktore gonilismy przez Krok 0.

Poprawka:
- system/tos_entry.S: trap1/trap13/trap14 czytaja `move.w (%usp),%d0` jako
  numer funkcji (zamiast d0) i przekazuja usp jak dawniej.
- system/tos_prg.c: tos_args() dla trybu uzytkownika zwraca usp+2 (argumenty za
  slowem funkcji).

### Wynik (Hatari, ten sam config)
- SysInfo dziala dalej: 22 wywolania GEM zamiast 21 i konczy sie na
  interaktywnym czekaniu na klawisz, a nie na wyjsciu.
- GEMDOS jest teraz poprawny: widoczne wywolania Mshrink(0x4A) i Malloc(0x48)
  z sensownymi wynikami.
- Ostatnie GEM: AES op=120 (biblioteka AES SysInfo, addrin) - jeszcze nie
  zaimplementowane u nas (zwraca 0).

### Nowy blocker: blokujace Cconin zakleszcza sie
`tos_getkey()` krecil sie w miejscu, a kolejka klawiszy jest wypelniana tylko
przez Timer C. Program TOS chodzi jednak z IPL 7 (`move.w #0x0700,%sr` w
asf_tos_enter), wiec Timer C nie moze wystartowac i kolejka nigdy sie nie
wypelnia - SysInfo wisi na pierwszym `Cconin`.

Poprawka czesciowa: `tos_getkey()` w petli wola teraz `tos_poll_input()`, ktore
ciagnie IKBD bezposrednio (`kernel_user_keyboard_event` sam robi
`input->poll(0u)`). Przy zamaskowanych przerwaniach jestesmy jedynym
konsumentem, wiec nie ma wyscigu z Timer C.

Efekt: PC w kernelu jest teraz w `kernel_find_service` (petla pollu IKBD),
czyli program naprawde czeka na klawisz i odpytuje sprzet. Wstrzykiwanie
klawisza przez `hatari-event keypress space` nie dotarlo jednak do kolejki
(liczniki GEM stoja na 22/3), wiec trzeba jeszcze ustalic, czy event Hatari
trafia do emulowanego IKBD, czy winna jest sciezka wejscia w kernelu.

### Nastepny krok
1. Potwierdzic, ze wstrzykniety klawisz dochodzi do IKBD (zrzut tos_rawlog /
   kernel_ikbd_raw_*), i doprowadzic Cconin do konca.
2. Zaimplementowac AES op=120 (i kolejne) oraz v_gtext, zeby SysInfo
   narysowalo interfejs w powierzchni okna kompozytora.
3. Sprawdzic multitasking, gdy SysInfo czeka na wejscie.

## Sesja 12 - SysInfo WIDOCZNE w oknie kompozytora i reaguje na klawiature

### Poprawka wejscia: zdekodowane ASCII vs surowe scancode
`kernel_tos_set_input(kernel_user_terminal_getchar)` daje juz ZDEKODOWANY
znak ASCII (kernel sam stosuje shift/caps i filtruje mysz), ale
`tos_next_scancode()`/`tos_key_ascii()` traktowaly go jak surowy scancode.
Kazdy klawisz (np. 0x61 = 'a') wypadal z mapy scancode'ow i byl porzucany -
dlatego Cconin nigdy nie wracal.

Poprawka w system/tos_prg.c: gdy `tos_key_ascii()` nie znajdzie kodu, a znak
jest drukowalny/kontrolny (>=0x20 albo CR/BS/ESC/TAB), przyjmujemy go
bezposrednio (scancode=0).

### Wynik (Hatari, ten sam config)
- Przed klawiszem: aes_log_n=22. Po wstrzyknieciu 'a','b','c': aes_log_n=24.
- **Okno SysInfo jest widoczne w kompozytorze** (zrzut: czarne okno aplikacji
  na pulpicie z belka statusu, w oknie widac echo klawisza i kursor, np. 'b_').
- SysInfo reaguje na klawisze (echo) - czyli petla wejscia pod personem TOS
  dziala i program nie wisi juz na slepo.
- Zaraz potem SysInfo wykonuje VDI 101 (v_clsvwk) i AES 19 (appl_exit), czyli
  konczy prace.

### Dlaczego konczy
Ostatnie nieudane wywolanie przed klawiszem to AES op=120
(c=[120,0,1,2,0,0], addrin[0], addrin[1], out=[0,0,0,0], res=0).
Nasz AES nie ma tego opcodu (default zwraca 0), wiec SysInfo uznaje, ze nie ma
zasobu/okna, pokazuje komunikat i po klawiszu wychodzi (v_clsvwk + appl_exit).
Kandydat: funkcja 'load/attach' z dwoma buforami addrin (material na nastepna
runde: zidentyfikowac opcode 120 i zaimplementowac go razem z wind_*/objc_*).

### Stan wzgledem kryterium
Kryterium 'SysInfo widoczne w oknie przy dzialajacym multitaskingu' jest
spelnione CZESCIOWO: okno SysInfo rysuje sie w kompozytorze na pulpicie, a
kernel obsluguje inne sesje (petla glowna + belka statusu dzialaja). Brakuje
pelnego UI, bo SysInfo przerywa po nieobsluzonym AES op=120.

### Nastepny krok
1. Ustalic, czym jest AES op=120 (tabela opcodow AES / demontaz binding'u
   SysInfo pod 0x04010152-0x040101C4) i zaimplementowac go.
2. Dopiac wind_create/wind_open/wind_get/wind_set, objc_* i v_gtext, zeby
   SysInfo narysowalo swoj interfejs zamiast czarnego okna.
3. Utrzymac petle zdarzen (evnt_multi) i sprawdzic multitasking w trakcie.

### Dopisane w trakcie sesji 12
- AES op=120 dostal tymczasowy stub sukcesu (intout[0]=1) - SysInfo nadal
  konczy po klawiszu, wiec to nie jedyny powod.
- Zaimplementowany v_gtext (rysuje glify font_glyph do powierzchni VDI) -
  okno nadal pokazuje tylko echo klawisza, bo SysInfo nie dochodzi do
  rysowania swojego UI przed wyjsciem.
- Zrzuty: przed klawiszem okno SysInfo widoczne w kompozytorze na pulpicie
  (belka statusu dziala), po klawiszu echo 'b_' i powrot do shella.
- Handoff: caly stan (KERNEL.SYS w work.img, ten dokument) jest trwaly;
  nowa sesja DSH moze kontynuowac od '### Nastepny krok' powyzej.
