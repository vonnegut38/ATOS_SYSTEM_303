import sys, time, re
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
HB = "/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536"
IMG = P + "/work/atos-v2.img"
NOACSI = []
for i in range(8): NOACSI += ["--acsi", "%d=none" % i]
EXTRA = ["--machine","megast","--cpulevel","3","--cpuclock","32","--memsize","4",
         "--ttram","64","--tf536","on","--graphics-card","vofa","--gfx-chip","et4000","--mono",
         "--ide-master", IMG] + NOACSI
h = Hatari(P, log=P+"/work/hatari-api160.log", window=True, image=IMG, binary=HB, cfg=True, extra=EXTRA)
def words(addr, n):
    t = h.read_mem(addr, n, settle=0.7); out=[]
    for line in t.splitlines():
        m=re.match(r"^([0-9A-Fa-f]{8}):", line)
        if not m: continue
        toks=[x for x in line.split(":",1)[1][:48].split() if len(x)==2]
        out += [int(x,16) for x in toks]
    return out
def be32(w,o): return (w[o]<<24)|(w[o+1]<<16)|(w[o+2]<<8)|w[o+3]
try:
    time.sleep(40)
    w = words(0x3b12c, 192)
    print("len", len(w), flush=True)
    for off in (16,20,24,28,32,36,40,96,160,164,168,172,176,180,184):
        print("api+%3d = 0x%08x" % (off, be32(w,off)), flush=True)
finally:
    print("rc", h.close(), flush=True)
