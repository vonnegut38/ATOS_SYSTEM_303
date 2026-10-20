#!/bin/zsh
# ATOS / SysInfo: poprawki odswiezania, 2026-10-08.
PROJECT_DIR="${0:A:h}"
exec /Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536 \
  -c "$PROJECT_DIR/atos2.cfg" \
  --machine megast --cpulevel 3 --cpuclock 32 --memsize 4 --ttram 64 \
  --tf536 on --graphics-card vofa --gfx-chip et4000 --mono --window \
  --sound off --confirm-quit false --fast-forward off \
  --tos "$PROJECT_DIR/work/bios-sysinfo-refresh.bin" --patch-tos false \
  --ide-master "$PROJECT_DIR/work/atos-sysinfo-refresh.img" \
  --acsi 0=none --acsi 1=none --acsi 2=none --acsi 3=none \
  --acsi 4=none --acsi 5=none --acsi 6=none --acsi 7=none
