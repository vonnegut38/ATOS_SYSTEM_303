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
h = Hatari(P, log=P + "/work/hatari-app.log", window=True, image=IMG, binary=HB, cfg=True, extra=EXTRA)
try:
    h.wait(20)
    h.type("hw", delay=0.15); h.press("\n", delay=0.1); h.wait(2.0)
    print("a1", h.screenshot(OUT + "/a1-hw-noext.png", settle=0.8), flush=True)
    h.type("dir", delay=0.15); h.press("\n", delay=0.1); h.wait(1.5)
    print("a2", h.screenshot(OUT + "/a2-dir-alive.png", settle=0.8), flush=True)
    h.type("nosuchprog", delay=0.12); h.press("\n", delay=0.1); h.wait(1.5)
    print("a3", h.screenshot(OUT + "/a3-missing.png", settle=0.8), flush=True)
    h.type("dir", delay=0.15); h.press("\n", delay=0.1); h.wait(1.5)
    print("a4", h.screenshot(OUT + "/a4-dir-alive2.png", settle=0.8), flush=True)
    h.type("type n", delay=0.15); h.press("Tab", delay=0.7); h.wait(0.5)
    print("a5", h.screenshot(OUT + "/a5-tab.png", settle=0.8), flush=True)
finally:
    print("rc", h.close())
