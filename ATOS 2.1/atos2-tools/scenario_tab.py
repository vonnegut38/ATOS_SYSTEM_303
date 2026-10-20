import sys, os
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
HB = "/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536"
IMG = P + "/work/atos-v2.img"
OUT = "/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/hw"
NOACSI = []
for i in range(8): NOACSI += ["--acsi", "%d=none" % i]
EXTRA = ["--machine","megast","--cpulevel","3","--cpuclock","32","--memsize","4",
         "--ttram","64","--tf536","on","--graphics-card","vofa","--gfx-chip","et4000","--mono",
         "--ide-master", IMG] + NOACSI
h = Hatari(P, log=P + "/work/hatari-tab.log", window=True, image=IMG, binary=HB, cfg=True, extra=EXTRA)
try:
    h.wait(15)
    h.type("dir", delay=0.12); h.press("\n", delay=0.1); h.wait(1.5)
    print("dir", h.screenshot(OUT + "/t1-dir.png", settle=0.8), flush=True)
    h.press("Up", delay=0.4); h.wait(0.5)
    print("hist", h.screenshot(OUT + "/t2-hist.png", settle=0.8), flush=True)
    h.press("\n", delay=0.1); h.wait(0.8)
    h.type("type n", delay=0.12); h.press("Tab", delay=0.5); h.wait(0.8)
    print("tab", h.screenshot(OUT + "/t3-tab.png", settle=0.8), flush=True)
finally:
    print("rc", h.close())
