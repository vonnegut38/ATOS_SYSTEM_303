#!/bin/sh
# Boot the ATOS v10 + EmuTOS (RAM-TOS) demo image.
set -e
DIR="$(cd "$(dirname "$0")" && pwd)"
IMG="$DIR/work/atos-v10-emutos.img"
HB="$DIR/../atari-030-port/hatari-tf536"
[ -x "$HB" ] || HB="hatari"     # fall back to system Hatari
[ -f "$IMG" ] || { echo "run: gmake image-v10-emutos" >&2; exit 1; }
exec "$HB" --machine megaste --monitor mono --cpulevel 0 --cpuclock 16 --memsize 4 \
  --tos "$DIR/build/bios_mste.bin" --patch-tos false --acsi 0="$IMG" "$@"
