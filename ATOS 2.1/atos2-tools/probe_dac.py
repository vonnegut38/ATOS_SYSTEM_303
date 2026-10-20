import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
CONSOLES=0x00047762
TERM_CELLS=2000
h = Hatari(P, log=P + "/work/hatari-dac.log", window=False)
try:
    h.wait(24)
    raw=h.read_bytes(CONSOLES, TERM_CELLS*2*3)
    # cells are 16-bit; chars are the low byte
    chars=bytes(raw[i*2] for i in range(len(raw)//2))
    txt=chars.decode('latin-1')
    for key in ("DAC","15-BIT","8-BIT","24-BIT","CIC10487"):
        idx=txt.find(key)
        print("find %-8s -> %s" % (key, ("pos %d" % idx) if idx>=0 else "not found"), flush=True)
    i=txt.find("DAC")
    if i>=0: print("AROUND: %r" % txt[max(0,i-90):i+70], flush=True)
finally:
    print("rc", h.close(), flush=True)
