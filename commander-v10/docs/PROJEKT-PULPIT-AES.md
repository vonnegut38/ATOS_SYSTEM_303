# Projekt pulpitu (AES + VDI) dla naszego systemu ASF

Dokument projektowy. Punkt wyjścia: kernel ASF (procesy wywłaszczające, IPC/mailbox, kmem,
sesje tts, grafika kgfx z surface'ami, dirty rectangles, blitter, kursor jako overlay,
usługi SYSTEM.GFX / SYSTEM.GUI, tekstowy terminal 80x25) + grafika z EmuTOS (mono VDI,
fonty) + shifter ST oraz VOFA/ET4000 (chunky 8bpp, liniowy VRAM 0xC00000).

Cel: pulpit jak w TOS — okna **przesuwane z zawartością**, otwieranie/zamykanie programów,
i **zgodność z aplikacjami GEM** (AES/VDI) — przy wysokiej płynności na 68030/TF536.

---

## 1. Zasady naczelne (decyzje, które warunkują wydajność)

1. **Kompozytor jest właścicielem jedynego ekranu.** Aplikacje nie piszą do ekranu —
   piszą do **własnych surface'ów** (backing store). Ekran = tło + okna w kolejności Z,
   składane przez kompozytor.
   *Dlaczego:* to eliminuje klasę błędów, którą właśnie widzieliśmy (dwie aplikacje piszące
   do jednego bufora, osobne bufory per sesja, zwis przy przełączaniu). Nie walczymy
   o bufor — każdy ma swój, a ekran składa się z nich.
2. **Zniszczenie (damage) jest walutą.** Każda operacja zgłasza prostokąty do odświeżenia;
   kompozytor składa **tylko** sumę prostokątów (z scalaniem i przycinaniem). Nigdy
   „pełny redraw", poza zmianą trybu.
3. **Ruch = blit, nie przerysowanie aplikacji.** Przesuwanie okna kopiuje **piksele
   surface'u** okna; aplikacja nie dostaje ani jednego zdarzenia rysowania. To jest
   różnica między „płynne" a „klatka na sekundę".
4. **Overlay na kursor i ramkę przeciągania.** Kursor (i ewentualnie gumowa ramka) jest
   rysowany przez overlay sprzętowy/kompozytor i nie wywołuje damage pod sobą.
5. **Aplikacja GEM nie wie o kompozytorze.** VDI pisze do okna/workstation; AES
   dostarcza zdarzenia. Dzięki temu istniejące .PRG działają bez rekompilacji.

---

## 2. Warstwy

