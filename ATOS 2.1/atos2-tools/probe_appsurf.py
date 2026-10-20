import sys, struct, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
h = Hatari(P, log=P + "/work/hatari-appsurf.log", window=True)
def u32(a):
    b=h.read_bytes(a,4,settle=0.25); return struct.unpack(">I",b)[0] if b and len(b)==4 else None
try:
    h.wait(20)
    h.press("F10", delay=0.4); time.sleep(1.2)
    s0=u32(0x47c94); print("app_surface[0]", hex(s0) if s0 else None, flush=True)
    if s0 and s0>0x10000:
        d=h.read_bytes(s0,32000,settle=1.0)
        for y in (18,20,22,26):
            row=d[y*80:(y+1)*80]
            print("y%d nz"%y, sum(1 for b in row if b), "b0-19", row[0:20].hex(), flush=True)
finally:
    print("rc", h.close(), flush=True)
