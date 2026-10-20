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
h = Hatari(P, log=P + "/work/hatari-tab4.log", window=True, image=IMG, binary=HB, cfg=True, extra=EXTRA)
try:
    h.wait(20)
    # file completion: type n -> notes.txt (lowercase)
    h.type("type n", delay=0.15); h.wait(0.4)
    print("f0", h.screenshot(OUT + "/u1-before.png", settle=0.7), flush=True)
    h.press("Tab", delay=0.7); h.wait(0.6)
    print("f1", h.screenshot(OUT + "/u2-file.png", settle=0.7), flush=True)
    h.press("\n", delay=0.1); h.wait(1.5)
    print("f2", h.screenshot(OUT + "/u3-filetxt.png", settle=0.7), flush=True)
    # directory completion: cd sy -> SYSTEM\ (uppercase + backslash)
    h.type("cd sy", delay=0.15); h.wait(0.4)
    h.press("Tab", delay=0.7); h.wait(0.6)
    print("d1", h.screenshot(OUT + "/u4-dir.png", settle=0.7), flush=True)
    h.press("\n", delay=0.1); h.wait(0.8)
    print("d2", h.screenshot(OUT + "/u5-cdok.png", settle=0.7), flush=True)
finally:
    print("rc", h.close())
