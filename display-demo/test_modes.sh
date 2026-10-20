#!/bin/bash
IMG=${IMG:-/Users/xxx/Documents/Deepseek_test/display-demo/demo-okno.img}
HAT=/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-src/build/src/hatari
CFG="/Users/xxx/Documents/Codex/2026-10-04/na-gorze-kolorysstycznie-kolok-przek-amae/work/nf-listen-r6-20261013/atos2.cfg"
for m in "$@"; do
  w=${m%%x*}; rest=${m#*x}; h=${rest%%x*}; b=${rest#*x}
  { printf 'RES=%s\n' "$m"; tail -n +2 /Users/xxx/Documents/Deepseek_test/display-demo/desktop-cfg-wzor.txt; } > /tmp/desktop.cfg
  python3 /Users/xxx/Documents/Deepseek_test/display-demo/install_file.py "$IMG" "SYSTEM\DESKTOP.CFG" /tmp/desktop.cfg > /dev/null 2>&1
  pkill -f "hatari-src/build/src/hatari" 2>/dev/null; sleep 1
  nohup "$HAT" --configfile "$CFG" --tos "/Users/xxx/Documents/Deepseek_test/ATOS 2.1/build/bios_mste.bin" --patch-tos on \
    --ide-master "$IMG" --graphics-card vofa --gfx-chip et4000w32i --machine megast --cpulevel 3 --cpuclock 32 \
    > "/tmp/mode_$m.log" 2>&1 &
  sleep 32
  got=$(grep -E "VOFA-MODE" "/tmp/mode_$m.log" | tail -1)
  pkill -f "hatari-src/build/src/hatari" 2>/dev/null; sleep 1
  printf '%-14s -> %s\n' "$m" "${got:-BRAK LINII}"
done
