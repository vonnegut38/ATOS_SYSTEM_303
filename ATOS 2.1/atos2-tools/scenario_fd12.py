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
h = Hatari(P, log=P+"/work/hatari-fat12boot.log", window=True, image=P+"/work/atos-v2.img",
           binary=HB, cfg=True, extra=EXTRA)
def typeline(text):
    for ch in text:
        h.keydown(ch, delay=0.14); h.keyup(ch, delay=0.14)
try:
    time.sleep(205)
    print("shell", h.screenshot(OUT+"/fat12-shell.png", settle=0.8), flush=True)
    typeline("dir"); time.sleep(0.6)
    h.keydown("\r", delay=0.15); h.keyup("\r", delay=0.4); time.sleep(4)
    print("dir", h.screenshot(OUT+"/fat12-dir.png", settle=0.8), flush=True)
    typeline("fdd"); time.sleep(0.6)
    h.keydown("\r", delay=0.15); h.keyup("\r", delay=0.4); time.sleep(4)
    print("fdd", h.screenshot(OUT+"/fat12-fdd.png", settle=0.8), flush=True)
finally:
    print("rc", h.close(), flush=True)
