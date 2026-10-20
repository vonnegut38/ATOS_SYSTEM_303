#!/bin/zsh
# ATOS - TEST PRE2 Z SZYBKIM TRACKEREM (AS_CAP_TRACKER), 15.10.2026
#
# PO CO: PRE2 ma dwuzakresowa binarke (ta sama co na certyfikowanym obrazie
# Codexa, 270 656 B):
#   - gdy urzadzenie reklamuje AS_CAP_TRACKER (bit 7 z GET_CAPS), gra wysyla
#     ORYGINALNY MODUL (12 modulow razem 463 944 B) i dekoduje go emulowane ESP
#     (libxmp w Hatari) razem z efektami;
#   - bez tej zdolnosci spada na stary backend PCM: wysyla .MUS (41 480 974 B)
#     i to jest to dlugie "loading..." (MINES: 20,327 s czasu Atari).
# Zmierzone w PRE2/NAPRAWA-LOADING-ESC-20261013.txt:
#   przed: MINES.MUS 3 325 140 B -> 20,327 s;  po: MINES.MOD 38 764 B -> 1,066 s.
# Nasz fork (atari-030-port) NIE MA libxmp, wiec ZAWSZE wybieral wolna sciezke.
# Ta binarka (tracker-controls/ATOS Tracker.app, 13.10) ma tracker.
# Obraz: display-demo/demo-test-20261015.img (ma PRE2.APP, 12x .MOD i 12x .MUS
# oraz nasz kernel z poprawka -997).
# Wasz obraz ATOS_AKTUALNY/ATOS.img i Wasza instancja Hatari NIE sa dotykane.
set -eu
cd -- "${0:A:h}"
A=../ATOS_AKTUALNY
H=./hatari-tracker-20261013
exec "$H" -c "$A/hatari.cfg" --machine megast --cpulevel 3 --cpuclock 32 --memsize 4 --grab --mousewarp true --ttram 64 --tf536 on --graphics-card vofa --gfx-chip et4000 --mono --fast-forward off --ide-master ./demo-test-20261015.img --tos "$A/BIOS.bin" --patch-tos false --mmu off --statusbar false --borders false --zoom 1 --sound 44100 --natfeats on --confirm-quit false --disable-video off --window --acsi 0=none --acsi 1=none --acsi 2=none --acsi 3=none --acsi 4=none --acsi 5=none --acsi 6=none --acsi 7=none --log-file ./hatari-test-tracker-20261015.log "$@" >./hatari-test-tracker-20261015-console.log 2>&1
