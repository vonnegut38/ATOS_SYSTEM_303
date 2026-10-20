#!/bin/sh
# ATOS v10 - launch the clean v10 boot image in Hatari.
# BIOS, BOOT.SYS and KERNEL.SYS all come from commander-v10/build/.
set -e
DIR="$(cd "$(dirname "$0")" && pwd)"
IMG="$DIR/work/atos-v10-clean.img"
[ -f "$IMG" ] || IMG="$DIR/work/atos-v10.img"   # fallback
if [ ! -f "$IMG" ]; then
    echo "No $IMG - run: gmake image-v10" >&2
    exit 1
fi
echo "ATOS v10: BIOS=$DIR/build/bios_mste.bin  IMAGE=$IMG"
exec hatari --machine megaste --monitor mono --cpulevel 0 --cpuclock 16 --memsize 4 \
    --tos "$DIR/build/bios_mste.bin" --patch-tos false \
    --acsi 0="$IMG" "$@"