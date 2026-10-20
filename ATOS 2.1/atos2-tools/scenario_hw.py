import sys, os
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
HB = "/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536"
IMG = P + "/work/atos-v2.img"
OUT = "/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/hw"
os.makedirs(OUT, exist_ok=True)
NOACSI = []
for i in range(8): NOACSI += ["--acsi", "%d=none" % i]
EXTRA = ["--machine","megast","--cpulevel","3","--cpuclock","32","--memsize","4",
         "--ttram","64","--tf536","on","--graphics-card","vofa","--gfx-chip","et4000","--mono",
         "--ide-master", IMG] + NOACSI
h = Hatari(P, log=P + "/work/hatari-hw.log", window=True, image=IMG, binary=HB, cfg=True, extra=EXTRA)
try:
    h.wait(15)
    h.type("hw.app", delay=0.12); h.press("\n", delay=0.1); h.wait(2.5)
    print("hw", h.screenshot(OUT + "/h1-hw.png", settle=0.8), flush=True)
    h.type("type no", delay=0.12); h.press("Tab", delay=0.4); h.wait(0.6)
    print("tab-file", h.screenshot(OUT + "/h2-tabfile.png", settle=0.8), flush=True)
    h.press("\n", delay=0.1); h.wait(1.0)
    h.type("type sy", delay=0.12); h.press("Tab", delay=0.4); h.wait(0.6)
    print("tab-dir", h.screenshot(OUT + "/h3-tabdir.png", settle=0.8), flush=True)
    h.press("\n", delay=0.1); h.wait(1.0)
    h.type("type sy", delay=0.12); h.press("Tab", delay=0.4); h.wait(0.5)
    h.type("default", delay=0.12); h.press("\n", delay=0.1); h.wait(1.5)
    print("dir2", h.screenshot(OUT + "/h4-dir2.png", settle=0.8), flush=True)
finally:
    print("rc", h.close())
