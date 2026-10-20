import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
h = Hatari(P, log=P + "/work/hatari-term3.log", window=True)
try:
    h.wait(20)
    d = h.read_bytes(0x435bc, 400, settle=0.6)   # consoles[0].cells, 200 cells
    txt = "".join(chr(d[i*2+1]) if 32 <= d[i*2+1] < 127 else "." for i in range(len(d)//2))
    print("cells:", txt[:80], flush=True)
    print("nonzero cells:", sum(1 for i in range(len(d)//2) if d[i*2+1]), flush=True)
finally:
    print("rc", h.close(), flush=True)
