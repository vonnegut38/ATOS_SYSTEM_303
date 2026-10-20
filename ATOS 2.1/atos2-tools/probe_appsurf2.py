import sys, struct, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
h = Hatari(P, log=P + "/work/hatari-appsurf2.log", window=True)
def u32(a):
    b=h.read_bytes(a,4,settle=0.25); return struct.unpack(">I",b)[0] if b and len(b)==4 else None
try:
    h.wait(20)
    h.press("F10", delay=0.4); time.sleep(1.5)
    s0=u32(0x47c94); s1=u32(0x47c98)
    print("surf0", hex(s0) if s0 else None, "surf1", hex(s1) if s1 else None, flush=True)
    for name,s in (("s0",s0),("s1",s1)):
        if not (s and s>0x10000): continue
        d=h.read_bytes(s,32000,settle=1.0)
        m=sum(1 for y in range(301,317) for x in range(204,340) if d[y*80+(x>>3)]&(0x80>>(x&7)))
        print(name,"test-region lit pixels:",m, flush=True)
        print(name,"row18 bytes 0-18:", d[18*80:18*80+19].hex(), flush=True)
finally:
    print("rc", h.close(), flush=True)
