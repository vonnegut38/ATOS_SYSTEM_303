import sys, time, re
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
HB = "/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536"
IMG = P + "/work/atos-v2.img"
N = []
for i in range(8): N += ["--acsi", "%d=none" % i]
E = ["--machine","megast","--cpulevel","3","--cpuclock","32","--memsize","4","--ttram","64",
     "--tf536","on","--graphics-card","vofa","--gfx-chip","et4000","--mono","--ide-master",IMG]+N
h = Hatari(P, log=P+"/work/hatari-pathmem3.log", window=True, image=IMG, binary=HB, cfg=True, extra=E)
def key(ch,d=0.18):
    h.keydown(ch, delay=d); time.sleep(0.05); h.keyup(ch, delay=d); time.sleep(0.06)
def typ(t,d=0.18):
    for ch in t: key(ch,d)
def dump(addr,n,label):
    t=h.read_mem(addr,n,settle=0.6); print(label,flush=True)
    for line in t.splitlines():
        if re.match(r"^[0-9A-Fa-f]{8}:",line): print(line,flush=True)
try:
    time.sleep(30)
    dump(0x3e2d4, 48, "=== before ===")
    typ("path \\NOPE"); key("\r",0.2); time.sleep(5)
    dump(0x3e2d4, 48, "=== after path NOPE ===")
    typ("run \\SYSTEM\\APP\\PATH.APP"); key("\r",0.2); time.sleep(5)
    dump(0x3e2d4, 48, "=== after explicit run ===")
finally:
    print("rc", h.close(), flush=True)
