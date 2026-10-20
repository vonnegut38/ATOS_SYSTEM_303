import sys, os
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
IMG = P + "/work/atos-v2-tt.img"
OUT = "/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/ttram"
os.makedirs(OUT, exist_ok=True)
h = Hatari(P, log=P + "/work/hatari-ttmem.log", window=True, image=IMG,
           extra=["--machine", "ste", "--ttram", "64", "--cpuclock", "16"])
try:
    h.wait(18)
    h.type("memory", delay=0.1); h.press("\n", delay=0.1); h.wait(2.0)
    print("memory", h.screenshot(OUT + "/ste-tt-memory.png", settle=0.8), flush=True)
    h.type("run sysinfo.app", delay=0.1); h.press("\n", delay=0.1); h.wait(4.0)
    print("sysinfo", h.screenshot(OUT + "/ste-tt-sysinfo.png", settle=0.8), flush=True)
finally:
    print("rc", h.close())
