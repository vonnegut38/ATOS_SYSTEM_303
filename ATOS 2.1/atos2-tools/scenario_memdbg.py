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
h = Hatari(P, log=P+"/work/hatari-mem.log", window=True, image=IMG, binary=HB, cfg=True, extra=EXTRA)
def dump(addr, n):
    t = h.read_mem(addr, n, settle=0.7)
    for line in t.splitlines():
        m=re.match(r"^([0-9A-Fa-f]{8}):", line)
        if not m: continue
        print(line, flush=True)
try:
    time.sleep(30)
    print("=== 0x80000 (kernel image) ===", flush=True); dump(0x80000, 64)
    print("=== 0x98cd4 (system_exec_path?) ===", flush=True); dump(0x98cd4, 48)
    print("=== 0x9b12c (module_api?) ===", flush=True); dump(0x9b12c, 32)
finally:
    print("rc", h.close(), flush=True)
