import sys, struct, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
h = Hatari(P, log=P + "/work/hatari-final.log", window=True)
def u8(a):
    b = h.read_bytes(a,1,settle=0.25); return b[0] if b else None
try:
    h.wait(18)
    for attempt in range(4):
        h.type("start desktop.app", delay=0.12); h.press("\n", delay=0.1); h.wait(6.0)
        if u8(0x3cfcd)==1: break
    time.sleep(2.0)
    print("active", u8(0x3cfcd), "attempt", attempt, flush=True)
    print("shot:", h.screenshot("/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/desktop-clock.png", settle=0.6), flush=True)
finally:
    print("rc", h.close(), flush=True)
