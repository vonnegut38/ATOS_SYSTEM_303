#!/bin/zsh
# ATOS - TEST APPLY / SETTINGS na KOPII z kompletu 15.10.2026.
# Obraz: display-demo/demo-test-20261015.img = kopia demo-poprawiony.img
#        + komplet z build/system (KERNEL.SYS 467080, SETTINGS.APP 5432,
#          GFX.APP 401, DESKTOP.APP 29433, ET4000.SYS 5564).
# Co testujemy: APPLY w SETTINGS ma pokazac KOD bledu (rc=-XXX), a przy -997
#        nie wchodzic w slepa uliczke ("PREVIEW RUNNING - ANSWER YES/NO").
# Wasz obraz ATOS_AKTUALNY/ATOS.img i Wasza instancja Hatari NIE sa dotykane.
# Ten launcher to dokladna kopia URUCHOM_DEMO_POPRAWIONY.command (ta sama
# binarka/konfiguracja/BIOS), zmieniony tylko obraz i nazwy logow - zeby
# porownanie z poprzednim "MODE REFUSED" bylo 1:1.
set -eu
cd -- "${0:A:h}"
A=../ATOS_AKTUALNY
exec "$A/hatari" -c "$A/hatari.cfg" --machine megast --cpulevel 3 --cpuclock 32 --memsize 4 --grab --mousewarp true --ttram 64 --tf536 on --graphics-card vofa --gfx-chip et4000 --mono --fast-forward off --ide-master ./demo-test-20261015.img --tos "$A/BIOS.bin" --patch-tos false --mmu off --statusbar false --borders false --zoom 1 --sound off --confirm-quit false --disable-video off --window --acsi 0=none --acsi 1=none --acsi 2=none --acsi 3=none --acsi 4=none --acsi 5=none --acsi 6=none --acsi 7=none --log-file ./hatari-test-20261015.log "$@" >./hatari-test-20261015-console.log 2>&1
