import sys, struct, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
h = Hatari(P, log=P + "/work/hatari-title.log", window=True)
WM=0x478e8
def u32(a):
    b=h.read_bytes(a,4,settle=0.25); return struct.unpack(">I",b)[0] if b and len(b)==4 else None
try:
    h.wait(20)
    h.press("F10", delay=0.3); time.sleep(0.8)
    h.press("Down", delay=0.2); time.sleep(0.4)
    h.press("Return", delay=0.3); time.sleep(0.8)
    h.type("WINDEMO", delay=0.15); time.sleep(0.4)
    h.press("Return", delay=0.3); time.sleep(5.0)
    cnt=u32(WM+480); print("wm.count", cnt, flush=True)
    for i in range(min(cnt or 0,8)):
        base=WM+i*60
        flags=h.read_bytes(base+20,1,settle=0.2)
        t=h.read_bytes(base+26,24,settle=0.2)
        title="".join(chr(c) if 32<=c<127 else "" for c in t).split(chr(0))[0]
        print("win%d flags=%r title=%r"%(i, flags[0] if flags else None, title), flush=True)
finally:
    print("rc", h.close(), flush=True)
