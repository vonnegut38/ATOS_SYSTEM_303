import sys, struct, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
KB=0x3c360; PS=0x3c344; DESK=0x3c330
h = Hatari(P, log=P + "/work/hatari-guiowner.log", window=True)
def rd(a,n): return h.read_bytes(a,n,settle=0.3)
def u32(a):
    b=rd(a,4); return struct.unpack(">I",b)[0] if b and len(b)==4 else None
def snap(tag):
    print(tag,"kb",[u32(KB+i*4) for i in range(3)],"persist",rd(PS,3).hex(),"desk",[u32(DESK+i*4) for i in range(3)],flush=True)
try:
    h.wait(22)
    snap("boot")
    h.press("F10", delay=0.5); time.sleep(1.2)
    h.press("Down", delay=0.4); time.sleep(0.8)
    h.press("Return", delay=0.5); time.sleep(1.2)
    h.type("WINDEMO", delay=0.3); time.sleep(1.2)
    h.press("Return", delay=0.5); time.sleep(5.0)
    snap("after")
finally:
    print("rc", h.close(), flush=True)
