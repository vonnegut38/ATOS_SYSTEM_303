#!/bin/sh
#
# ATOS 2.0 na docelowej konfiguracji: Mega ST + TF536 (68030, 64 MB ALT-RAM,
# IDE) + karta ET4000 (VOFA/STGA). Uruchamia lokalnie zbudowany, spatchowany
# Hatari z ../atari-030-port (opcje --tf536 / --graphics-card nie istnieja w
# stockowym /opt/homebrew/bin/hatari).
#
#   ./run-tf536.sh              # ET4000 (VOFA), boot z IDE TF536
#   GFX=shifter ./run-tf536.sh  # sam Shifter
#   BOOT=acsi ./run-tf536.sh    # boot z ACSI zamiast IDE
#
# Konsola dziala na obu wariantach (przy VOFA rysuje wprost do VRAM karty).
# IDE/ALT-RAM stabilne, TT RAM widoczny w "memory".
#
set -e
DIR=$(cd "$(dirname "$0")" && pwd)
HB="$DIR/../atari-030-port/hatari-tf536"
IMAGE="$DIR/work/atos-v2.img"
BIOS="$DIR/build/bios_mste.bin"
[ -x "$HB" ] || { echo "Brak $HB (zbuduj atari-030-port/hatari-src)." >&2; exit 1; }
[ -f "$IMAGE" ] || { echo "Brak $IMAGE." >&2; exit 1; }

if [ "${GFX:-vofa}" = "shifter" ]; then CARD=none; else CARD=vofa; fi

if [ "${BOOT:-ide}" = "acsi" ]; then
    exec "$HB" \
        --machine megast --cpulevel 3 --cpuclock 32 --memsize 4 --ttram 64 \
        --tf536 on --graphics-card "$CARD" --gfx-chip et4000 --mono \
        --tos "$BIOS" --patch-tos false \
        --acsi 0="$IMAGE" "$@"
fi

exec "$HB" \
    --machine megast --cpulevel 3 --cpuclock 32 --memsize 4 --ttram 64 \
    --tf536 on --graphics-card "$CARD" --gfx-chip et4000 --mono \
    --tos "$BIOS" --patch-tos false \
    --acsi 0=none --acsi 1=none --acsi 2=none --acsi 3=none \
    --acsi 4=none --acsi 5=none --acsi 6=none --acsi 7=none \
    --ide-master "$IMAGE" "$@"
