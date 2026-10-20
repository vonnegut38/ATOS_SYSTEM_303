#!/bin/zsh
# ATOS - TEST z DZWIEKIEM (ten sam obraz i ta sama konfiguracja co
# URUCHOM_TEST_20261015.command, zmienione WYLACZNIE --sound off -> --sound 44100).
# Po co: PRE2 gralo bez dzwieku, bo poprzedni launcher mial wpisane --sound off
# (w Hatari "off" = brak dzwieku i najszybsza emulacja).  Uwaga: ta binarka nie
# zna wartosci "on" - --sound przyjmuje CZESTOTLIWOSC (off/6000-50066).
# Wasz obraz ATOS_AKTUALNY/ATOS.img i Wasza instancja Hatari NIE sa dotykane.
set -eu
cd -- "${0:A:h}"
A=../ATOS_AKTUALNY
exec "$A/hatari" -c "$A/hatari.cfg" --machine megast --cpulevel 3 --cpuclock 32 --memsize 4 --grab --mousewarp true --ttram 64 --tf536 on --graphics-card vofa --gfx-chip et4000 --mono --fast-forward off --ide-master ./demo-test-20261015.img --tos "$A/BIOS.bin" --patch-tos false --mmu off --statusbar false --borders false --zoom 1 --sound 44100 --confirm-quit false --disable-video off --window --acsi 0=none --acsi 1=none --acsi 2=none --acsi 3=none --acsi 4=none --acsi 5=none --acsi 6=none --acsi 7=none --log-file ./hatari-test-dzwiek-20261015.log "$@" >./hatari-test-dzwiek-20261015-console.log 2>&1
