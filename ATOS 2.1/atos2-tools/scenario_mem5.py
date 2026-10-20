import sys, time, os
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
HB = "/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536"
IMG = P + "/work/atos-v2.img"
OUT = "/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/mem5"
os.makedirs(OUT, exist_ok=True)
N = []
for i in range(8): N += ["--acsi", "%d=none" % i]
E = ["--machine","megast","--cpulevel","3","--cpuclock","32","--memsize","4","--ttram","64",
     "--tf536","on","--graphics-card","vofa","--gfx-chip","et4000","--mono","--ide-master",IMG]+N
h = Hatari(P, log=P+"/work/hatari-mem5.log", window=True, image=IMG, binary=HB, cfg=True, extra=E)
def key(ch,d=0.15):
    h.keydown(ch, delay=d); time.sleep(0.04); h.keyup(ch, delay=d); time.sleep(0.05)
def line(t, tag, wait=5.0):
    for ch in t: key(ch)
    time.sleep(0.35); key("\r",0.2); time.sleep(wait)
    print(tag, h.screenshot(OUT+"/"+tag+".png", settle=0.7), flush=True)
try:
    time.sleep(30)
    line("memory", "01-mem-before", 4)
    line("path \\NOPE", "02-run-and-exit", 6)
    line("memory", "03-mem-after", 4)
    line("memory", "04-mem-after2", 4)
finally:
    print("rc", h.close(), flush=True)
