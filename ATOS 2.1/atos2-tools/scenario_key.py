import sys, os
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
h = Hatari(P, log=P + "/work/hatari-key.log", window=True, image=IMG, binary=HB, cfg=False, extra=EXTRA)
try:
    h.wait(35)
    print("pre", h.screenshot(OUT + "/k1-before-key.png", settle=0.8), flush=True)
    h.press("Return", delay=0.2); h.wait(2.0)
    print("post", h.screenshot(OUT + "/k2-after-key.png", settle=0.8), flush=True)
    h.type("memory", delay=0.1); h.press("\n", delay=0.1); h.wait(2.5)
    print("mem", h.screenshot(OUT + "/k3-memory.png", settle=0.8), flush=True)
finally:
    print("rc", h.close())
