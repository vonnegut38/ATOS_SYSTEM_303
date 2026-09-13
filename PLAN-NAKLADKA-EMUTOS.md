# Projekt: nakładka systemowa w stylu EmuTOS, wielozadaniowa, z aplikacjami TOS w oknach

Dokument projektowy. Rozwinięcie Etapów 8–10 z PLAN-BIOS-MEGA-ST.md.
Cel sprzętowy: Atari Mega ST + TF536 (68030 + 64 MB ALT-RAM + IDE) + ET4000/W32i na VOFA (MegaBus).

---

## 0. Wniosek w jednym akapicie

Nie piszemy systemu od zera — bazą jest BIOS + jądro ASF (procesy, VFS, blok, FAT16, moduły, timer, input, prosty GUI) oraz źródła EmuTOS w katalogu. Nakładka to cztery warstwy: **serwer GEM (VDI+AES)**, **menedżer okien**, **kompozytor ekranów** i **warstwa zgodności TOS**. Kluczem jest **MMU 68030**: każdy proces dostaje prywatne 32 KB pamięci ekranu Shiftera (zawsze 32 KB, niezależnie od trybu!), więc aplikacja TOS myśli, że ma pełny ekran, a kompozytor składa ten obraz do okna na ET4000. Kolory i mono są konwertowane planarnie→chunky, a W32i robi BitBlt/patfill/color‑expand dla okien, scrolla i VDI.

---

## 1. Cel i zakres

Funkcjonalnie:
1. Wygląd i zachowanie jak EmuTOS (pulpit, ikony, okna, menu, dialogi, fonty, GEM).
2. Wielozadaniowość wywłaszczeniowa z priorytetami.
3. Natywne aplikacje TOS (.PRG/.APP/.TOS) w oknach systemowych.
4. Tryby ST: mono 640×400, color low 320×200, color medium 640×200.
5. Akceleracja 2D W32i (BitBlt, wzorce, color‑expand, linie).
6. Przełączanie zadań, fokus, schowek, wspólny system plików (FAT16/32/AHDI).

Poza zakresem na start: pełna zgodność AES/VDI 100% (idziemy podzbiorem rosnącym), dźwięk DMA, sieć.

---

## 2. Punkt wyjścia (co już mamy i co reużywamy)

Jądro ASF (katalog system/):
- procesy i kontekst (kernel_entry.S, kernel_exception.c, kernel_proc),
- pamięć (kernel_memory.c) + PMMU niewykorzystany,
- VFS/partycje/FAT16 (kernel_vfs.c, kernel_partition.c, kernel_fat.c),
- warstwa blokowa z buforowanym DMA (kernel_block.c, kernel_module.c),
- timer 50 Hz, zdarzenia, input, shell (shell_module.S),
- grafika mono: kernel_graphics.c, kernel_surface.c, kernel_blitter.c,
- zalążek VDI EmuTOS: system/emutos_vdi_mono.c,
- prosty WM mono: mono_wm.c / desktop.

EmuTOS (work/emutos-reference/aes, /vdi, /bios): wzorzec zachowania i rysowania; RSC/ikony/fonty do adaptacji.

Brakuje: trybu 8 bpp karty, sterownika W32i, kompozytora okien, serwera VDI/AES, warstwy trapów TOS, MMU‑owej izolacji.

---

## 3. Architektura warstwowa

    Aplikacje TOS (.PRG/.APP)        Aplikacje natywne ASF
             |                                |
    Trap 1/2/13/14 + Line-A          syscall jądra
             |                                |
    +--------------------------------------------------+
    |  Warstwa zgodności TOS                            |
    |  - basepage, Pexec/Pterm, GEMDOS/BIOS/XBIOS       |
    |  - VBR per proces, wektory trapów                 |
    +--------------------------------------------------+
    |  Serwer GEM: VDI + AES                            |
    |  - VDI: opnvwk, primitywy, ROP, wzorce, fonty      |
    |  - AES: appl/wind/evnt/form/menu/objc/rsrc         |
    |  - wygląd z EmuTOS                                 |
    +--------------------------------------------------+
    |  Menedżer okien (WM)                              |
    |  - Z-order, clip, drag/resize, fokus, kursor       |
    +--------------------------------------------------+
    |  Kompozytor ekranów                               |
    |  - prywatne ekrany ST -> chunky 8bpp              |
    |  - mono/low/medium, palety, dirty rects           |
    +--------------------------------------------------+
    |  Sterownik W32i (2D)      +  Sterownik ET4000     |
    +--------------------------------------------------+
    |  Jądro ASF: procesy, MMU, VFS, blok, timer, input |
    +--------------------------------------------------+
    |  BIOS: boot, detekcja, ET4000, config             |
    +--------------------------------------------------+

