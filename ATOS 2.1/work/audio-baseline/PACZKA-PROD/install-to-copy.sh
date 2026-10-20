#!/bin/sh
# Apply the audio package to a COPY of the system image.  It refuses to touch the
# live set: ATOS_AKTUALNY and ATOS_KOPIA must not be modified until the package
# passes the acceptance run.
set -e
TARGET="$1"
[ -n "$TARGET" ] || { echo "usage: $0 <copy-of-ATOS.img>"; exit 2; }
case "$(cd "$(dirname "$TARGET")" && pwd)/$(basename "$TARGET")" in
  */ATOS_AKTUALNY/*|*/ATOS_KOPIA/*) echo "REFUSED: target is a live set ($TARGET)"; exit 3;;
esac
[ -f "$TARGET" ] || { echo "no such image: $TARGET"; exit 2; }
HERE="$(cd "$(dirname "$0")" && pwd)"
TOOLS="/Users/xxx/Documents/Deepseek_test/ATOS 2.1/tools"
python3 "$TOOLS/install_boot_sys.py" "$TARGET" "$HERE/KERNEL.SYS"                | tail -1
python3 "$TOOLS/install_boot_sys.py" "$TARGET" "$HERE/DESKTOP.APP" --directory SYSTEM/APP | tail -1
python3 "$TOOLS/install_boot_sys.py" "$TARGET" "$HERE/T.PRG"       --directory SYSTEM/APP | tail -1
echo "BIOS.bin: copy it OUTSIDE the image (the emulator loads it with --tos)"
echo "then verify: python3 \"$TOOLS/audio_manifest.py\" --image \"$TARGET\" --out manifest-check.md"
