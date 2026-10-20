import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
HB = "/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536"
IMG = P + "/work/atos-v2.img"
OUT = "/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/boot"
NOACSI = []
for i in range(8): NOACSI += ["--acsi", "%d=none" % i]
EXTRA = ["--machine","megast","--cpulevel","3","--cpuclock","32","--memsize","4",
         "--ttram","64","--tf536","on","--graphics-card","vofa","--gfx-chip","et4000","--mono",
         "--ide-master", IMG] + NOACSI
h = Hatari(P, log=P + "/work/hatari-bioskey.log", window=True, image=IMG, binary=HB, cfg=True, extra=EXTRA)
try:
    h.wait(5.0)
    h.press("F1", delay=0.3)
    h.wait(2.0)
    print("k1", h.screenshot(OUT + "/k1.png", settle=0.8), flush=True)
    h.wait(6.0)
    print("k2", h.screenshot(OUT + "/k2.png", settle=0.8), flush=True)
finally:
    print("rc", h.close())
