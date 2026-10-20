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
def tap(h,k):
    h.keydown(k); time.sleep(0.28); h.keyup(k); time.sleep(0.2)
def cmd(h,t):
    h.type(t,delay=0.09); tap(h,"Return")
h = Hatari(P, log=P+"/work/hatari-tidy.log", window=True, image=IMG, binary=HB, cfg=True, extra=EXTRA)
try:
    time.sleep(28)
    cmd(h,"command"); time.sleep(5)
    print("commander", h.screenshot(OUT+"/tidy-command.png", settle=0.8), flush=True)
    tap(h,"F10"); time.sleep(2)
    cmd(h,"sysinfo"); time.sleep(3)
    print("sysinfo", h.screenshot(OUT+"/tidy-sysinfo.png", settle=0.8), flush=True)
finally:
    print("rc", h.close(), flush=True)
