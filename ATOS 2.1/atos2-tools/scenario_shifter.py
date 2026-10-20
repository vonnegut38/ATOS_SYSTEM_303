import sys
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
HB = "/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536"
IMG = P + "/work/atos-v2.img"
OUT = "/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/boot"
NOACSI = []
for i in range(8): NOACSI += ["--acsi", "%d=none" % i]
EXTRA = ["--machine","megast","--cpulevel","3","--cpuclock","32","--memsize","4",
         "--ttram","64","--tf536","on","--graphics-card","none","--mono",
         "--ide-master", IMG] + NOACSI
h = Hatari(P, log=P + "/work/hatari-shifter.log", window=True, image=IMG, binary=HB, cfg=True, extra=EXTRA)
try:
    h.wait(20)
    print("s1", h.screenshot(OUT + "/s1-shifter.png", settle=0.8), flush=True)
    h.type("hw", delay=0.15); h.press("\n", delay=0.1); h.wait(2.0)
    print("s2", h.screenshot(OUT + "/s2-shifter-hw.png", settle=0.8), flush=True)
finally:
    print("rc", h.close())
