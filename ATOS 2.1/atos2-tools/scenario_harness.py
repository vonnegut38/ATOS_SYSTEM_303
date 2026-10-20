import sys, time, os
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
HB = "/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536"
IMG = P + "/work/atos-v2.img"
N = []
for i in range(8): N += ["--acsi", "%d=none" % i]
E = ["--machine","megast","--cpulevel","3","--cpuclock","32","--memsize","4","--ttram","64",
     "--tf536","on","--graphics-card","vofa","--gfx-chip","et4000","--mono","--ide-master",IMG]+N
h = Hatari(P, log=P+"/work/hatari-harness.log", window=True, image=IMG, binary=HB, cfg=True, extra=E)
try:
    time.sleep(30)
    print("console_addr:", hex(h.console_addr() or 0), flush=True)
    for cmd in ["memory", "help", "path"]:
        ok = h.type_checked(cmd)
        h.press("\r", 0.2); time.sleep(3.0)
        lines = h.console_lines() or []
        print("=== typed %r  verified=%s ===" % (cmd, ok), flush=True)
        for ln in lines[-6:]:
            print("   |" + ln, flush=True)
finally:
    print("rc", h.close(), flush=True)
