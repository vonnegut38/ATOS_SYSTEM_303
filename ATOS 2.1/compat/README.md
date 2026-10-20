# ATOS compatibility core (wariant B)

Cel: użyć EmuTOS (GPL-2.0-or-later) jako warstwy GEMDOS/BIOS/XBIOS/VDI/AES **nad**
jądrem ATOS i `video_driver_t`. ATOS zachowuje BIOS/boot/sprzęt oraz kompozytor;
EmuTOS daje zgodność GEM.

## Kroki

1. Zbudować vendored źródła EmuTOS (`files-pasted-by-the-user-projekt/work/emutos-reference`).
2. Wybrać minimalny zestaw obiektów GEMDOS/VDI/AES.
3. Dostarczyć klej ATOS: `tosvars` (0x380-0x800), wektory, cookie jar, kolejkę VBL.
4. Skierować rysowanie VDI przez `video_driver_t` (shifter/et4000).
5. Podłączyć ładowanie .PRG (`bdos/kpgmld.c`) zamiast LEGACY `tos_prg`.

## Licencja

Wszystko pochodne od EmuTOS jest **GPL-2.0-or-later**; patrz
`licenses/emutos-NOTICE` i `licenses/emutos-GPL-2.0.txt`. Źródła publikujemy.
Analiza: `docs/EMUTOS-INTEGRATION.md`.
