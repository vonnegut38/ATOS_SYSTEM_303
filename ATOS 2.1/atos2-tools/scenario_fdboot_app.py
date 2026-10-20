import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
HB = "/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536"
FLOP = "/tmp/atos-boot.st"
OUT = "/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/boot"
NOACSI = []
for i in range(8): NOACSI += ["--acsi", "%d=none" % i]
EXTRA = ["--machine","megast","--cpulevel","3","--cpuclock","32","--memsize","4",
         "--ttram","64","--tf536","on","--graphics-card","vofa","--gfx-chip","et4000","--mono",
         "--disk-a", FLOP] + NOACSI
h = Hatari(P, log=P+"/work/hatari-fdapp.log", window=True, image=P+"/work/atos-v2.img",
           binary=HB, cfg=True, extra=EXTRA)
try:
    time.sleep(215)
    print("shell", h.screenshot(OUT+"/fdapp-shell.png", settle=0.8), flush=True)
    h.type("dir", delay=0.14)
    h.press("\r", delay=0.5)
    time.sleep(3)
    print("dir", h.screenshot(OUT+"/fdapp-dir.png", settle=0.8), flush=True)
    h.type("command", delay=0.14)
    h.press("\r", delay=0.5)
    time.sleep(8)
    print("cmd", h.screenshot(OUT+"/fdapp-command.png", settle=0.8), flush=True)
finally:
    print("rc", h.close(), flush=True)
