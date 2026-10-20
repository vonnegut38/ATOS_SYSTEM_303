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
         "--ttram","64","--tf536","on","--graphics-card","none","--mono",
         "--ide-master", IMG] + NOACSI
h = Hatari(P, log=P + "/work/hatari-novofa2.log", window=True, image=IMG, binary=HB, cfg=True, extra=EXTRA)
try:
    h.wait(12)
    print("t12", h.screenshot(OUT + "/nv2-t12.png", settle=0.6), flush=True)
    h.type("memory", delay=0.12); h.press("\n", delay=0.1); h.wait(3.0)
    print("mem", h.screenshot(OUT + "/nv2-memory.png", settle=0.8), flush=True)
    h.type("run sysinfo.app", delay=0.1); h.press("\n", delay=0.1); h.wait(5.0)
    print("sysinfo", h.screenshot(OUT + "/nv2-sysinfo.png", settle=0.8), flush=True)
finally:
    print("rc", h.close())
