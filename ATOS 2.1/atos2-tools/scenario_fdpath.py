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
h = Hatari(P, log=P+"/work/hatari-fdpath.log", window=True, image=P+"/work/atos-v2.img",
           binary=HB, cfg=True, extra=EXTRA)
def typeline(text):
    for ch in text:
        h.keydown(ch, delay=0.13); h.keyup(ch, delay=0.13)
try:
    time.sleep(210)
    typeline("command"); time.sleep(0.6)
    print("before", h.screenshot(OUT+"/fdpath-before.png", settle=0.8), flush=True)
    h.keydown("\r", delay=0.15); h.keyup("\r", delay=0.4)
    time.sleep(25)
    print("after", h.screenshot(OUT+"/fdpath-after.png", settle=0.8), flush=True)
finally:
    print("rc", h.close(), flush=True)
