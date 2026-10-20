import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
HB = "/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536"
IMG = P + "/work/atos-v2.img"
OUT = "/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/boot"
N = []
for i in range(8): N += ["--acsi", "%d=none" % i]
E = ["--machine","megast","--cpulevel","3","--cpuclock","32","--memsize","4","--ttram","64",
     "--tf536","on","--graphics-card","vofa","--gfx-chip","et4000","--mono","--ide-master",IMG]+N
h = Hatari(P, log=P+"/work/hatari-colon.log", window=True, image=IMG, binary=HB, cfg=True, extra=E)
def key(ch,d=0.17):
    h.keydown(ch, delay=d); time.sleep(0.05); h.keyup(ch, delay=d); time.sleep(0.06)
def colon(d=0.15):
    h.cmd("hatari-event keydown 42"); time.sleep(d)
    h.cmd("hatari-event keydown 39"); time.sleep(d)
    h.cmd("hatari-event keyup 39"); time.sleep(d)
    h.cmd("hatari-event keyup 42"); time.sleep(d)
def typ_mixed(t):
    for ch in t:
        if ch==':': colon()
        else: key(ch)
try:
    time.sleep(30)
    typ_mixed("dir C:\\SYSTEM"); time.sleep(0.5); key("\r",0.2); time.sleep(4)
    print("shiftcolon", h.screenshot(OUT+"/colon-1.png", settle=0.8), flush=True)
    typ_mixed("cd C:\\SYSTEM"); time.sleep(0.5); key("\r",0.2); time.sleep(3)
    key("p"); key("w"); key("d"); key("\r",0.2); time.sleep(3)
    print("shiftcolon2", h.screenshot(OUT+"/colon-2.png", settle=0.8), flush=True)
finally:
    print("rc", h.close(), flush=True)
