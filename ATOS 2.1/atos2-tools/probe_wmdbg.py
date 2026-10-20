import sys, struct, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
WM=0x48456; STRIDE=58
DBG={ 'press':0x483aa,'drag_start':0x483a6,'drag_move':0x483a2,'drag_end':0x4839e,'rsz':0x4839a }
h = Hatari(P, log=P + "/work/hatari-wmdbg.log", window=True)
def rd(a,n): return h.read_bytes(a,n,settle=0.3)
def u32(a):
    b=rd(a,4); return struct.unpack(">I",b)[0] if b and len(b)==4 else None
def cnt(): return u32(WM+8*58) or 0
def f1():
    b=rd(WM+STRIDE,8); return struct.unpack(">hhHH",b) if b and len(b)==8 else None
def dbg(): return {k:u32(v) for k,v in DBG.items()}
try:
    h.wait(22)
    for a in range(3):
        if cnt()>=2: break
        h.press("F10", delay=0.5); time.sleep(1.2)
        h.press("Down", delay=0.4); time.sleep(0.8)
        h.press("Return", delay=0.5); time.sleep(1.2)
        h.type("WINDEMO", delay=0.3); time.sleep(1.2)
        h.press("Return", delay=0.5); time.sleep(5.0)
    print("launched", cnt(), f1(), dbg(), flush=True)
    h.press("F1", delay=0.6); time.sleep(6.0)
    print("after F1", cnt(), f1(), dbg(), flush=True)
    h.press("F2", delay=0.6); time.sleep(6.0)
    print("after F2", cnt(), f1(), dbg(), flush=True)
finally:
    print("rc", h.close(), flush=True)
