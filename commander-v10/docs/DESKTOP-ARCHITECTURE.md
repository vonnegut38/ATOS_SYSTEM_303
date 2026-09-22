# ATOS Desktop — kierunek projektowy (v10, do zaimplementowania później)

Dokument **przewidujący** architekturę pulpitu. Nie implementujemy go teraz —
ustalamy kontrakty, żeby Phase 7-13 (`video_driver_t`/`video_surface_t`) i przyszły
AES nie zamknęły sobie tej drogi.

## 1. Cel

- Pulpit **graficzny od startu**, na `video_driver_t`.
- Docelowo **ET4000 / ET4000-W32i, 1024x768, 8 bpp (256 kolorów)**, CHUNKY.
- Bez karty: **Shifter** (PLANAR 1/2/4bpp) — ten sam interfejs, fallback software.
- Wygląd **modułowy**: motywy i aplety ładowane przy starcie z katalogu, nie zaszyte.

## 2. Warstwy

~~~
DESKTOP.APP (powłoka: tło, ikony, okna, dock)
   ├── MODULES/  (aplety paska: zegar, sieć, monitor, przełącznik desktopów)
   ├── THEMES/   (motyw: zasoby, palety, grafiki ramek, fonty)
   └── AES/VDI (przyszłość)  ->  video_surface_t  ->  video_driver_t
~~~

Pulpit jest **klientem kompozytora** — nie zna sprzętu i nie pisze do VRAM.
Wszystko rysuje do własnej powierzchni przez VDI/kompozytor (zasada z §7-13 v10).

## 3. Moduły wyglądu i aplety (kontrakt)

Katalog startowy (docelowo, obok `BIOS.CFG`):

~~~
C:\SYSTEM\DESKTOP\
  DESKTOP.CFG        lista modułów + motyw + geometria (1024x768, dock, pasek)
  THEMES\<nazwa>\   theme.cfg + zasoby (RSC/PNG/BMP/ikony/fonty/paleta)
  MODULES\<nazwa>\  modul.app + modul.cfg (aplet paska / launcher / przełącznik)
~~~

Minimalny ABI modułu (projekt, nie kod): `init(cfg)`, `draw(surface, rect)`,
`tick(now)`, `event(ev)`, `configure()`, `shutdown()`. Moduł **nie zna**
ET4000 ani Shiftera — dostaje `video_surface_t` i region.

Zastosowania docelowe (kolejność późniejsza):
- górny pasek zadań: **zegarek, stan sieci, ustawienia monitora, przełączanie desktopów**,
- **launcher programów na dole** (styl macOS/Linux dock),
- motywy przełączalne bez restartu.

## 4. Ikony i grafiki — pochodzenie i licencje

- Ikony można wziąć z **otwartych dystrybucji** (zestawy ikon Linux), ale **tylko
  z licencją zezwalającą na użycie i redystrybucję** (public domain/CC0, MIT/BSD,
  Apache-2.0). Unikamy zestawów GPL, jeśli nie chcemy rozciągać GPL na motyw
  (projekt i tak akceptuje GPL, ale decyzję o zasięgu podejmujemy świadomie).
- **Każdy import**: zapisujemy źródło, wersję, autora i licencję w
  `licenses/` + `docs/ASSETS.md` (ta sama dyscyplina co dla EmuTOS).

## 5. Zasady spójne z audytem v10

1. `(0,0)` = lewy górny róg; X w prawo, Y w dół (jedna konwencja).
2. Żadnych `wm_app_flip`-podobnych łat — konwersję planar/chunky robi backend/blitter.
3. Kompozytor = jedyny właściciel framebuffera; moduły rysują do surface'ów.
4. Brudne prostokąty; zakaz pełnego redrawu w pętli zdarzeń.
5. Zero wiedzy o sprzęcie w VDI/AES/Desktop/modułach — tylko `video_driver_t`.
6. 1024x768x8 wymaga `video_surface_t` z `pitch` i formatem CHUNKY_8BPP; Shifter
   pozostaje PLANAR — ten sam interfejs, inny backend.

## 6. Czego teraz NIE robimy

Żadnego kodu pulpitu, widgetów, motywów, animacji ani W32i. Ten dokument jest
wyłącznie kontraktem, który ma przetrwać fazy 7-13.
