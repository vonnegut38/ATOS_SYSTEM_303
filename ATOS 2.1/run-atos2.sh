#!/bin/sh
set -eu
DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
exec /opt/homebrew/bin/hatari --configfile "$DIR/atos2.cfg" --machine ste --monitor mono --cpulevel 3 --cpuclock 16 --memsize 4 --ttram 0 --tos "$DIR/build/bios_mste.bin" --patch-tos false --acsi "0=$DIR/work/atos-v2.img" --sound off --confirm-quit false --cmd-fifo "$DIR/work/control.fifo" --log-file "$DIR/work/hatari.log" "$@"
