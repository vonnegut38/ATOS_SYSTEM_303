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
h = Hatari(P, log=P+"/work/hatari-cmdr.log", window=True, image=IMG, binary=HB, cfg=True, extra=EXTRA)
try:
    time.sleep(28)
    print("shell", h.screenshot(OUT+"/cmdr0-shell.png", settle=0.8), flush=True)
    h.type("command")
    h.press("Return")
    time.sleep(4)
    print("cmdr1", h.screenshot(OUT+"/cmdr1-open.png", settle=0.8), flush=True)
    h.press("Down")
    time.sleep(1)
    print("cmdr2", h.screenshot(OUT+"/cmdr2-afterkey.png", settle=0.8), flush=True)
    h.press("Tab")
    time.sleep(1)
    print("cmdr3", h.screenshot(OUT+"/cmdr3-tab.png", settle=0.8), flush=True)
finally:
    print("rc", h.close(), flush=True)
