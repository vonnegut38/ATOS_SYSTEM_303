# ATOS DEMOS — paczka efektów (2026-10-22)

## Zawartość

`\DEMOS` (folder na dysku, ikona **DEMOS** na pulpicie):

| plik | efekt | rozmiar | metoda |
|---|---|---|---|
| PLASMA.APP | spirala/zoom/rotator | 320×200 | wzór + paleta 236..251, ~40 fps |
| TUNNEL.APP | tunel | 320×200 | wzór + paleta, ~40 fps |
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
- start efektu z file window w teście harness dawał `STARTED`, ale okno
  nie pojawiało się — do dalszej diagnozy spawnu z pulpitu.
