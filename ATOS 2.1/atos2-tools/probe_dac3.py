import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
CONSOLES=0x00047762
h = Hatari(P, log=P + "/work/hatari-dac3.log", window=False)
def scan(tag):
    raw=h.read_bytes(CONSOLES, 2000*2*3)
    txt=bytes(raw[i*2] for i in range(len(raw)//2)).decode('latin-1')
    res={k:txt.find(k) for k in ("DAC","MEMORY MANAGER","PROCESS MANAGER","GRAPHICS BACKEND","SHELL READY","VIDEO:")}
    print(tag, {k:v for k,v in res.items()}, flush=True)
    if res["DAC"]>=0:
        i=res["DAC"]; print("   AROUND:", repr(txt[max(0,i-60):i+50]), flush=True)
try:
    for t in (8,12,16,20):
        time.sleep(t if t==8 else 4)
        scan("t=%ds"%t)
finally:
    print("rc", h.close(), flush=True)
