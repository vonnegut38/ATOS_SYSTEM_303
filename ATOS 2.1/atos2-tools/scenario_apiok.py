import sys, time, re
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
h = Hatari(P, log=P+"/work/hatari-apiok.log", window=True, image=IMG, binary=HB, cfg=True, extra=EXTRA)
def words(addr, n):
    t = h.read_mem(addr, n, settle=0.7); out=[]
    for line in t.splitlines():
        m=re.match(r"^([0-9A-Fa-f]{8}):", line)
        if not m: continue
        toks=[x for x in line.split(":",1)[1][:48].split() if len(x)==2]
        out += [int(x,16) for x in toks]
    return out
try:
    time.sleep(30)
    w = words(0x3b12c, 192)
    print("module_api[0:8] =", [hex(x) for x in w[:8]], flush=True)
    print("module_api[40] (get_exec_path@160) =", hex(w[40]), flush=True)
    print("module_api+160 raw =", hex(w[40]), "expect kernel_exec_path_get=0x2395e", flush=True)
    print("system_exec_path @0x38cd4 =", bytes(w[0:0]) if False else "", flush=True)
    t = h.read_mem(0x38cd4, 48, settle=0.7)
    print(t, flush=True)
finally:
    print("rc", h.close(), flush=True)
