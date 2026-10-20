import sys, os, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
HB = "/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536"
IMG = P + "/work/atos-v2.img"
OUT = "/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/tt536"
NOACSI = []
for i in range(8): NOACSI += ["--acsi", "%d=none" % i]
EXTRA = ["--machine","megast","--cpulevel","3","--cpuclock","32","--memsize","4",
         "--ttram","64","--tf536","on","--graphics-card","vofa","--gfx-chip","et4000","--mono",
         "--ide-master", IMG] + NOACSI
for run in range(3):
    h = Hatari(P, log=P + "/work/hatari-stab-%d.log" % run, window=True, image=IMG, binary=HB, cfg=False, extra=EXTRA)
    t0=time.time()
    try:
        h.wait(40)
        ok = h.screenshot(OUT + "/stab-%d.png" % run, settle=0.8)
        print("run", run, "shot", ok, "%.0fs" % (time.time()-t0), flush=True)
    finally:
        h.close()
