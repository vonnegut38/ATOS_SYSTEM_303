import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
HB = "/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536"
OUT = "/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/boot"
EXTRA = ["--machine","megast","--cpulevel","3","--cpuclock","32","--memsize","4",
         "--ttram","64","--tf536","on","--graphics-card","vofa","--gfx-chip","et4000","--mono"]
h = Hatari(P, log=P+"/work/hatari-hddobs.log", window=True, image=P+"/work/atos-v2.img",
           binary=HB, cfg=True, extra=EXTRA)
try:
    for t in (20,40,70):
        time.sleep(t if t==20 else 20 if t==40 else 30)
        print(t, h.screenshot(OUT+"/hddobs-%d.png"%t, settle=0.8), flush=True)
finally:
    print("rc", h.close(), flush=True)
