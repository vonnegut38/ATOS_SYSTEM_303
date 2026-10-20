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
h = Hatari(P, log=P+"/work/hatari-tosrun.log", window=True, image=IMG, binary=HB, cfg=True, extra=E)
def typ(text):
    for ch in text:
        h.keydown(ch, delay=0.13); h.keyup(ch, delay=0.13)
try:
    time.sleep(30)
    typ("tos \\PROG1.PRG"); time.sleep(0.6)
    print("before", h.screenshot(OUT+"/tosrun-before.png", settle=0.7), flush=True)
    h.keydown("\r", delay=0.15); h.keyup("\r", delay=0.4)
    time.sleep(6)
    print("after", h.screenshot(OUT+"/tosrun-after.png", settle=0.8), flush=True)
    time.sleep(4)
    print("after2", h.screenshot(OUT+"/tosrun-after2.png", settle=0.8), flush=True)
finally:
    print("rc", h.close(), flush=True)
