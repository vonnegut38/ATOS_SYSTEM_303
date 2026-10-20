import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
log = P + "/work/hatari-tr2.log"
h = Hatari(P, log=log, window=False, extra=["--cpulevel", "0", "--trace", "cpu_exception", "--log-level", "debug"])
try:
    time.sleep(26)
finally:
    h.close()
txt = open(log + ".out", "r", errors="replace").read()
lines = [l for l in txt.splitlines() if "Resource temporarily" not in l]
print("total lines", len(lines), flush=True)
for l in lines[-45:]:
    print(l, flush=True)
