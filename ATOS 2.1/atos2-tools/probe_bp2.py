import sys, struct, subprocess, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
h = Hatari(P, log=P + "/work/hatari-bp2.log", window=True)
CAP = str(P + "/work/hatari-bp2.log") + ".out"
def alltext():
    try:
        with open(CAP, "r", errors="replace") as f:
            return f.read()
    except Exception as e:
        return "ERR %s" % e
def since(mark):
    t = alltext()
    return t[mark:]
try:
    time.sleep(0.4)
    h.cmd("hatari-debug b pc=0x201d4")
    mark = len(alltext())
    for i in range(4):
        time.sleep(8 if i == 0 else 4)
        h.cmd("hatari-debug r")
        time.sleep(0.6)
        h.cmd("hatari-debug history")
        time.sleep(1.2)
        print("=== hit %d ===" % i, flush=True)
        chunk = since(mark)
        print(chunk[-3000:], flush=True)
        mark = len(alltext())
        h.cmd("hatari-debug c")
        time.sleep(0.6)
finally:
    print("rc", h.close(), flush=True)
