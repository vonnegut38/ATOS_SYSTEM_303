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
h = Hatari(P, log=P+"/work/hatari-tosrun2.log", window=True, image=IMG, binary=HB, cfg=True, extra=E)
def key(ch,d):
    h.keydown(ch, delay=d); time.sleep(0.05); h.keyup(ch, delay=d); time.sleep(0.05)
try:
    time.sleep(30)
    for _ in range(3): key("\b",0.15)          # clear any stray
    for ch in "tos \\PROG1.PRG": key(ch,0.20)
    time.sleep(0.6)
    print("before", h.screenshot(OUT+"/tosrun2-before.png", settle=0.7), flush=True)
    key("\r",0.2); time.sleep(6)
    print("after", h.screenshot(OUT+"/tosrun2-after.png", settle=0.8), flush=True)
    time.sleep(4)
    print("after2", h.screenshot(OUT+"/tosrun2-after2.png", settle=0.8), flush=True)
finally:
    print("rc", h.close(), flush=True)
