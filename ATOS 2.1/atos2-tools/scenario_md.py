import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
HB = "/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536"
IMG = P + "/work/atos-v2.img"
N = []
for i in range(8): N += ["--acsi", "%d=none" % i]
E = ["--machine","megast","--cpulevel","3","--cpuclock","32","--memsize","4","--ttram","64",
     "--tf536","on","--graphics-card","vofa","--gfx-chip","et4000","--mono","--ide-master",IMG]+N
h = Hatari(P, log=P+"/work/hatari-md.log", window=True, image=IMG, binary=HB, cfg=True, extra=E)
try:
    time.sleep(30)
    h.type("memory"); h.press("\r", 0.2); time.sleep(3)
    for spec in [("46d58", "10"), (hex(0x46d58), "10"), (str(0x46d58), "16")]:
        raw = h.read_mem(spec[0], spec[1], settle=1.0)
        print("=== memdump %r %r -> %d chars ===" % (spec[0], spec[1], len(raw)), flush=True)
        print(repr(raw[:600]), flush=True)
finally:
    print("rc", h.close(), flush=True)
