# ATOS DEMOS — paczka efektów (2026-10-22)

## Zawartość

`\DEMOS` (folder na dysku, ikona **DEMOS** na pulpicie):

| plik | efekt | rozmiar | metoda |
|---|---|---|---|
| PLASMA.APP | spirala/zoom/rotator | 160×100 | wzór + paleta 236..251, ~40 fps |
| TUNNEL.APP | tunel | 160×100 | wzór + paleta, ~40 fps |
| FLAMES.APP | ogień | 160×100 | symulacja ognia + BLIT8, ~20 fps |
| ROTOZOOM.APP | obrót+zoom tekstury | 160×100 | próbkowanie + BLIT8, ~20 fps |
| SCALER.APP | skalowanie | 160×100 | próbkowanie + BLIT8, ~20 fps |
| WOBBLE.APP | deformacja falkowa | 160×100 | próbkowanie + BLIT8, ~20 fps |

Wszystkie korzystają z `ATOS_CALL_BLIT8` i palety 236..251 (TOS), więc
UI/ikony nie zmieniają kolorów.

## Folder na pulpicie

Desktop obsługuje teraz wpis typu folder w `LAUNCH.CFG`:
`DEMOS=*D\DEMOS`; ikona `ICON_FOLDER`; klik otwiera file window tego
katalogu. `\DEMOS` tworzy `tools/img_mkdir.py` (FAT16).

## Build

`make TOS=1 demos` buduje `build/system/DEMO_*.APP`.
Gotowe aplikacje instalowane jako `\DEMOS\*.APP`.

## Stan testów

- folder/ikona: działa (zrzut `folder-icon.png`),
- file window listuje sześć aplikacji (`folder-open.png`),
- start SYSINFO z tego samego file window działa,
- start PLASMA/demo z file window w harness dawał `STARTED`, ale okno
  nie pojawiało się — problem specyficzny dla aplikacji BLIT8/PLASMA przy
  spawnie z pulpitu; do dalszej diagnozy (nie folderu).


## Korekta 160x100 (2026-10-22)

Wszystkie efekty są teraz 160×100 i korzystają z **statycznego bufora**
(bez `ATOS_CALL_ALLOC`), żeby wykluczyć limit heapu przy spawnie z pulpitu.
PLASMA/TUNNEL nadal animują tylko paletę (~40 fps), pozostałe liczą
16 tys. pikseli/klatkę (~20 fps).


## Stan WIP (2026-10-22, do dokończenia)

- folder `\DEMOS` + ikona + file window: **działa**,
- SYSINFO uruchomiony z tego samego file window: **działa**,
- PLASMA/demo apps: proces `STARTED`, ale **app_main nie dochodzi do okna**;
  ślad `FXT0..` nie powstaje, więc problem jest przed/na starcie aplikacji,
  nie w folderze ani w 160×100.
- następny krok: minimalna aplikacja zapisująca etapy (lookup GFX/GUI,
  GUI_BEGIN_KEYS, VIDEO_MODE, WINDOW, CONTENT, BLIT8) do **osobnych plików**
  `\FXTn.TXT` i porównanie z SYSINFO; potem poprawka.
