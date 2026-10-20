import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
A_RSZW=0x4ba22; A_RSZH=0x4ba1e; A_START=0x4ba82
h = Hatari(P, log=P + "/work/hatari-gt2.log", window=True)
def rd(a): return int.from_bytes(h.read_bytes(a,4),"big")
try:
    h.wait(24)
    print("start0", rd(A_START), "w0", rd(A_RSZW), "h0", rd(A_RSZH), flush=True)
    h.press("F7", delay=0.8); time.sleep(2.5)
    h.press("F2", delay=0.8); time.sleep(3.0)
    print("start1", rd(A_START), "w1", rd(A_RSZW), "h1", rd(A_RSZH), flush=True)
    h.cmd("hatari-debug screenshot /tmp/gt2-2.png"); time.sleep(2.0)
finally:
    print("rc", h.close(), flush=True)
