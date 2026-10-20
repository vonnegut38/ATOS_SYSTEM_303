import sys, struct, subprocess, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
h = Hatari(P, log=P + "/work/hatari-bp5.log", window=True)
CAP = str(P + "/work/hatari-bp5.log") + ".out"
def alltext():
    try:
        with open(CAP, "r", errors="replace") as f:
            return f.read()
    except Exception:
        return ""
def clean(s):
    return "\n".join(l for l in s.splitlines() if "Resource temporarily" not in l)
def u32(a):
    b = h.read_bytes(a, 4, settle=0.6)
    return struct.unpack(">I", b)[0] if b and len(b) == 4 else None
try:
    time.sleep(0.3)
    print("cpu_type =", h.read_bytes(0x3c962, 1, settle=0.6), flush=True)
    h.cmd("hatari-debug b pc=0x201d4")
    mark = len(alltext())
    for i in range(3):
        time.sleep(14 if i == 0 else 4)
        h.cmd("hatari-debug r")
        time.sleep(0.8)
        h.cmd("hatari-debug m a7 36")
        time.sleep(1.5)
        print("=== bus hit %d ===" % i, flush=True)
        print(clean(alltext()[mark:])[-2500:], flush=True)
        mark = len(alltext())
        h.cmd("hatari-debug c")
        time.sleep(0.5)
finally:
    print("rc", h.close(), flush=True)
