import sys, subprocess, time, os
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
log = P + "/work/hatari-tr.log"
h = Hatari(P, log=log, window=False, extra=["--cpulevel", "0", "--trace", "cpu_exception", "--log-level", "trace"])
try:
    time.sleep(26)
finally:
    h.close()
p = log + ".out"
txt = open(p, "r", errors="replace").read()
lines = [l for l in txt.splitlines() if "Resource temporarily" not in l]
print("total lines", len(lines), flush=True)
hits = [l for l in lines if "xception" in l or "rror" in l or "BUs" in l or "ADDR" in l]
print("exception-ish lines", len(hits), flush=True)
for l in hits[-40:]:
    print(l, flush=True)
