# Nowy system dla Atari Mega ST — TOS-emulujący, okienkowy, wielozadaniowy

Własny system oparty na źródłach **EmuTOS**: emuluje TOS, ale jest zaprojektowany
wokół **okien i wielozadaniowości**. Natywne aplikacje TOS działają każde
we własnym oknie, z możliwością przełączania. Start na ET4000, gdy wykryta;
inaczej na Shifterze.

- **Dokument kanoniczny:** [PLAN-00-FUNDAMENT.md](PLAN-00-FUNDAMENT.md)
- **Target:** Mega ST4 (4 MB ST-RAM) bez blittera + TF536 @ 50 MHz
  (64 MB ALT-RAM, IDE) + karta ET4000 przez interfejs VOFA w MegaBus + VGA.
- **Priorytety:** szybkość, bezpieczeństwo, kompatybilność z TOS, nowoczesny wygląd.
- **System na dysku:** C:\SYSTEM\ z podkatalogami BOOT, KERNEL, DRIVERS, TOS,
  APPS, CONFIG, THEMES, FONTS, AUTO, TEMP.

## Mapa repozytorium

| Ścieżka | Co to jest |
|---|---|
| PLAN-00-FUNDAMENT.md | obowiązujący plan i architektura (źródło prawdy) |
| files-pasted-by-the-user-projekt/ | BIOS, jądro, grafika, desktop, import EmuTOS VDI |
| atari-030-port/ | fork Hatari (TF536/VOFA), narzędzia, obrazy dysków, zrzuty |
| atari-030-port/hatari-tf536 | zbudowany emulator |
| atari-030-port/tools/ | generatory obrazów AHDI/FAT16/Minix, skrypty zrzutów |
| tools/ | dodatkowe narzędzia obrazów AHDI |
| pozostałe PLAN-*.md, analiza-systemu-atari.md, T2-DESKTOP.md | archiwum ustaleń |

## Szybki start (emulator)

    atari-030-port/hatari-tf536 --machine megast --cpulevel 3 --cpuclock 32 \
      --memsize 4 --ttram 64 --tf536 on --graphics-card vofa --gfx-chip et4000 \
      --mono --zoom 2 --tos atari-030-port/disk/emutos-pl-256k.img

Uwaga: Hatari przyjmuje `--cpuclock` tylko 8/16/32; 50 MHz to realny TF536, nie emulator.

Uwaga: build wymaga gmake (make 3.81 nie obsługuje celów &:) oraz
atari-030-port/tools/py.

## Stan

**M0 zrobione:** build odtwarzalny bit-for-bit (92/92 artefakty po clean),
testy check / check-graphics / check-wm / check-gui-apps zielone.
**Następny krok:** M1 — BIOS graficzny: detekcja ET4000 → VGA, fallback Shifter,
POST, logo i graficzny Setup (zegar, urządzenie bootowania, lista urządzeń).
