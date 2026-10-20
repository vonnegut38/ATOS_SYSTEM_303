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
h = Hatari(P, log=P+"/work/hatari-fddiag.log", window=True, image=P+"/work/atos-v2.img",
           binary=HB, cfg=True, extra=EXTRA)
def cmd(line, tag, wait):
    h.type(line, delay=0.14); h.press("\r", delay=0.4); time.sleep(wait)
    print(tag, h.screenshot(OUT+"/fddiag-"+tag+".png", settle=0.8), flush=True)
try:
    time.sleep(215)
    cmd("dir \\SYSTEM", "1-system", 3)
    cmd("dir \\SYSTEM\\APP", "2-app", 8)
    cmd("run \\SYSTEM\\APP\\HW.APP", "3-hw", 8)
    cmd("run \\SYSTEM\\APP\\COMMAND.APP", "4-cmd", 10)
finally:
    print("rc", h.close(), flush=True)
