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
h = Hatari(P, log=P + "/work/hatari-clean.log", window=True, image=IMG, binary=HB, cfg=True, extra=EXTRA)
try:
    h.wait(6)
    print("t6", h.screenshot(OUT + "/clean-t6.png", settle=0.6), flush=True)
    h.wait(16)
    print("t22-black", h.screenshot(OUT + "/clean-t22.png", settle=0.6), flush=True)
    h.type("memory", delay=0.12); h.press("\n", delay=0.1); h.wait(3.0)
    print("mem", h.screenshot(OUT + "/clean-memory.png", settle=0.8), flush=True)
    h.type("run sysinfo.app", delay=0.1); h.press("\n", delay=0.1); h.wait(5.0)
    print("sysinfo", h.screenshot(OUT + "/clean-sysinfo.png", settle=0.8), flush=True)
finally:
    print("rc", h.close())
