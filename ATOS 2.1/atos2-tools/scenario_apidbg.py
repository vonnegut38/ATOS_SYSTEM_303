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
h = Hatari(P, log=P+"/work/hatari-apidbg.log", window=True, image=IMG, binary=HB, cfg=True, extra=EXTRA)
def dump(addr, n):
    t = h.read_mem(addr, n, settle=0.7)
    out=[]
    for line in t.splitlines():
        m=re.match(r"^([0-9A-Fa-f]{8}):", line)
        if not m: continue
        toks=[x for x in line.split(":",1)[1][:48].split() if len(x)==2]
        out.append((int(m.group(1),16), bytes(int(x,16) for x in toks)))
    return out
try:
    time.sleep(30)
    print("=== module_api @0xbb12c (kernel base 0x80000 + 0x3b12c) ===", flush=True)
    for a,b in dump(0xbb12c, 200):
        print("%08x %s" % (a, b.hex()), flush=True)
    print("=== shell_api pointer @0xbb1cc (module_api+160) ===", flush=True)
    for a,b in dump(0xbb1cc, 8):
        print("%08x %s" % (a, b.hex()), flush=True)
    h.type("path", delay=0.2); h.press("\r", delay=0.5); time.sleep(4)
    print("panic-shot", h.screenshot(OUT+"/apidbg-panic.png", settle=0.8), flush=True)
finally:
    print("rc", h.close(), flush=True)
