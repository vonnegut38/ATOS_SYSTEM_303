import sys, os
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
IMG = P + "/work/atos-v2-tt.img"
h = Hatari(P, log=P + "/work/hatari-ttmap.log", window=True, image=IMG,
           extra=["--machine","tt","--cpulevel","3","--ttram","64"])
try:
    h.wait(8)
    for addr in [0x01000000, 0x01020000, 0x04000000, 0x04100000]:
        raw = h.read_mem(addr, 16, settle=0.5)
        lines = [l for l in raw.splitlines() if l.strip().startswith(("%08X" % addr), 0) or ":" in l][:2]
        print("addr 0x%08X ->" % addr, repr(raw[-200:]), flush=True)
finally:
    print("rc", h.close())
