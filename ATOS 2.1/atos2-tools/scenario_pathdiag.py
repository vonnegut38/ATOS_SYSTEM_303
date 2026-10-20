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
h = Hatari(P, log=P+"/work/hatari-pathdiag.log", window=True, image=IMG, binary=HB, cfg=True, extra=E)
def key(ch,d=0.18):
    h.keydown(ch, delay=d); time.sleep(0.05); h.keyup(ch, delay=d); time.sleep(0.06)
def typ(t,d=0.18):
    for ch in t: key(ch,d)
def line(t, tag, wait=4.5):
    typ(t); time.sleep(0.5); key("\r",0.2); time.sleep(wait)
    print(tag, h.screenshot(OUT+"/pdiag-"+tag+".png", settle=0.8), flush=True)
try:
    time.sleep(30)
    line("path","1-a",5)
    line("path \\NOPE","2-b",5)
    line("path","3-c",5)
    line("run \\SYSTEM\\APP\\PATH.APP","4-d",5)
    line("path","5-e",5)
finally:
    print("rc", h.close(), flush=True)
