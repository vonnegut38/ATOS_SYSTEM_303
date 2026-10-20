#!/usr/bin/env python3
"""Co widac po starcie: konsola ATOS + liczniki bledow jadra (diagnostyka bootu)."""
import shutil, sys, time
from pathlib import Path
REAL = Path(__file__).resolve().parent.parent
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari

IMG = str(REAL / "work/atos-nftest.img")
OVERRIDE = ["--machine","megast","--memsize","8","--ttram","64","--cpuclock","32",
            "--cpulevel","3","--tf536","on","--graphics-card","vofa","--gfx-chip","et4000",
            "--sound","44100","--mmu","off",
            "--acsi","0=none","--ide-master", IMG]
(REAL / "work/build").mkdir(parents=True, exist_ok=True)
shutil.copy(str(REAL / "work/audio-baseline/BIOS-tree.bin"), str(REAL / "work/build/bios_mste.bin"))

h = Hatari(str(REAL / "work"), log=str(REAL / "work/boot-probe.log"), window=True,
           image=IMG, binary="/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536",
           cfg=True, extra=OVERRIDE)
try:
    time.sleep(50)
    print("=== konsola ATOS (ostatnie linie) ===", flush=True)
    try:
        lines = h.console_lines(rows=25, cols=80)
        for l in (lines or [])[-18:]:
            print(repr(l)[:100])
    except Exception as e:
        print("console_lines blad:", e)
finally:
    print("close rc=%s" % h.close(), flush=True)