---

## 4. Mechanizmy kluczowe

### 4.1 Wielozadaniowość i izolacja (MMU 68030)

- 68030 ma PMMU; używamy drzewa translacji jako korzenia przestrzeni procesu.
- Kontekst: D0-D7/A0-A6, PC, SR, USP + **CRP (root PMMU)** + **VBR**.
- Mapowanie procesu:
  - kod/dane/sterta/basepage pod adresami, których oczekuje TOS,
  - prywatny bufor ekranu (32 KB) pod adresem bazowym ekranu, który ustawia aplikacja,
  - reszta przestrzeni: identyczność (transparent), żeby działały rejestry sprzętowe (0xFFxxxx) i entry pointy VDI/AES.
- Przełączenie: zapis CRP/VBR i rejestry; powrót do procesu użytkownika przez RTE.
- Wywłaszczanie: timer 50 Hz + kwant czasu; priorytety; kolejka gotowych.
- Jądro działa w trybie supervisor w swojej przestrzeni (lub procesu z prawami jądra).

### 4.2 Wirtualny ekran procesu — „Shifter w oknie"

Kluczowa obserwacja: **każdy tryb ST ma dokładnie 32000 B pamięci ekranu**:
- mono 640×400, 1 bpp, 80 B/linia = 32000 B,
- low 320×200, 4 bpp planar, 160 B/linia = 32000 B,
- medium 640×200, 2 bpp planar, 160 B/linia = 32000 B.

Dlatego na proces wystarczy jedna ramka 32 KB (+ padding). Aplikacja pisze do „ekranu" jak na ST; my podmieniamy fizyczną ramkę:
- Physbase/Logbase/Setphysbase (XBIOS) → adres prywatnego bufora procesu,
- zmienna systemowa _v_bas_ad (0x044E) i p_video w basepage są spójne z procesem,
- Line-A i bezpośrednie zapisy do ekranu trafiają do prywatnego bufora.

Aplikacja nie wie, że nie jest jedyna. Kompozytor czyta bufor i renderuje okno.

### 4.3 Przechwytywanie trapów i VBR

