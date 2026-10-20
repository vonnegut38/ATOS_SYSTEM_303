import sys, struct, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
A={'calls':0x3c3e0,'clear':0x3c3dc,'pid':0x3c3d8,'slot':0x3c3d4}
h = Hatari(P, log=P + "/work/hatari-rel.log", window=True)
def u32(a):
    b=h.read_bytes(a,4,settle=0.25); return struct.unpack(">I",b)[0] if b and len(b)==4 else None
def snap(tag): print(tag,{k:u32(v) for k,v in A.items()},flush=True)
try:
    h.wait(22)
    snap("boot")
    h.press("F10", delay=0.5); time.sleep(1.5)
    h.press("Down", delay=0.4); time.sleep(1.0)
    h.press("Return", delay=0.5); time.sleep(1.5)
    h.type("WINDEMO", delay=0.3); time.sleep(1.5)
    snap("typed")
    h.press("Return", delay=0.5); time.sleep(5.0)
    snap("spawned")
finally:
    print("rc", h.close(), flush=True)
