import sys, os
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
IMG = P + "/work/atos-v2-tt.img"
OUT = "/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/ttram"
os.makedirs(OUT, exist_ok=True)
h = Hatari(P, log=P + "/work/hatari-tt3.log", window=True, image=IMG,
           extra=["--machine","ste","--cpulevel","3","--cpuclock","16","--ttram","64"])
try:
    h.wait(19)
    h.type("memory", delay=0.1); h.press("\n", delay=0.1); h.wait(2.0)
    print("memory", h.screenshot(OUT + "/v3-ste-memory.png", settle=0.8), flush=True)
    h.type("run sysinfo.app", delay=0.1); h.press("\n", delay=0.1); h.wait(4.5)
    print("sysinfo", h.screenshot(OUT + "/v3-ste-sysinfo.png", settle=0.8), flush=True)
    h.press("F10", delay=0.4); h.wait(1.5)
    h.type("run command.app", delay=0.1); h.press("\n", delay=0.1); h.wait(6)
    print("commander", h.screenshot(OUT + "/v3-ste-commander.png", settle=0.8), flush=True)
finally:
    print("rc", h.close())
