#!/bin/zsh
# ATOS - demo-final-claude.img (kopia demo-final.img + MODPLAY z przejsciem na
# Yamahe/PCM, gdy ESP32 nie ma trackera).  Claude, 16.10.2026.
# Binarka Z TRACKEREM (libxmp): PRE2 wysyla oryginalne moduly zamiast .MUS,
# wiec plansza laduje sie ~1 s zamiast ~2 min; MODPLAY gra przez ESP32.
# Pelne flagi z recepty: --tf536 on, --memsize 4, --natfeats on, --sound 44100.
set -eu
cd -- "${0:A:h}"
A=../ATOS_AKTUALNY
H=./hatari-tracker-20261013
exec "$H" -c "$A/hatari.cfg" --machine megast --cpulevel 3 --cpuclock 32 --memsize 4 --grab --mousewarp true --ttram 64 --tf536 on --graphics-card vofa --gfx-chip et4000 --mono --fast-forward off --ide-master ./demo-final-claude.img --tos "$A/BIOS.bin" --patch-tos false --mmu off --statusbar false --borders false --zoom 1 --sound 44100 --natfeats on --confirm-quit false --disable-video off --window --acsi 0=none --acsi 1=none --acsi 2=none --acsi 3=none --acsi 4=none --acsi 5=none --acsi 6=none --acsi 7=none --log-file ./hatari-demo-final-claude.log "$@" >./hatari-demo-final-claude-console.log 2>&1
