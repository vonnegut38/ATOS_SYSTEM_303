import sys, time, os
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
HB = "/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536"
IMG = P + "/work/atos-v2.img"
OUT = "/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/bootclear"
os.makedirs(OUT, exist_ok=True)
N = []
for i in range(8): N += ["--acsi", "%d=none" % i]
E = ["--machine","megast","--cpulevel","3","--cpuclock","32","--memsize","4","--ttram","64",
     "--tf536","on","--graphics-card","vofa","--gfx-chip","et4000","--mono","--ide-master",IMG]+N
h = Hatari(P, log=P+"/work/hatari-bootclear.log", window=True, image=IMG, binary=HB, cfg=True, extra=E)
try:
    time.sleep(8)
    print("post", h.screenshot(OUT+"/01-post.png", settle=0.8), flush=True)
    time.sleep(30)
    print("boot", h.screenshot(OUT+"/02-after-boot.png", settle=0.8), flush=True)
finally:
    print("rc", h.close(), flush=True)
