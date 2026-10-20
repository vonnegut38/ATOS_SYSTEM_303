import sys, os
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
HB = "/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536"
IMG = P + "/work/atos-v2.img"
OUT = "/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/tt536"
extra = ["--machine","megast","--cpulevel","3","--cpuclock","32","--memsize","4",
         "--ttram","64","--tf536","on","--graphics-card","vofa","--gfx-chip","et4000","--mono",
         "--acsi","0=none","--acsi","1=none","--acsi","2=none","--acsi","3=none",
         "--acsi","4=none","--acsi","5=none","--acsi","6=none","--acsi","7=none",
         "--ide-master", IMG]
h = Hatari(P, log=P + "/work/hatari-tt536d.log", window=True, image=IMG, binary=HB, extra=extra)
try:
    for i, w in enumerate([25,10,10,10]):
        h.wait(w)
        print("shot", i, h.screenshot(OUT + "/i-boot-%d.png" % i, settle=0.8), flush=True)
    h.type("memory", delay=0.1); h.press("\n", delay=0.1); h.wait(3.0)
    print("memory", h.screenshot(OUT + "/j-memory.png", settle=0.8), flush=True)
finally:
    print("rc", h.close())