- VBR (68010+) per proces: każdy proces ma tablicę wektorów, której wpisy 1, 2, 13, 14 i 0xA000-0xA00F wskazują na stuby jądra.
- GEMDOS (Trap #1): Fopen/Fread/Fwrite/Fclose/Fseek/Fcreate/Fdelete/Frename, Fsetdta/Fsfirst/Fsnext, Pexec/Pterm/Pterm0, Malloc/Mfree/Mshrink, Tgetdate/Tgettime, Dgetdrv/Fgetdta, itd.
- BIOS (Trap #13): Bconin/Bconout/Bconstat, Rwabs, Setexc, Bcostat, Mediach.
- XBIOS (Trap #14): Physbase/Logbase/Setphysbase, Setpalette, Vsync, Kbshift, Ikbd, Random, Setcolor, Esetshift.
- AES/VDI (Trap #2): numer funkcji w D0 (VDI opcode albo AES control/global/intin/intout).
- Line-A: wektor pod 0xA000+opcode*4; implementujemy najczęstsze (blit, wypełnienia, tekst) na prywatnym ekranie, docelowo akceleracja.

### 4.4 Serwer GEM: VDI + AES

- VDI: jedno „workstation" na okno (8 bpp chunky). Implementujemy podzbiór: v_opnvwk/v_clsvwk, v_clrwk, v_pline, v_pmarker, v_gtext, v_fillarea, v_bar, v_circle, v_rfbox, v_rbox, v_arc, v_pieslice, v_ellipse, v_blt/vro_cpyfm/vrt_cpyfm, v_setpalette, vs_color, atrybuty (vs_*), GDP.
- AES: prototyp z EmuTOS: appl_init/exit, graf_handle, wind_create/open/close/delete, evnt_*, form_*, menu_*, objc_*, rsrc_*, shel_*.
- Wygląd: przenosimy RSC, ikony i logikę rysowania z EmuTOS (GPL — zachować licencję/atrybucję).
- **Reentrancja:** stan GEM nie jest reentrantny. Rozwiązanie jak w MiNT: jeden proces‑serwer GEM posiada stan; aplikacje wysyłają żądania przez kolejkę i blokują się na czas obsługi. To daje prawdziwą współbieżność GEM.
- GEMDOS: reentrantny per plik, ale I/O dysku serializowane przez warstwę blokową (mutex).

### 4.5 Kompozytor i tryby ST

Powierzchnia okna: 8 bpp chunky (640×480 albo 800×600), paleta 256.
Konwersja per tryb:
- mono 1 bpp → chunky: 256‑bajtowa tablica na 8 pikseli (bajt źródłowy → 8 bajtów); alternatywnie W32i color‑expand (1 bpp + fg/bg).
- low 4 bpp planar → chunky: 4 bajty (po jednym z każdej z 4 płaszczyzn) → 4 indeksy; tablice 16‑bitowe dla par płaszczyzn (indeks = bity z 4 planów).
- medium 2 bpp planar → chunky: 2 płaszczyzny.
- Paleta: 16 kolorów ST mapowane na paletę okna (vs_color / Setpalette → aktualizacja).

Optymalizacja:
- dirty rectangles: regiony, które aplikacja zmieniła. Start: pełny recompose okna raz na klatkę (32 KB → chunky).
- Dalej: śledzenie „dirty" na poziomie wiersza (checksum), albo MMU write‑protect regionu ekranu → trap przy zapisie (jak MiNT/MagiC), albo timery aplikacji.
- Okno zminifikowane/nieaktywne: pomijane w kompozycji.

### 4.6 Akceleracja W32i

Sterownik w32i.c:
- unlock rejestrów rozszerzonych (port 0x3BF),
- BitBlt ekran‑ekran z ROP (SRCCOPY, PATCOPY, OR, AND, XOR...),
- pattern fill, linie, clip,
- color‑expand źródła 1 bpp (mono→kolor) — wprost pod nasz tryb mono,
- dla AX (bez blittera) fallback CPU.

Zastosowania:
- przesuwanie/odświeżanie okien (kopia prostokątów),
- scroll konsoli i list,
- VDI v_blt/vro_cpyfm/vrt_cpyfm,
- kursor sprzętowy (jeśli starczy VRAM: 1 KB).

Rejestry i semantyka: wg manuala Tseng ET4000/W32 oraz implementacji XFree86 xf86-video-tseng (BitBlt, screencopy, pattern, color expand). To źródło jest otwarte i potwierdza te tryby.

---

## 5. Model danych (szkic)

    typedef struct window {
        uint16_t id;
        rect_t   frame;              /* pozycja/rozmiar na pulpicie ET4000 */
        surface_t *surface;          /* 8bpp chunky powierzchnia okna */
        uint8_t  mode;               /* MONO / LOW / MEDIUM / CHUNKY */
        uint8_t  owner_pid;
        uint8_t  flags;              /* ACTIVE, OPEN, MINIMIZED, GEM */
        uint16_t *palette;           /* 16 kolorów ST (gdy tryb ST) */
        struct window *next;         /* Z-order */
    } window_t;

    typedef struct proc_screen {
        uint8_t  *buffer;            /* 32 KB prywatnej pamięci ekranu */
        physaddr_t phys;             /* ramka fizyczna */
        uint8_t  mode;               /* mono/low/medium */
        uint32_t base_expected;      /* adres, którego oczekuje aplikacja */
    } proc_screen_t;

    typedef struct vdi_wk {
        window_t *window;
        int16_t  clip[4];
        palette16_t pal;
        uint8_t  writemode, rop, pat;
    } vdi_wk_t;

---

## 6. Plan etapowy

### E1. Fundament graficzny ET4000 8 bpp + W32i — ✅ pierwszy krok (2026-09-11)
Zrobione: tryb ET4000 640x480x8 (chain-4, CRTC doubleword offset 0xA0), paleta 256 kolorów, powrót do tekstowego trybu karty po wyjściu; primitywy chunky (clear/fill/pixel/linia/rect/tekst 8x16/blit); demo pulpitu w stylu GEM (pasek menu + 2 okna + kursor + pasek statusu) widoczne na karcie. Wejście: menu BIOS **F6 GFX** (albo wybór F6 GFX + Enter).
Hatari: VOFA_GetMode obsługuje teraz tryb doubleword (offset licznony w jednostkach 32-bit), więc 8 bpp jest poprawnie dekodowany.
Do dokończenia w E1: sterownik w32i.c (BitBlt/patfill/color-expand) z fallbackiem CPU oraz emulacja blittera W32i w Hatari do testów.

#### Oryginalny zakres E1 (2–3 tyg.)
- Tryb 640×480×8 (lub 800×600×8), paleta 256, kontekst chunky.
- Gfx: piksel, linia, prostokąt, fill, blit, clip, przezroczystość.
- w32i.c: BitBlt/patfill/color‑expand; fallback CPU.
- DoD: kolorowy pulpit i przesuwane okno bez rozdzielania się na Shifterze.

### E2. WM + kompozytor (3–4 tyg.)
- Okna Z-ordered, clip, drag/resize, ramki, przyciski, fokus, kursor.
- Routing zdarzeń (klawiatura do focusu, mysz do okna pod kursorem).
- DoD: 2–3 okna, przesuwanie, minimalizacja, przełączanie.

### E3. Pulpit i wygląd EmuTOS (3 tyg.)
- RSC, ikony, fonty, menu, dialogi, pasek zadań.
- DoD: pulpit wygląda i zachowuje się jak EmuTOS.

### E4. Serwer VDI/AES (3–4 tyg.)
- VDI podzbiór dla typowych aplikacji, AES kontrolki/form/menu/evnt.
- DoD: natywna aplikacja ASF rysuje przez VDI do okna.

### E5. Zgodność TOS + MMU (4–5 tyg.)
- Trapsy 1/2/13/14, Line-A, basepage, Pexec/Pterm, VBR per proces.
- Prywatny ekran 32 KB + izolacja.
- DoD: mono aplikacja TOS (.PRG) działa w oknie.

### E6. Tryby kolorowe w oknach (3–4 tyg.)
- 320×200 4 bpp i 640×200 2 bpp → chunky; palety; scaling w oknie.
- DoD: kolorowa aplikacja TOS działa w oknie.

### E7. Akceleracja i ergonomia (ciągłe)
- dirty rects, W32i wszędzie, Alt+Tab, schowek, drag&drop plików.
- DoD: kilka aplikacji jednocześnie, płynne przełączanie.

---

## 7. Ryzyka i mitygacje

| Ryzyko | Wpływ | Mitygacja |
|---|---|---|
| Aplikacje bijące wprost w sprzęt (dźwięk, porty) | brak zgodności | MMU transparent + wirtualizacja XBIOS/Rezydent |
| Niereentrantny GEM | wyścigi | proces‑serwer GEM + kolejka (model MiNT) |
| Wydajność planar→chunky | wolno | tablice LUT, dirty rects, W32i color‑expand |
| Line-A / BLiTTER | złe rysowanie | emulacja Line-A + mapowanie na W32i |
| Różnice mono/color (paleta, 50/60 Hz) | zły obraz | Vsync wirtualny, paleta per okno |
| Licencja EmuTOS (GPL) | prawne | zachować GPL/atrybucję, nasz kod jądra osobno |
| Złożoność AES/VDI | długo | podzbiór rosnący, testy na realnych .APP |
| Aplikacje pełnoekranowe | brak okna | tryb „fullscreen exclusive” + powrót do okna |

---

## 8. Pierwszy krok (konkret)

1. Tryb ET4000 640×480×8 bpp + paleta (rozszerzyć hw/nova.c o kontekst chunky).
2. Gfx chunky (piksel/linia/fill/blit/clip) w jądrze.
3. w32i.c: BitBlt SRCCOPY + PATCOPY na 8 bpp, potem color‑expand dla 1 bpp.
4. Prosty kompozytor: tło pulpitu + jedno okno z tekstem i ramką, przesuwane strzałkami/myszą.
5. Test w Hatari: okno chunky, płynne przesuwanie (W32i albo fallback).

To daje wizualny efekt „GEM‑owego" okna na ET4000 i fundament pod resztę.

---

## 9. Mapowanie na istniejące moduły ASF

| Nowe | Bazuje na |
|---|---|
| hw/et4000.c (8bpp, paleta) | hw/nova.c, hw/config.c |
| system/w32i.c | nowy (XFree86/Manuel Tseng) |
| system/compositor.c | kernel_surface.c, kernel_blitter.c |
| system/wm.c | mono_wm.c, desktop |
| system/vdi_server.c | emutos_vdi_mono.c, emutos-reference/vdi |
| system/aes_server.c | emutos-reference/aes |
| system/tos_traps.c | kernel_entry.S, kernel_exception.c |
| system/tos_proc.c (basepage, Pexec) | kernel_proc, loader/tos.c |

Uwaga: loader/tos.c już umie wczytać/zwalidować obraz TOS — przyda się do Pexec i do startu EmuTOS z ALT-RAM (maprom).
