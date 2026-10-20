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

def run(tag, wait=5.0):
    h = Hatari(P, log=P + "/work/hatari-" + tag + ".log", window=True, image=IMG, binary=HB, cfg=True, extra=EXTRA)
    try:
        h.wait(wait)
        return h.screenshot(OUT + "/" + tag + ".png", settle=0.8)
    finally:
        h.close()
print("b1", run("fo1", 26.0), flush=True)
time.sleep(2)
print("b2", run("fo2", 20.0), flush=True)
