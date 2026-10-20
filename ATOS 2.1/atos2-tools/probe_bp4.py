import sys, struct, subprocess, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
h = Hatari(P, log=P + "/work/hatari-bp4.log", window=True)
CAP = str(P + "/work/hatari-bp4.log") + ".out"
def alltext():
    try:
        with open(CAP, "r", errors="replace") as f:
            return f.read()
    except Exception:
        return ""
def clean(s):
    return "\n".join(l for l in s.splitlines() if "Resource temporarily" not in l)
try:
    time.sleep(0.3)
    h.cmd("hatari-debug hi cpu 400")
    time.sleep(0.3)
    h.cmd("hatari-debug b pc=0x262f6")
    mark = len(alltext())
    for i in range(3):
        time.sleep(16 if i == 0 else 6)
        h.cmd("hatari-debug r")
        time.sleep(0.6)
        h.cmd("hatari-debug hi 70")
        time.sleep(1.5)
        print("=== hit %d ===" % i, flush=True)
        print(clean(alltext()[mark:])[-5000:], flush=True)
        mark = len(alltext())
        h.cmd("hatari-debug c")
        time.sleep(0.5)
finally:
    print("rc", h.close(), flush=True)
