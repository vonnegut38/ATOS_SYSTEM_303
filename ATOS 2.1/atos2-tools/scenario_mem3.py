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
h = Hatari(P, log=P+"/work/hatari-mem2.log", window=True, image=IMG, binary=HB, cfg=True, extra=EXTRA)
def dump(addr, n):
    t = h.read_mem(addr, n, settle=0.7)
    for line in t.splitlines():
        if re.match(r"^[0-9A-Fa-f]{8}:", line): print(line, flush=True)
try:
    time.sleep(40)
    print("=== kernel_exec_path_get @0x2395e (expect 2f03 2f02 222f 000c) ===", flush=True); dump(0x2395e, 16)
    print("=== system_exec_path @0x38f48 (expect backslash SYSTEM...) ===", flush=True); dump(0x38f48, 48)
    print("=== module_api+160 @0x3b1cc (expect 0002395e) ===", flush=True); dump(0x3b1cc, 8)
finally:
    print("rc", h.close(), flush=True)
