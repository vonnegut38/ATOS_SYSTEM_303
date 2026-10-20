import sys, struct, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
A={'route_shell':0x3c2b8,'route_gui':0x3c2bc,'wake_keys':0x3c2c0,'wake_calls':0x3c2c4}
h = Hatari(P, log=P + "/work/hatari-kdbg.log", window=True)
def u32(a):
    b=h.read_bytes(a,4,settle=0.25); return struct.unpack(">I",b)[0] if b and len(b)==4 else None
def snap(tag): print(tag,{k:u32(v) for k,v in A.items()},flush=True)
try:
    h.wait(22)
    snap("boot")
    h.press("F10", delay=0.5); time.sleep(1.2)
    h.press("Down", delay=0.4); time.sleep(0.8)
    h.press("Return", delay=0.5); time.sleep(1.2)
    h.type("WINDEMO", delay=0.3); time.sleep(1.2)
    h.press("Return", delay=0.5); time.sleep(5.0)
    snap("after-launch")
    h.press("F3", delay=0.6); time.sleep(3.0)
    snap("after-F3")
finally:
    print("rc", h.close(), flush=True)
