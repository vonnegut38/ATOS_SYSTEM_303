import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
HB = "/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536"
IMG = P + "/work/atos-v2.img"
OUT = "/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/boot"
N = []
for i in range(8): N += ["--acsi", "%d=none" % i]
E = ["--machine","megast","--cpulevel","3","--cpuclock","32","--memsize","4","--ttram","64",
     "--tf536","on","--graphics-card","vofa","--gfx-chip","et4000","--mono","--ide-master",IMG]+N
h = Hatari(P, log=P+"/work/hatari-altmid.log", window=True, image=IMG, binary=HB, cfg=True, extra=E)
try:
    h.wait(6.0)
    h.press("Alt", delay=0.2)
    h.wait(14)
    print("force_shifter:", h.read_u32(0x00039045), flush=True)
    print("nova_init    :", h.read_u32(0x00039044), flush=True)
    print("mid", h.screenshot(OUT + "/altmid.png", settle=0.8), flush=True)
finally:
    print("rc", h.close())