\`\`\`
   DESKTOP.APP (powłoka: ikony, .INF, shel_*, pasek menu)
        |
   AES  (okna, obiekty, menu, zdarzenia, komunikaty, RSC)      <- serwer systemowy
        |
   VDI  (workstation: atrybuty, clipping, primitywy, ROP, fonty) <- serwer systemowy
        |
   GDOS/fonty (8x16 mono, .FNT)      GEMDOS/BIOS/XBIOS persona (.PRG, uchwyty, DTA)
        |                                        |
   -----------------------------------------------------------------
   KERNEL ASF: kmem, PROC (RR + wait), EVENT/IPC, VFS/FAT, kgfx (surfaces,
   dirty, blitter, kursor), SESSIONS (tts), BLOCK (ACSI/IDE + cache)
\`\`\`

AES i VDI to **moduły usługowe** (jak SYSTEM.GFX/GUI dzisiaj) — nie biblioteki linkowane
do aplikacji. Aplikacja GEM dostaje je przez trap #2 (zgodność) albo przez IPC (natywnie).

---

## 3. Model wykonania (to decyduje o „otwieraniu i zamykaniu jak w TOS")

GEM jest **kooperacyjny i jednowątkowy** w obrębie aplikacji. My mamy kernel wywłaszczający.
Pogodzenie obu: **AES wielozadaniowy, aplikacja jednowątkowa**.

- Każdy proces-aplikacja ma **kontekst AES**: własny mailbox (już mamy IPC), własną listę okien,
  własne workstation VDI, własny wskaźnik do drzewa obiektów i RSC.
- Wywołania AES/VDI są **synchroniczne dla aplikacji** (wchodzą przez trap/IPC i czekają na
  odpowiedź). `evnt_multi` = oczekiwanie na mailbox → kernel usypia zadanie i **żyje dalej**
  (to jest przewaga nad TOS: inne aplikacje i pulpit pracują).
- **Brak stanu globalnego „na aplikację"** — wszystko w tablicach per PID
  (tak jak `gui_keyboard_pids[]`, `terminal_owners[]`).
- **Izolacja awarii**: wyjście procesu zwalnia jego okna, workstation, surface'y i uchwyty
  (mamy już `ksurface_release_owner`, `user_terminal_release`). Pulpit nie ginie razem z aplikacją.
- **Bez blokowania pulpitu**: operacje synchroniczne aplikacji (np. `Fread`) nie mogą
  zatrzymywać kompozytora — kompozytor to osobne zadanie/etap w pętli kernela.

---

## 4. Rendering i wydajność — sedno projektu

### 4.1 Surface per okno
- Każde okno ma surface w formacie ekranu (mono planar / 2-4bpp planar / 8bpp chunky).
- Aplikacja rysuje **bez clippingu do ekranu** (clipping tylko do własnego okna) — VDI
  jest przez to tanie: brak sprawdzania zasłonięć przy każdym pikselu.
- Surface powstaje przy `wind_open`, znika przy `wind_close`; alokacja z kmem (mamy).

### 4.2 Lista prostokątów przycięcia (clip list) — wzorzec XaAES/EmuTOS
- Menedżer okien utrzymuje dla każdego okna **listę widocznych prostokątów**
  (okno minus okna wyżej minus granice ekranu minus ramka).
- VDI przy składaniu i rysowaniu przycina do tej listy. Aktualizacja listy tylko przy
  zmianie Z/topologii — tania.
- EmuTOS/XaAES mają dokładnie ten model (`w_rlist`), więc idziemy sprawdzoną drogą.

### 4.3 Kompozycja: recompose z damage (rekomendacja)
Dwa warianty, wybieramy **B**:

**A. Save-under (pamięć pod oknem).** Ruch = skopiuj pod spód, blitnij okno, odtwórz.
Tanie dla małych okien, ale pamięć rośnie i są pułapki (wiele okien, dialogi).

**B. Recompose z listą przycięcia.** Ekran = tło + okna; ruch okna = **rekompozycja
sumy (stary ∪ nowy prostokąt okna)**, z przycięciem do listy i Z-order. Koszt = obszar
dotknięty, nie cały ekran. Brak stanu „pod spodem", więc brak artefaktów.

Koszt ruchu okna o wymiarach W×H: `blit tła + blit okna` po prostokątach — czyli
~2× pole okna, **bez udziału aplikacji**. Na ET4000 (chunky, memcpy) to praktycznie
prędkość pamięci; na shifterze planar dochodzi konwersja, ale i tak blitter ST pomaga.

### 4.4 Ścieżki szybkie (konkretnie, z tego co już jest)
| Sytuacja | Mechanizm | Co mamy |
|---|---|---|
| mono 640x400 (shifter) | blitter ST: SRC/AND/HOP, kopie słowami | `kblit_*` |
| ET4000 8bpp | liniowy chunky: kopie 32-bit, LUT palety | `kvideo_*`, LUT w `video8` |
| tekst 8x16 | font jako bitmapa + blit (nie pętla po pikselach) | `term_render_mono`, LUT |
| kursor | overlay, bez repaintu tła | `kgfx_cursor_*` |
| brak migotania | backbuffer + flip na vsync | `desktop_backbuffer` |
| scalanie odświeżeń | dirty rectangles | `kgfx_dirty`, `gui_damage.h` |
| powierzchnie | handle z kontrolą właściciela | `ksurface_*` |

### 4.5 Przeciąganie okna „z zawartością" — algorytm
1. `evnt_multi` zwraca przycisk na pasku tytułu → AES wchodzi w **tryb przesuwania**.
2. Pętla przesuwania (w AES, nie w aplikacji):
   - czytaj mysz (bez zdarzeń do aplikacji),
   - policz nowy prostokąt okna,
   - **damage = star_rec ∪ nowy_rec**, przycięty do ekranu,
   - rekompozycja tylko tych prostokątów (blit tła + blit okien w Z),
   - kursor jako overlay.
3. Puszczenie przycisku → koniec trybu, jeden komunikat `WM_MOVED` do aplikacji
   (żeby znała nową pozycję), nic więcej.
   *Klucz:* aplikacja **nie** uczestniczy w rysowaniu ramki ani zawartości — dlatego to jest płynne.

### 4.6 Budżet i pomiar
- Cel: przesuw okna 300×200 na ET4000 — **kilka ms/klatka** (blit ~120 KB w obie strony);
  na 8 MHz 68000 + shifter — kilkanaście ms (blitter + konwersja), czyli 25-50 Hz.
- Pomiar mamy: `ticks()` (50 Hz) + istniejące benchmarki SYSINFO (CPU/RAM/dysk).
  Dodamy licznik „klatek kompozytora" i „pikseli/s" — bez zgadywania.
- Twarda reguła: **żadnych pełnych redrawów** w pętli zdarzeń; pełny redraw tylko przy
  zmianie trybu/rozdzielczości.

---

## 5. VDI

- **Workstation** = kontekst rysowania okna: `v_opnvwk` tworzy (per okno), `v_clsvwk` zamyka.
- Atrybuty: kolor, wzór, tryb zapisu (ROP), clip, inkarnacja linii/tekstu, tryb wypełnienia.
- Primitwy: `v_pline/v_bar/v_gtext/v_fillarea/v_rbox...` — implementowane na surface + clip list.
- Tryby: mono 1bpp, 2/4bpp (planar, shifter), 8bpp (chunky, ET4000) — jedna abstrakcja
  „pixel writer", trzy backendy.
- **Line-A**: emulacja dla starych aplikacji korzystających z blittera/linii przez Line-A
  (mamy już w personie TOS) — nie musi być szybka, musi być zgodna.
- Fonty: 8x16 (mamy), docelowo GDOS `.FNT` (wektorowe/bitmapowe) — na start tylko 8x16.

## 6. AES

- **Okna**: `wind_create/open/close/delete/get/set/calc/find/update`; ramka w stylu TOS
  (tytuł, mover, closer, info, pełny/zamknij), konfigurowalna przez `wind_set`.
- **Obiekty**: drzewo OBJECT, `objc_draw/find/add/delete/change/offset`, stany
  (SELECTED/CROSSED/DISABLED/OUTLINED), `form_*/`.
- **Menu**: `menu_bar/title/item/icheck/ienable/register/unregister`, rozwijane panelede.
- **Zdarzenia**: `evnt_keybd/button/mouse/mesag/multi/timer/dclick` — wszystko na naszym
  IPC: aplikacja śpi w mailboxie, AES budzi ją zdarzeniem.
- **Komunikaty**: standardowe 8 longwordów GEM (`WM_REDRAW/WM_CLOSED/WM_MOVED/...`)
  + własne rozszerzenia; `appl_write`/`appl_find`.
- **RSC**: parser skompilowanych zasobów (`.RSC`) — deski, drzewa, łańcuchy, obrazy;
  `rsrc_load/free/gaddr/obfix/rc_intersect`.
- **Schowek/system**: `shel_get/put/write/read/find/envrn`, `fsel_*`, `appl_init/exit`.

## 7. Zgodność z aplikacjami GEM (bez rekompilacji)

Ścieżka zgodności = **persona TOS** (już istnieje w drugim drzewie: `tos_prg.c`):
- **Loader .PRG**: nagłówek 28 B, relokacje, `Mshrink`, basepage — mamy.
- **Traps**: #2 AES/VDI, #1 GEMDOS, #13 BIOS, #14 XBIOS, Line-A — mamy dispatch.
- **GEMDOS**: uchwyty plików, DTA, `Fsfirst/Fsnext`, katalogi, `Pexec` — mamy.
- **AES/VDI**: do podmiany z „zaślepki" na **prawdziwy serwer AES/VDI** z tego projektu
  (dzisiaj `aesvdi` w personie zwraca minimalny zestaw).
- Aplikacja GEM staje się **procesem ASF** z własnym kontekstem AES — czyli dostaje
  wielozadaniowość „za darmo", a pulpit widzi jej okna.

Czego brakuje do pełnej zgodności (kolejność): GDOS/fonty → `form_*`/`fsel_*` →
pełny zestaw `objc_*` → tryby kolorów → `appl_*`/schowek.

## 8. Plan wdrożenia (etapy z kryterium zaliczenia)

| Etap | Zakres | Kryterium |
|---|---|---|
| P1 | VDI core na surface'ach: `opnvwk`, bar/fill/frame/text/line, clip list | okno z ramką i tekstem rysowane do własnego surface'u |
| P2 | Kompozytor: lista okien, Z-order, damage, recompose, flaga vsync | dwa okna, brak artefaktów, brak pełnych redrawów |
| P3 | AES-okna: `wind_*`, mover/closer, **ruch z zawartością**, `WM_*` | przesuwanie 300×200 płynne (pomiar) |
| P4 | AES-obiekty/menu/RSC + `form_*` (proste deski) | okno dialogowe z przyciskami, reakcja na klik |
| P5 | Persona GEM: trap #2 → nasz AES, `.PRG` z AES | uruchomiona istniejąca aplikacja GEM |
| P6 | DESKTOP.APP: ikony, `.INF`, `shel_*`, pasek menu, dwuklik | pulpit jak TOS, otwieranie/zamykanie programów |

## 9. Ryzyka i jak je zdejmujemy

1. **Jeden ekran w kernelu, sesje tts.** Kusiło nas „osobny bufor na sesję" — to zamroziło
   system. Wniosek: **kompozytor jest jedynym właścicielem framebuffera**; sesje tts to
   konteksty powłoki/klawiatury, a nie osobne ekrany graficzne. Aplikacje graficzne mają
   surface'y i żyją w jednym pulpicie.
2. **Blokujące operacje.** Długie synchroniczne I/O aplikacji nie może wstrzymywać
   kompozytora — kompozycja w pętli kernela, I/O w zadaniach (mamy PROC/EVENT).
3. **Wydajność shiftera.** Planar mono wymaga konwersji — mitygacja: blitter + LUT + brak
   pełnych redrawów. ET4000 (chunky) to ścieżka docelowa.
4. **Zgodność AES.** Idziemy zestawem, którego używają realne aplikacje (najpierw to, co
   woła DESKTOP/kompilatory/Norton), a nie „cały AES naraz".
5. **Regresje jak przy przełączaniu sesji.** Każdy etap: test w Hatari + pomiar, dopiero
   potem następny. Żadnych zmian „w locie" w grafice bez testu.

---

## 10. Jedno zdanie podsumowania
Pulpit budujemy jako **jeden kompozytor z listą przycięcia i damage**, na **surface'ach per
okno**, z VDI/AES jako serwerami i personą TOS dla zgodności — dzięki temu przesuwanie okien
z zawartością jest blitem, a nie przerysowywaniem, i nic nie może „zamalować" cudzego ekranu.
