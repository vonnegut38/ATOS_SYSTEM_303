# Emulacja TOS-a w multitaskingu + system graficzny z EmuTOS (niezależny od rozdzielczości)

Dokument projektowy. Uzupełnia PLAN-NAKLADKA-EMUTOS.md o dwie rzeczy, o które pytasz:
(1) **jak emulować TOS i jego aplikacje w multitaskingu**, (2) **jak oprzeć grafikę na źródłach EmuTOS**.
Zaszyte w projekt: rozdzielczość pulpitu 640x480 / 800x600 / 1024x768 i 16–256 kolorów, zmieniana w locie.

---

## 0. Trzy decyzje, które wszystko porządkują

1. **Emulujemy API, nie sprzęt.** Aplikacja widzi basepage, wektory trapów, GEMDOS/BIOS/XBIOS/AES/VDI/Line-A i „swój" ekran. Nie widzi Shiftera, MFP, ACIA — chyba że świadomie po nie sięga (wtedy wirtualizujemy). Dzięki temu ta sama binarka .PRG działa bez zmian.
2. **Ekran aplikacji jest rozdzielony od rozdzielczości pulpitu.** Aplikacja zawsze dostaje swój tryb ST (320x200, 640x200, 640x400 = zawsze 32 KB). Pulpit może być 640x480/800x600/1024x768, a kompozytor skaluje okno. Zmiana rozdzielczości **nie dotyka aplikacji**.
3. **Serwer GEM jest jednym zadaniem.** AES/VDI nie są reentrantne, więc jeden task-serwer posiada stan, a aplikacje wywołują go przez kolejkę i blokują się. To daje prawdziwą współbieżność.

---

## 1. Warstwy i odpowiedzialności

    .APP/.PRG (GEM)      .PRG (console)       gra/hardware
          |                    |                   |
          | Trap #2 (AES/VDI)  | Trap #1/13/14     | bezpośrednio
          v                    v                   v
    +-------------------------------------------------------------+
    | TOS personality: VBR, basepage, Pexec/Pterm, GEMDOS/BIOS/     |
    | XBIOS, Line-A, emulacja trapów                               |
    +-------------------------------------------------------------+
    | Serwer GEM (AES + VDI)  — stan, kolejki, okna, form/menu      |
    +-------------------------------------------------------------+
    | Kompozytor + WM         — Z-order, clip, mapowanie ST->chunky |
    +-------------------------------------------------------------+
    | screen_t (tryb pulpitu) + W32i (2D) + ET4000                 |
    +-------------------------------------------------------------+
    | Jądro ASF: procesy, MMU, VFS, blok, timer, input             |
    +-------------------------------------------------------------+

---

## 2. Trzy poziomy zgodności aplikacji

| Typ aplikacji | Co robi | Jak jest prezentowana | Trudność |
|---|---|---|---|
| **GEM (.APP)** | rysuje przez VDI, zdarzenia przez AES | okno systemowe; VDI pisze do powierzchni okna | łatwe |
| **Console / bezpośredni ekran** | pisze do pamięci ekranu, GEMDOS | okno = jego prywatny ekran 32 KB (konwersja ST->chunky) | średnie |
| **Gra / hardware** | bije w rejestry, pełny ekran | tryb „fullscreen exclusive"; MMU transparentne; powrót do okna | trudne |

Kolejność wdrażania: GEM → console → gry. Większość użytkowych programów TOS to pierwsze dwie.

---

## 3. Fundament okien: MMU + prywatny ekran

- Każdy tryb ST to **32000 B** pamięci ekranu (mono 80 B x 400, low/medium 160 B x 200).
- Proces dostaje prywatną ramkę 32 KB, mapowaną pod adres, którego oczekuje aplikacja.
- Spójne per proces: XBIOS Physbase/Logbase/Setphysbase, zmienna _v_bas_ad (0x044E), p_video w basepage.
- Line-A i zapisy wprost do ekranu trafiają do prywatnej ramki (MMU), więc aplikacja „myśli", że ma cały ekran.
- Kompozytor czyta ramkę, konwertuje tryb ST -> powierzchnia pulpitu i wklejona do okna (z clipem i skalą).

