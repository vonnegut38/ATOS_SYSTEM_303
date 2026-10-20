import sys
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
h = Hatari(P, log=P + "/work/hatari-hw2.log", window=True, image=IMG, binary=HB, cfg=True, extra=EXTRA)
try:
    h.wait(20)
    h.type("hw.app", delay=0.15); h.press("\n", delay=0.1); h.wait(2.0)
    print("hw", h.screenshot(OUT + "/h3-hw.png", settle=0.9), flush=True)
    h.type("memory", delay=0.15); h.press("\n", delay=0.1); h.wait(1.0)
    print("mem", h.screenshot(OUT + "/h4-mem.png", settle=0.9), flush=True)
finally:
    print("rc", h.close())
