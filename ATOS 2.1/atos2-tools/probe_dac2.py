import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
CONSOLES=0x00047762
h = Hatari(P, log=P + "/work/hatari-dac2.log", window=False)
try:
    time.sleep(6)
    raw=h.read_bytes(CONSOLES, 2000*2*3)
    chars=bytes(raw[i*2] for i in range(len(raw)//2))
    txt=chars.decode('latin-1')
    for key in ("DAC","15-BIT","8-BIT","24-BIT","MONO / VIEW","GRAPHICS BACKEND"):
        i=txt.find(key)
        print("%-16s %s" % (key, ("pos %d" % i) if i>=0 else "not found"), flush=True)
    i=txt.find("DAC")
    if i>=0: print("AROUND: %r" % txt[max(0,i-120):i+60], flush=True)
finally:
    print("rc", h.close(), flush=True)
