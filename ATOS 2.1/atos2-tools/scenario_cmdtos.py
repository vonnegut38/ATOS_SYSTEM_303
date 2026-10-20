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
h = Hatari(P, log=P+"/work/hatari-cmdtos.log", window=True, image=IMG, binary=HB, cfg=True, extra=E)
def key(ch,d=0.16):
    h.keydown(ch, delay=d); time.sleep(0.05); h.keyup(ch, delay=d); time.sleep(0.06)
def typ(text,d=0.16):
    for ch in text: key(ch,d)
try:
    time.sleep(30)
    typ("command"); key("\r",0.2); time.sleep(10)
    print("cmdr", h.screenshot(OUT+"/cmdtos-open.png", settle=0.7), flush=True)
    for _ in range(9): key("Down",0.18)
    time.sleep(0.5)
    print("sel", h.screenshot(OUT+"/cmdtos-sel.png", settle=0.7), flush=True)
    key("\r",0.2); time.sleep(8)
    print("run", h.screenshot(OUT+"/cmdtos-run.png", settle=0.8), flush=True)
    time.sleep(4)
    print("run2", h.screenshot(OUT+"/cmdtos-run2.png", settle=0.8), flush=True)
finally:
    print("rc", h.close(), flush=True)
