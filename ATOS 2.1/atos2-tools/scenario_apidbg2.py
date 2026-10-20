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
h = Hatari(P, log=P+"/work/hatari-apidbg2.log", window=True, image=IMG, binary=HB, cfg=True, extra=EXTRA)
def dump(addr, n):
    t = h.read_mem(addr, n, settle=0.7)
    out=[]
    for line in t.splitlines():
        m=re.match(r"^([0-9A-Fa-f]{8}):", line)
        if not m: continue
        toks=[x for x in line.split(":",1)[1][:48].split() if len(x)==2]
        out.append((int(m.group(1),16), bytes(int(x,16) for x in toks)))
    return out
# runtime = ELF vaddr - 0x20000 + 0x80000
def rt(v): return v - 0x20000 + 0x80000
try:
    time.sleep(30)
    print("kernel_entry rt=%08x" % rt(0x20020), flush=True)
    print("=== module_api rt=%08x ===" % rt(0x3b12c), flush=True)
    for a,b in dump(rt(0x3b12c), 192):
        print("%08x %s" % (a, b.hex()), flush=True)
    print("=== module_api+160 rt=%08x ===" % rt(0x3b1cc), flush=True)
    for a,b in dump(rt(0x3b1cc), 8):
        print("%08x %s" % (a, b.hex()), flush=True)
    print("=== system_exec_path rt=%08x ===" % rt(0x38cd4), flush=True)
    for a,b in dump(rt(0x38cd4), 64):
        print("%08x %s  %r" % (a, b.hex(), b), flush=True)
finally:
    print("rc", h.close(), flush=True)
