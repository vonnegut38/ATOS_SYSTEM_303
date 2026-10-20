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
h = Hatari(P, log=P+"/work/hatari-hddpath2.log", window=True, image=IMG, binary=HB, cfg=True, extra=EXTRA)
try:
    time.sleep(30)
    h.type("path", delay=0.14); h.press("\r", delay=0.5); time.sleep(3)
    print("path", h.screenshot(OUT+"/hddpath2-path.png", settle=0.8), flush=True)
    h.type("command", delay=0.14); h.press("\r", delay=0.5); time.sleep(12)
    print("cmd", h.screenshot(OUT+"/hddpath2-command.png", settle=0.8), flush=True)
finally:
    print("rc", h.close(), flush=True)
