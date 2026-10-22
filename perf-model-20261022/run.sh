#!/bin/sh
# Model kompozycji ATOS 2.1 na hoscie: PRAWDZIWE wm_core.c, wm_blit.c i kernel_video.c
# z repozytorium (kernel_video.c kopiowany z JEDNYM licznikiem kdbg_scan_words).
# Uruchom z katalogu repozytorium (tam, gdzie lezy "ATOS 2.1"). Wymaga gcc lub clang.
set -e
D=$(cd "$(dirname "$0")" && pwd); A=${ATOS:-"ATOS 2.1"}; O=${OUT:-/tmp/atos-perf-model}; mkdir -p "$O"
CC=${CC:-cc}
python3 - "$A/system/kernel_video.c" "$O/kvideo_model.c" <<'PY'
import sys
s=open(sys.argv[1]).read()
a="uint32_t kdbg_dmg_px,kdbg_flush_words,kdbg_flush_calls,kdbg_flush_area;"
b="   for(x=begin;x+1u<end;x+=2u){"
if s.count(a)!=1 or s.count(b)!=1: sys.exit("kernel_video.c zmienil sie: nie ma gdzie wstawic licznika")
s=s.replace(a,a[:-1]+",kdbg_scan_words;").replace(b,"   kdbg_scan_words+=(uint32_t)(end-begin);\n"+b)
open(sys.argv[2],"w").write(s)
PY
grep -q 'kdbg_scan_words+=' "$O/kvideo_model.c" || { echo "kernel_video.c zmienil sie: brak miejsca na licznik"; exit 1; }
$CC -std=c11 -O2 -I"$A/include" "$D/model.c" "$O/kvideo_model.c" "$A/system/wm_core.c" "$A/system/wm_blit.c" "$D/stubs.c" -o "$O/model"
"$O/model"
