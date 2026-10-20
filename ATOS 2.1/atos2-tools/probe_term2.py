import sys, struct, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
h = Hatari(P, log=P + "/work/hatari-term2.log", window=True)
def u32(a):
    b=h.read_bytes(a,4,settle=0.25); return struct.unpack(">I",b)[0] if b and len(b)==4 else None
def u8(a):
    b=h.read_bytes(a,1,settle=0.25); return b[0] if b else None
try:
    h.wait(20)
    print("before: reg",u8(0x4769a),"hidden",u8(0x47686),"focused",u8(0x3ad35), flush=True)
    h.press("F10", delay=0.3); time.sleep(0.8)
    h.press("Return", delay=0.3); time.sleep(1.5)
    print("after: reg",u8(0x4769a),"hidden",u8(0x47686),"focused",u8(0x3ad35), flush=True)
    sp=u32(0x47648); print("console_surface", hex(sp) if sp else None, flush=True)
    if sp and sp>0x10000:
        d=h.read_bytes(sp,32000,settle=1.0)
        print("surface nonzero", sum(1 for x in d if x), "head", d[:32].hex(), flush=True)
finally:
    print("rc", h.close(), flush=True)
