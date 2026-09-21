# Plan M1-M5 — wydajny pulpit na 68030 (i dlaczego może być szybszy niż EasyMiNT)

Założenie: 68030 @32 MHz + TF536, docelowo ET4000 chunky 8bpp; shifter planar jako fallback.
Metoda: każdy krok = jedna zmiana + test w Hatari + POMIAR (ticks() 50 Hz, licznik klatek,
piksele/s). Bez pomiaru krok nie jest skończony.

## 0. Dlaczego możemy być szybsi od EasyMiNT (konkretne)

1. BRAK MMU i ochrony pamięci. EasyMiNT ma MMU, translacje i ochrone stron — każdy trap
   i każde przełączenie kontekstu kosztuje. U nas adresy fizyczne, trap to zwykłe przejście.
   To największa oszczędność przy pracy interaktywnej.
2. WŁASNY SURFACE OKNA. W XaAES VDI przycina każde wywołanie do ekranu (warstwy, clip).
   U nas aplikacja rysuje bez sprawdzania zasłonięć; clipping robi dopiero kompozytor.
3. DAMAGE ZAMIAST PEŁNEGO REDRAW. Składamy tylko sumę prostokątów (merge + intersect).
4. KURSOR JAKO OVERLAY. Zero damage pod kursorem (XaAES odrysowuje tło).
5. TEKST PRZEZ LUT + BLIT WIERSZY. VDI glif po glifie; u nas font jako bitmapa + LUT,
   a konsola renderuje TYLKO zmienione komórki.
6. ET4000 CHUNKY (liniowy). Kopie 32-bit po wierszach = prędkość pamięci.
7. RUCH OKNA = BLIT SURFACE'U — kopiujemy gotowe piksele, nie odrysowujemy treści.
8. IPC BEZ KOPII — mailbox wprost, batchowanie zdarzeń.
9. PRIORYTET + SEN NA MAILBOXIE — bezczynna aplikacja nie zjada CPU w ogóle.

## M1 — Priorytety i kwant (tło nie zjada CPU)

Pliki: system/kernel_exception.c (PCB + scheduler), include/kernel_system.h

1. user_process_t: dodać uint8_t priority; uint8_t quantum_left;
2. Scheduler z czystego RR na WAŻONY RR:
   - priority 0..3, przydział kwantu = base << priority (np. 4/8/16/32 ticki),
   - wybór: pierwszy gotowy o najwyższym priorytecie, w obrębie priorytetu RR,
   - koszt O(6 slotów) — bez list. Wiekowanie (aging) co N ticków, żeby nie głodzić.
3. Nowe wywołanie: set_priority(pid, p). Pulpit: okno aktywne=3, tło=1, usługi=3.
4. Bezczynność: brak gotowych zadań -> stop/pętla konsoli, zero spalania CPU.

Test: tło liczy w pętli, druga w fokusie. Kryterium: tło <= ~25% CPU, fokus bez zwolnień.

## M2 — Kompozytor z listą przycięcia (serce wydajności)

Pliki: nowy system/wm_core.c + include/wm_core.h; używa ksurface, kblit, kgfx_dirty, kvideo_*.

Struktury:
  typedef struct {int16_t x,y; uint16_t w,h;} wm_rect_t;
  typedef struct { wm_rect_t frame, client; ksurface_t *surface; uint32_t pid;
                   uint8_t z, flags; } wm_window_t;

- Lista okien: wm_window_t win[WM_MAX] w kolejności Z (indeks = Z).
- Lista przycięcia: dla każdego okna wm_rect_t clip[WM_CLIP_MAX] (typ. 4-8 prostokątów)
  = klient minus okna wyżej minus ekran minus ramka. Aktualizowana TYLKO przy zmianie
  Z/geometrii, nie przy każdym rysowaniu.
- Damage: wm_rect_t damage[WM_DAMAGE_MAX], scalane.

Algorytm klatki:
  1. scal damage (łączenie nachodzących prostokątów)      // O(n^2), n<=8
  2. dla każdego prostokąta damage R:
       a. tło w R                                        // fill 32-bit / kblit
       b. okna od Z=0 do top: Rc = R ∩ clip[okno];
          jeśli Rc: blit surface[okno] -> framebuffer w Rc
  3. wyczyść damage

- Blity: planar -> kblit (SRC/HOP); chunky -> pętla 32-bit, line_bytes/4 słów na wiersz.
- Klatka tylko na vsync (50 Hz) i tylko gdy damage niepuste — inaczej 0 pracy.
- Double buffer: rysujemy do desktop_backbuffer, flip na końcu klatki.

Test: dwa okna, przesuwanie, zasłanianie, częściowe odsłony. Kryterium: brak artefaktów,
liczba blitów = suma pól prostokątów damage, zero pełnych redrawów.

## M3 — Shell jako OKNO (najkrótsza droga do efektu Win 3.11)

Pliki: system/kernel_console.c (siatka), nowy system/wm_console.c, kernel_sessions.c.

1. Siatka term_cell_t cells[80*25] per sesja (jest) + lista zmienionych komórek (jest:
   term_display z damage).
2. wm_console.c: okno 640x416 (80x25 znaków 8x16 + ramka) z surface'em; render TYLKO
   zmienionych komórek: dla każdej zmienionej (x,y) blit 8x16 glifu (LUT + 16 słów
   32-bit) do surface[okno] w wierszu y*16. Jeden znak ~64 B zapisu.
3. Wejście: sesje już routują klawisze per kontekst; fokus okna konsoli = dana sesja.
4. Scroll: memmove surface'u o 16 wierszy (jeden ruch 30 KB) + dorysowanie nowego wiersza.

