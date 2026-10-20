import sys, struct, subprocess, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
h = Hatari(P, log=P + "/work/hatari-bp.log", window=True)
def tail(n=40):
    try:
        with open(str(P + "/work/hatari-bp.log") + ".out", "r", errors="replace") as f:
            return "".join(f.readlines()[-n:])
    except Exception as e:
        return "ERR %s" % e
try:
    time.sleep(0.5)
    h.cmd("hatari-debug b pc=0x201d4")
    h.cmd("hatari-debug b pc=0x2025a")
    time.sleep(18)
    print("=== after 18s ===", flush=True)
    print(tail(60), flush=True)
    h.cmd("hatari-debug r")
    time.sleep(1.0)
    h.cmd("hatari-debug d")
    time.sleep(1.0)
    print("=== regs/disasm ===", flush=True)
    print(tail(80), flush=True)
    h.cmd("hatari-debug c")
    time.sleep(3)
    print("=== after continue ===", flush=True)
    print(tail(30), flush=True)
finally:
    print("rc", h.close(), flush=True)
