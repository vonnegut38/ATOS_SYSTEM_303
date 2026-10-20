import sys, struct, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
WM=0x48426; STRIDE=58; CNT=WM+8*58
h = Hatari(P, log=P + "/work/hatari-wmtest5.log", window=True)
def rd(a,n): return h.read_bytes(a,n,settle=0.3)
def u32(a):
    b=rd(a,4); return struct.unpack(">I",b)[0] if b and len(b)==4 else None
def count(): return u32(CNT) or 0
def frame1():
    b=rd(WM+STRIDE,8)
    return struct.unpack(">hhHH",b) if b and len(b)==8 else None
def launch():
    h.press("F10", delay=0.5); time.sleep(1.2)
    h.press("Down", delay=0.4); time.sleep(0.8)
    h.press("Return", delay=0.5); time.sleep(1.2)
    h.type("WINDEMO", delay=0.3); time.sleep(1.2)
    h.press("Return", delay=0.5); time.sleep(5.0)
try:
    h.wait(22)
    for a in range(4):
        if count()>=2: break
        launch()
    print("start count",count(),"frame1",frame1(), flush=True)
    h.press("F1", delay=0.5); time.sleep(5.0)
    print("after drag count",count(),"frame1",frame1(), flush=True)
    h.press("F2", delay=0.5); time.sleep(5.0)
    print("after resize count",count(),"frame1",frame1(), flush=True)
    print("shot:", h.screenshot("/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/wmtest.png", settle=0.8), flush=True)
finally:
    print("rc", h.close(), flush=True)
