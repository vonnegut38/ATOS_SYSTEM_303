#!/bin/zsh
# ATOS - TEST DZWIEKU PRZEZ EMULOWANE ESP (NF_ATOS_SOUND / ASND), 15.10.2026
#
# DLACZEGO TAK:
#  - Dzwiek PRE2 nie idzie przez PSG, tylko do emulowanego ESP/ASND.
#    W Hatari to urzadzenie NatFeats "NF_ATOS_SOUND" (src/debug/natfeats.c:314),
#    a kernel ATOS probkuje je przez NatFeats (system/kernel_sound_nf.c) i bez
#    niego zwraca KSND_E_UNAVAILABLE (-0x400) - czyli cisza.
#  - Binarka ATOS_AKTUALNY/hatari (10.10) NIE ZAWIERA "NF_ATOS_SOUND"
#    (strings | grep -c = 0), wiec z nia dzwiek nie zagra nigdy.
#    Ta binarka (fork atari-030-port, build 14.10) ma to urzadzenie.
#  - Stad: fork + --natfeats on + --sound 44100.  Reszta flag 1:1 jak Wasz
#    launcher (TT-RAM 64, TF536, MMU off, ten sam cfg i BIOS).
# W GRZE: F8 wlacza/wylacza muzyke (wlaczenie startuje utwor od poczatku).
# Wasz obraz ATOS_AKTUALNY/ATOS.img i Wasza instancja Hatari NIE sa dotykane.
set -eu
cd -- "${0:A:h}"
A=../ATOS_AKTUALNY
H=../atari-030-port/hatari-src/build/src/hatari
exec "$H" -c "$A/hatari.cfg" --machine megast --cpulevel 3 --cpuclock 32 --memsize 4 --grab --mousewarp true --ttram 64 --tf536 on --graphics-card vofa --gfx-chip et4000 --mono --fast-forward off --ide-master ./demo-test-20261015.img --tos "$A/BIOS.bin" --patch-tos false --mmu off --statusbar false --borders false --zoom 1 --sound 44100 --natfeats on --confirm-quit false --disable-video off --window --acsi 0=none --acsi 1=none --acsi 2=none --acsi 3=none --acsi 4=none --acsi 5=none --acsi 6=none --acsi 7=none --log-file ./hatari-test-esp-20261015.log "$@" >./hatari-test-esp-20261015-console.log 2>&1
