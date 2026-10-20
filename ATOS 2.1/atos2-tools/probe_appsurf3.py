import sys, struct, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
h = Hatari(P, log=P + "/work/hatari-appsurf3.log", window=True)
def u32(a):
    b=h.read_bytes(a,4,settle=0.25); return struct.unpack(">I",b)[0] if b and len(b)==4 else None
try:
    h.wait(20)
    h.press("F10", delay=0.4); time.sleep(1.5)
    s0=u32(0x47c94); d=h.read_bytes(s0,32000,settle=1.0)
    for y in range(16,52,2):
        print("y%02d"%y, d[y*80:y*80+20].hex(), flush=True)
finally:
    print("rc", h.close(), flush=True)
