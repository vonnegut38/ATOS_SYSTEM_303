import sys, struct, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
WM=0x48426; STRIDE=58; CNT=WM+8*58
h = Hatari(P, log=P + "/work/hatari-wmtest4.log", window=True)
def rd(a,n): return h.read_bytes(a,n,settle=0.3)
def u32(a):
    b=rd(a,4); return struct.unpack(">I",b)[0] if b and len(b)==4 else None
def frames(tag):
    cnt=u32(CNT)
    out=[]
    for i in range(min(cnt or 0,8)):
        b=rd(WM+i*STRIDE,8)
        if b and len(b)==8:
            x,y,w,hh=struct.unpack(">hhHH",b); out.append((i,x,y,w,hh))
    print(tag,"count",cnt,out, flush=True)
try:
    h.wait(22)
    h.press("F10", delay=0.4); time.sleep(1.2)
    h.press("Down", delay=0.3); time.sleep(0.8)
    h.press("Return", delay=0.4); time.sleep(1.2)
    h.type("WINDEMO", delay=0.25); time.sleep(1.0)
    h.press("Return", delay=0.4); time.sleep(4.0)
    frames("before")
    h.press("F1", delay=0.4); time.sleep(4.0); frames("after-drag")
    h.press("F2", delay=0.4); time.sleep(4.0); frames("after-resize")
    print("shot:", h.screenshot("/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/wmtest.png", settle=0.8), flush=True)
finally:
    print("rc", h.close(), flush=True)