Test: pisanie i scroll w oknie na 030. Kryterium: pisanie bez opóźnień, scroll = 1 kopie +
1 wiersz; licznik pikseli vs pełny redraw (oczekiwane dziesiątki razy mniej pracy).

## M4 — VDI: workstation per okno + primitywy na surface

Pliki: nowy system/vdi_service.c (+ emutos_vdi_mono.c jest), kernel_surface.c.

1. Workstation: struct vwk { ksurface_t *surface; wm_rect_t clip[WM_CLIP_MAX];
   uint16_t fg,bg,mode,line_w; uint8_t font; } — tworzony przy wind_open.
2. Primitywy na surface, bez sprawdzania zasłonięć:
   - v_bar/v_fillarea: fill 32-bit (chunky) lub kblit wzorca (planar),
   - v_gtext: font + LUT + blit wierszy,
   - v_pline/v_rbox/v_arc: Bresenham/rysowanie do surface + clip z workstation,
   - vrt_cpyfm/vro_cpyfm: blit z ROP-ami.
3. Clipping: tylko do clip[] workstation (kilka prostokątów) — nie do okien.
4. Tryby: jedna warstwa pixel-writer + trzy backendy: mono planar, 2/4bpp planar,
   8bpp chunky. Paleta: v_setpalette.
5. Trapy: persona TOS (#2) mapuje v_* na te funkcje — aplikacje GEM bez zmian.

Test: aplikacja GEM rysująca w oknie. Kryterium: liczba operacji na klatkę, czas pełnego
odrysowania okna 300x200 (ticki).

## M5 — AES: okna, zdarzenia, RUCH Z ZAWARTOŚCIĄ

Pliki: nowy system/aes_service.c, rozszerzenie wm_core.c.

1. wind_create/open/close/delete/get/set/calc/update na wm_window_t. Ramka TOS: tytuł,
   mover, closer, info, rozmiar (gadżety jako prostokąty w ramce).
2. Zdarzenia: evnt_multi = SEN NA MAILBOXIE aplikacji (IPC jest). AES budzi aplikację
   tylko zdarzeniami jej dotyczącymi. Bezczynna aplikacja = 0% CPU.
3. Komunikaty: 8 longwordów GEM; WM_REDRAW/WM_CLOSED/WM_MOVED/WM_ONTOP/WM_TOPPED.
   Redraw przekazujemy jako prostokąty damage, nie całe okno.
4. Ruch z zawartością (klucz wydajności):
     on mover_press: drag=1; last=frame;
     pętla na vsync gdy drag:
        new = frame przesunięty o deltę myszy (klamrowana do ekranu)
        jeśli new != last:
            dodaj_damage(last ∪ new)      // suma stary+nowy
            zaktualizuj frame i clip[]     // tylko przesunięcie
            last = new                     // kompozytor robi resztę — aplikacja
                                           // NIE dostaje zdarzeń rysowania
     on mover_release: drag=0; wyślij WM_MOVED(new) — jeden komunikat
   Koszt klatki ≈ 2 × pole okna w blitach; na chunky to praktycznie memcpy.
5. WM_REDRAW z prostokątami: aplikacja przerysowuje tylko odsłonięte fragmenty (clip VDI).

Test: przesuwanie okna 300x200 z zawartością + druga aplikacja w tle. Kryterium:
klatki mierzone w tickach (cel >=25 Hz na 030+ET4000), brak smug, tło żyje.

## Zbiorcza tabela ścieżek szybkich

  wypełnienie tła : fill 32-bit (chunky) / kblit wzorca (planar)  | fallback: pętla słów
  blit okna       : wiersze 32-bit, line_bytes/4 słów             | fallback: kblit SRC
  tekst 8x16      : font + LUT + 16 słów na znak                  | fallback: glif po glifie
  scroll konsoli  : memmove surface + 1 wiersz                    | fallback: pełny redraw
  kursor          : overlay, 0 damage                             | fallback: odrys tła
  ramka okna      : 4 linie + 4 narożniki                         | fallback: per-piksel
  zdarzenia       : mailbox, batch                               | fallback: polling

## Kryteria pomiarowe (żeby szybsze niż EasyMiNT było sprawdzalne)

1. Przesuw okna 300x200 z zawartością — klatki/s.
2. Pisanie w konsoli — czas od klawisza do znaku (ticki).
3. Otwarcie aplikacji GEM — od Pexec do pierwszego okna (ticki).
4. Scroll listy plików (Commander) — czas na stronę (ticki); wzorzec z v9 już mierzony.
5. Obciążenie tła — udział CPU aplikacji w tle przy aktywnej drugiej.

Wzorzec porównawczy: te same operacje na tym samym obrazie/Hatari z EasyMiNT (MiNT+XaAES),
jeśli da się je uruchomić w tej samej konfiguracji (STE/030, ET4000). Inaczej podajemy liczby
bezwzględne i mówimy wprost, że NIE mamy pomiaru porównawczego (tak zrobił Codex z 3 tickami).

## Ryzyka per krok

M1: głodzenie przy złych priorytetach -> limit kwantu + aging co N ticków.
M2: błędy clip list -> testy jednostkowe na prostokątach (1600 kombinacji jak w v9 dla
    syscallu 52) + wizualny test zasłaniania.
M3: rozjazd siatki i surface'u -> jedno źródło prawdy (siatka), surface tylko pochodny.
M4: ROP-y i clipping -> porównanie z wzorcem EmuTOS dla kilku prymitywów.
M5: ruch okna i komunikaty -> ŻADNYCH zmian bez testu w Hatari (tu dziś zamroziliśmy
    system przy per-sesyjnym buforze).
