import sys, os
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
HB = "/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536"
IMG = P + "/work/atos-v2.img"
NOACSI = []
for i in range(8): NOACSI += ["--acsi", "%d=none" % i]
EXTRA = ["--machine","megast","--cpulevel","3","--cpuclock","32","--memsize","4",
         "--ttram","64","--tf536","on","--graphics-card","vofa","--gfx-chip","et4000","--mono",
         "--ide-master", IMG] + NOACSI
h = Hatari(P, log=P + "/work/hatari-dbg2.log", window=True, image=IMG, binary=HB, cfg=True, extra=EXTRA)
try:
    h.wait(10)
    syms = {"card":0x438ea, "front":0x438ee, "shadow":0x438e6, "active":0x435c0, "calls":0x435bc, "copies":0x435b8}
    for n,a in syms.items():
        print(n, hex(h.read_u32(a) or 0), flush=True)
finally:
    print("rc", h.close())