Adresacja: 68030 PMMU, drzewo translacji na proces; na przełączeniu zapis CRP + VBR.
Reszta przestrzeni: identyczność (rejestry 0xFFxxxx, entry pointy AES/VDI).

---

## 4. Kontrakt z aplikacją TOS

- **VBR per proces** (68010+): wektory 1 (GEMDOS), 2 (AES/VDI), 13 (BIOS), 14 (XBIOS),
  0xA000–0xA00F (Line-A) wskazują na stuby jądra.
- **Basepage**: p_lowtpa, p_hitpa, p_tbase/tlen, p_dbase/dlen, p_bbase/blen, p_dta,
  p_parent, p_env, p_exec, p_cmdlin.
- **Pexec**: nagłówek .PRG (0x601A): tlen/dlen/blen/slen, relokacje, załadowanie, basepage, skok.
- **Pterm/Pterm0/Ptermres**: zwolnienie pamięci, zamknięcie okna/pliku, powrót do powłoki.
- **GEMDOS (#1)**: Fopen/Fread/Fwrite/Fclose/Fseek, Fsetdta/Fsfirst/Fsnext, Pexec/Pterm, Malloc/Mfree/Mshrink, Tgetdate/Tgettime.
- **BIOS (#13)**: Bconin/Bconout/Bconstat, Rwabs, Setexc, Mediach.
- **XBIOS (#14)**: Physbase/Logbase/Setphysbase, Setpalette/Setcolor, Vsync, Kbshift, Ikbd, Random.
- **Line-A (0xA000+)**: najczęstsze operacje (blit, wypełnienie, tekst) na prywatnej ramce, potem W32i.

Reentrancja: GEMDOS per-plik, a I/O dysku serializowane w warstwie blokowej (mutex). Jeden kanał ACSI/IDE.

---

## 5. Serwer GEM — model komunikatów (to jest sedno multitaskingu)

AES/VDI trzymają globalny stan (aktualne okno, atrybuty, schowek, kolejka zdarzeń), więc nie mogą
być wołane równolegle. Rozwiązanie z MiNT/XaAES, zaadaptowane do ASF:

1. Aplikacja woła trap #2 (albo AES) -> stub jądra pakuje contrl/intin/ptsin/intout/ptsout.
2. Stub wysyła żądanie do **kolejki serwera GEM** i blokuje TYLKO ten proces.
3. Serwer wykonuje funkcję (np. wind_create, evnt_multi, v_gtext) na powierzchni okna klienta.
4. Serwer odsyła wynik i wznawia proces.

Skutek: gdy aplikacja A siedzi w evnt_multi, aplikacja B liczy, a serwer obsługuje innych.
To odróżnia prawdziwy multitasking GEM od „jedno okno na raz".

Warianty:
- evnt_multi/evnt_button itd. — serwer blokuje klienta do zajścia zdarzenia; inne zadania żyją.
- form_do/menu — pętla lokalna AES, ale nadal przez serwer (spójne).

---

## 6. Grafika z EmuTOS: co bierzemy, co wymieniamy

Źródła są w work/emutos-reference (licencja GPL — nasz kod musi to respektować).

### 6.1 VDI (bierzemy prawie cały)
- **Dispatch**: vdi/vdi_main.c — tablice jmptb1 (opcode 1..) i jmptb2 (100..), sygnatury (nintin/nptsout).
- **Workstation**: vdi/vdi_defs.h — typ Vwk (virtual workstation) trzyma atrybuty (kolor, wzorzec, font, clip).
- **Kontrola**: vdi/vdi_control.c (vdi_v_opnvwk, vdi_v_clswk, vdi_vq_extnd, vs_color, clip...).
- **Prymitywy**: vdi_line.c, vdi_fill.c, vdi_text.c, vdi_marker.c, vdi_raster.c, vdi_bezier.c.
- **Blit**: vdi_blit.S, vdi_tblit.S, vdi_tblit_cf.S (table blit = rdzeń kopiowania prostokątów).

### 6.2 To, co wymieniamy: warstwa urządzenia
EmuTOS VDI rysuje do ekranu ST (planarnego). My podstawiamy **screen_t** — powierzchnię chunky 8 bpp
(bądź 4 bpp) w trybie pulpitu:
- vdi_blit / vra_cpyfm / vrt_cpyfm -> blit na screen_t (najlepiej przez W32i),
- vs_color / paleta -> wpis do palety trybu (16 lub 256),
- clip -> do prostokąta okna,
- konwersja źródeł MFDB (planarne bitmapy aplikacji) -> chunky.

Zysk: VDI „myśli", że rysuje do ekranu, a naprawdę komponuje okno — bez zmian w aplikacji.

### 6.3 AES (bierzemy warstwę okien/zdarzeń/zasobów)
- **Okna**: aes/gemwmlib.c (wind_create/open/close/set/get, draw window).
- **Zdarzenia**: aes/gemevlib.c (evnt_multi, timery, mysz, klawiatura).
- **Formularze i menu**: aes/gemfmlib.c, gemfslib.c, gemmnlib.c.
- **Zasoby**: aes/gemrslib.c + gem.rsc (ikony, dialogi, fonty) — wygląd „jak EmuTOS".
- **Grafika AES**: aes/gemgraf.c, gemgrlib.c (desktop, ramki, cienie).
- **Pulpit**: desk/ (górne menu, ikony dysków, okna).

### 6.4 Interfejsy, których dotykamy
- gemgrlib.c (grafika pulpitu), gemgsxif.c (warstwa VDI AES-a), gemctrl.c (control array).

---

## 7. Rozdzielczości i zmiana w locie

### 7.1 Tabela trybów (display_mode_t)
    typedef struct {
        uint16_t w, h;
        uint8_t  bpp;          /* 4 = 16 kolorow, 8 = 256 */
        uint16_t stride;       /* bajtow na linie */
        uint8_t  seq[5];       /* 3C4/3C5 0..4 */
        uint8_t  crtc[0x19];   /* 3D4/3D5 */
        uint8_t  gc[9];        /* 3CE/3CF */
        uint8_t  misc;         /* 3C2 */
        uint32_t vram_used;
    } display_mode_t;

Tryby docelowe (VRAM 1 MB na karcie):
| tryb | 4 bpp | 8 bpp |
|---|---|---|
| 640x480 | 150 KB | 300 KB |
| 800x600 | 234 KB | 469 KB |
| 1024x768 | 384 KB | 768 KB |
8 bpp przy 1024x768 mieści się w 1 MB, ale zostaje mało na offscreen/kursor — planujemy osobny
limit i ewentualnie 4 bpp jako domyślny dla 1024x768.

### 7.2 Zmiana trybu — sekwencja
1. Zablokuj kompozycję (flaga w WM).
2. Reprogramuj ET4000 z display_mode_t (sequencer, CRTC inc. doubleword, GC, DAC, misc).
3. Ustaw screen_t: nowa szerokość/wysokość/stride/format; przelicz layout pulpitu i pozycje okien.
4. Redraw pulpitu i okien z ich powierzchni (skala zmieniona).
5. Odblokuj; aplikacje dostają nową geometrię przez vq_extnd / wind_get (WF_WORKXYWH).
6. Aplikacje ST pozostają w swoich trybach — nic nie trzeba przeładowywać.

### 7.3 Projekcja trybu ST na pulpit
- mono 640x400 -> okno 640x400 (1:1) lub skalowane 2x na dużym pulpicie,
- low 320x200 -> 1:1/2x/3x; medium 640x200 -> 1:1/2x,
- skalowanie całkowite (najostrzejsze) jako domyślne; opcjonalnie wygładzanie.
- reguła: okno ma zawsze tytuł/ramkę w rozdzielczości pulpitu, treść skalowana.

### 7.4 Gdzie trafia „16 czy 256 kolorów"
- 4 bpp: paleta 16 kolorów; 8 bpp: 256. VDI zna v_planes i sam się przestawia (TRUECOLOR_MODE w EmuTOS).
- Aplikacja ST ma własną paletę 16 kolorów (ST); kompozytor mapuje ją do palety trybu pulpitu.
- Przy zmianie 8->4 bpp robimy kwantyzację (najbliższy kolor) dla powierzchni systemowych.

---

## 8. Kluczowe struktury

    typedef struct screen {          /* powierzchnia pulpitu */
        display_mode_t *mode;
        uint8_t *pixels;             /* VRAM albo offscreen */
        uint16_t stride;
        uint32_t palette[256];
    } screen_t;

    typedef struct vdi_wk {          /* odpowiednik Vwk z EmuTOS */
        window_t *win;
        int16_t  clip[4];
        uint8_t  planes;             /* 4 albo 8 */
        uint16_t color, fill, style; /* atrybuty */
        uint8_t  pat[8], font[8];    /* wzorce/font VDI */
    } vdi_wk_t;

    typedef struct app_ctx {         /* kontekst aplikacji TOS */
        basepage_t bp;
        uint8_t   *screen32k;        /* prywatna ramka 32 KB */
        physaddr_t screen_phys;
        uint8_t    st_mode;          /* mono/low/medium */
        window_t  *win;
        msgq_t     gem_reply;
    } app_ctx_t;

---

## 9. Kolejność prac (zaktualizowane etapy)

- **T1. Szkielet persony TOS — ✅ ZROBIONE (2026-09-11)**
  Loader .PRG (0x601A) z dysku (nagłówek + relokacje), basepage, uruchomienie w trybie supervisor
  z ochroną rejestrów callee-saved, tymczasowa podmiana wektorów 33/34/45/46, **konwencja TOS:
  argumenty na stosie** (ramka 68010+ = 8 B, pierwszy arg pod frame+8) i **numer funkcji z dolnego
  słowa D0**. API: GEMDOS (Cconin/Cconout/Cconws/Cconis/Crawio/Cnecin/Dgetdrv/Mshrink/Pterm/Pterm0),
  BIOS (Bconstat/Bconin/Bconout/Bcostat/Kbshift), XBIOS (Getrez/Physbase/Logbase/Setscreen/
  Setpalette/Setcolor/Vsync). Program dostaje własny bufor ekranu 32 KB. Wejście z linii komend:
  **run <plik.prg>** (shell wykrywa 0x601A). Test: "TOS .PRG: Cconws OK / stack args OK /
  XBIOS/BIOS OK / TOS PROGRAM EXIT: 0".
  **Line-A** (wektor 10): ramka Line-A ma PC pod offsetem 2, opcode w (PC), blok parametrów w A0;
  zaimplementowane $1 put pixel, $2 get pixel, $3 line, $4 hline, $5 filled rect (prosto na buforze
  ekranu programu, tryby mono/medium/low). Test: "LINE-A: PUT/GET PIXEL OK" + "RECT DRAWN".
  **GEMDOS file I/O** (2026-09-11): uchwyty plików (Fopen/Fread/Fclose/Fseek, max 8), sterta
  (Malloc/Mfree, 64 KB bump), DTA (Fgetdta/Fsetdta), wyszukiwanie katalogu (Fsfirst/Fsnext, wildcard).
  Test: FILEIO.PRG czyta HELLO.TXT przez Fopen/Fread → "FILE READ: HELLO FROM GEMDOS FREAD!" + "MALLOC OK".
  **Dopracowanie (2026-09-11):** (1) **odporność na błędy** — na czas programu instalowane są
  wektory 2/3/4/5/6/7/8/11, wyjątek kończy program i wraca do shella z ujemnym kodem (test:
  FAULT.PRG z ILLEGAL → "TOS PROGRAM FAULT", kernel żyje); (2) **klawiatura** — Cconin/Cnecin/
  Crawio/Crawcin/Cconis oraz BIOS Bconin/Bconstat czytają IKBD (scancode→ASCII, US, bez shiftu;
  test: KEY.PRG echo klawisza); (3) **linia poleceń** w p_cmdlin (0x80) = nazwa pliku.
  **Pexec + dopracowanie (2026-09-11):** GEMDOS **Pexec** (0x4B, tryb 3 = load&go) — rekurencyjne
  uruchomienie innego .PRG; stan rodzica (ekran/sterta/uchwyty/DTA) zapisywany i odtwarzany, a
  **stos jądra dla enter/exit jest łańcuchowany** (poprzedni SP na stosie), więc zagnieżdżone runy
  wracają poprawnie. Test: PARENT.PRG → CHILD.PRG (kod 7) → "child exit code = 7" → EXIT 0.
  Naprawiony też kod błędu (stub wyjątku pushuje wektor na stos; teraz EXIT = -4 dla ILLEGAL).
  Klawiatura: shift/caps (litery + symbole shiftowane).
  **Zapis plików (2026-09-11):** pełny zapis FAT16 w jądrze — alokacja klastrów, wpisy FAT w OBU
  kopiach, aktualizacja wpisu katalogowego i rozmiaru, kasowanie (zwalnianie łańcucha). Warstwa
  blokowa: `kblock_write`, a handoff dostał callback `acsi_write_sector` (wersja 4 — wymaga
  wgrania nowego BOOT.SYS). GEMDOS: `Fcreate` (0x3C, z truncate), `Fwrite` (0x40), `Fdelete` (0x41).
  Test: WRITE.PRG tworzy OUT.TXT, pisze "WRITTEN BY TOS GEMDOS!", zamyka i czyta z powrotem →
  "READBACK: WRITTEN BY TOS GEMDOS!"; plik potwierdzony na obrazie (klaster 130, 26 B).
  **Cache dyskowy (2026-09-11):** warstwa blokowa przepisana z okna 1-sektorowego na
  **write-back cache** w stylu SMARTDRV: 2000 sektorów (~1 MB) w TT-RAM, adresowanie bezpośrednie
  (sector % SLOTS), **read-ahead 32 sektory** ze stagingiem DMA w ST-RAM, odroczony zapis
  (dirty) z writebackiem przy ewykcji oraz jawny `kblock_flush()` (wołany po zakończeniu
  programu TOS). Statystyki: `kblock_cache_hits/misses/flushes/slots`; test pokazuje
  ~92% trafień (hits=102/misses=9). **Przełączony na write-through** (2026-09-11): każdy zapis
  idzie od razu na dysk (cache aktualizowany, więc odczyt dalej szybki) — writebacks=0.
  **DISKPD.PRG + SMARTDRV.PRG (2026-09-11):** cache domyślnie **OFF** (pass-through w warstwie
  blokowej); `SMARTDRV.PRG` włącza go przez XBIOS **0x00FE** (0=off,1=on,2=ticks,3=hits,4=misses,
  5=benchmark). `DISKPD.PRG` to krótki benchmark (~4–5 s): 64 KB zapis/odczyt + 12 losowych
  odczytów, wynik w ticks/ms/KB/s. Pomiar przed/po (Hatari, IDE):
  **OFF**: write 45 KB/s, read 33 KB/s, seek 80 ms; **ON**: write 133 KB/s, read 228 KB/s,
  seek 10 ms (hits=513/misses=4). `SMARTDRV.PRG` przyjmuje parametr: bez argumentu = **-on**,
  `-off` = wyłączenie i **zwolnienie pamięci cache** (XBIOS 0xFE cmd 6 → `kblock_cache_release`);
  argumenty trafiają do `p_cmdlin` (path + args). Po `-off` kolejne `-on` realokuje cache.
  Po drodze naprawione: przepełnienie licznika w `alloc_cluster`, zbędny odczyt przy zapisie
  pełnego sektora, obsługa braku cache w ścieżce pass-through.
  **Ctrl+Alt+Del = ciepły reset:** wykrywany w BIOS-ie (POST/odliczanie/menu) oraz w jądrze
  (IKBD IRQ, skan 0x1D+0x38+0x53); przed resetem wołane `kblock_flush()`, potem `RESET`
  (BIOS) i skok do wejścia ROM 0x00E00030 → restart POST.
  Do dokończenia: ekran color/medium (wyświetlenie), $7 blit / $E raster / $F fill, Alt/kursor,
  rename/katalogi, wiele .PRG naraz (T2).
- **T2. MMU + wirtualny ekran**: prywatna ramka 32 KB, Physbase/Setphysbase per proces,
  kompozytor wklejający ją do okna. Test: aplikacja console w oknie.
  **Stan (2026-09-11) — ETAP T2.1 UKOŃCZONY:** prywatny ekran 32 KB per program ✓; ścieżka
  prezentacji wideo (handoff v5: `video_enter/leave/clear/fill/text/blit_mono` → BIOS `video8_*`, ET4000 8bpp) ✓; kompozytor w jądarze (`tos_open_window`/`tos_composite_tick`/`tos_close_window`)
  rysuje okno (belka+ramka) i **wkleja prywatny ekran do klienta** — potwierdzone zrzutem
  (program rysuje biały ekran + czarny prostokąt, widać go w oknie).
  Wydajność: `video8_blit_mono` z LUT 256×8 i zapisami 32‑bitowymi; kompozycja w Timer C
  ograniczona do ~10 Hz i tylko przy zmianie (rolling checksum 16000 słów). Tryb okna jest
  opt‑in: XBIOS `0x00FE` cmd 7 = ON, cmd 8 = OFF (`WINON.PRG`).
  Do dokończenia T2: wiele okien/procesów naraz, przesuwanie okna, ekrany color/medium.
- **T3. Serwer GEM + VDI z EmuTOS**: przenieść vdi_main/vdi_control/prymitywy, podłączyć screen_t.
  Test: aplikacja GEM rysuje w oknie.
- **T4. AES z EmuTOS**: okna, zdarzenia, formularze, menu, rsc. Test: okno z menu i dialogiem.
- **T5. Tryby pulpitu**: tabela + zmiana 640x480/800x600/1024x768, 4/8 bpp, skalowanie okien.
  Test: przełączenie w locie przy działających aplikacjach.
- **T6. Akceleracja W32i**: blit/patfill/color-expand w VDI i kompozytorze.
- **T7. Gry/hardware**: MMU transparent, tryb fullscreen, powrót do okna.

To kładzie T1–T2 przed „dużą grafiką", zgodnie z Twoją intuicją: najpierw emulacja TOS-a,
potem system graficzny.

---

## 10. Ryzyka

| Ryzyko | Mitygacja |
|---|---|
| Aplikacje bijące w sprzęt (dźwięk, rejestry) | MMU transparent + wirtualizacja XBIOS; tryb fullscreen |
| Niereentrantny AES/VDI | jeden serwer + kolejki (model MiNT) |
| Różnorodność .PRG (relokacje, A-line, A5-world) | testy na realnych programach, kolejne poziomy zgodności |
| VRAM 1 MB przy 1024x768x8 | 4 bpp domyślne, offscreen w ALT-RAM (nie na karcie) |
| Wydajność kompozycji 1024x768 | dirty rects, W32i, konwersja LUT |
| GPL EmuTOS | zachować licencję/atrybucję; kod jądra ASF osobno |

---

## 11. Pierwszy krok (konkret pod T1–T2)

1. Stub trapy #1/#2/#13/#14 + VBR per proces (na razie jeden proces testowy).
2. Prywatna ramka 32 KB + podmiana Physbase/Setphysbase; kompozytor pokazuje ją w oknie 1:1.
3. Prosty .PRG (napisany ręcznie, nagłówek 0x601A) rysujący do ekranu — uruchomiony przez Pexec.
4. Dopiero potem VDI z EmuTOS i tryby 800x600/1024x768.

Efekt pośredni: pierwsza aplikacja TOS w oknie, jeszcze bez GEM-u — ale z prawdziwym ABI.
