import sys, os
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
HB = "/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536"
IMG = P + "/work/atos-v2.img"
OUT = "/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/tt536"
os.makedirs(OUT, exist_ok=True)
h = Hatari(P, log=P + "/work/hatari-tt536.log", window=True, image=IMG, binary=HB,
           extra=["--machine","megast","--cpulevel","3","--cpuclock","32","--memsize","4",
                  "--ttram","64","--tf536","on","--graphics-card","vofa","--gfx-chip","et4000","--mono"])
try:
    h.wait(22)
    print("boot", h.screenshot(OUT + "/a-boot.png", settle=0.8), flush=True)
    h.type("memory", delay=0.1); h.press("\n", delay=0.1); h.wait(2.5)
    print("memory", h.screenshot(OUT + "/b-memory.png", settle=0.8), flush=True)
    h.type("run sysinfo.app", delay=0.1); h.press("\n", delay=0.1); h.wait(5.0)
    print("sysinfo", h.screenshot(OUT + "/c-sysinfo.png", settle=0.8), flush=True)
finally:
    print("rc", h.close())
