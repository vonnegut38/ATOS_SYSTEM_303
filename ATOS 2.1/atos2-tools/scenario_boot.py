import sys, time, os
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
h = Hatari(P, log=P + "/work/hatari-boot.log", window=True, image=IMG, binary=HB, cfg=True, extra=EXTRA)
try:
    t0 = time.time()
    for n in range(26):
        target = 1.0 + n*0.4
        while time.time() - t0 < target: time.sleep(0.02)
        p = OUT + "/b%02d.png" % n
        h.screenshot(p, settle=0.12)
        sz = os.path.getsize(p) if os.path.exists(p) else 0
        print("%02d %.2f %6d" % (n, time.time()-t0, sz), flush=True)
finally:
    print("rc", h.close())
