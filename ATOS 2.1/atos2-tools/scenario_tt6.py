import sys, os
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
IMG = P + "/work/atos-v2-tt.img"
OUT = "/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/ttram"
os.makedirs(OUT, exist_ok=True)
h = Hatari(P, log=P + "/work/hatari-tt6.log", window=True, image=IMG,
           extra=["--machine","tt","--cpulevel","3","--cpuclock","32","--ttram","64",
                  "--tos", P + "/build/bios_tt_test.bin"])
try:
    h.wait(19)
    print("boot", h.screenshot(OUT + "/v6-tt-boot.png", settle=0.8), flush=True)
    h.type("memory", delay=0.1); h.press("\n", delay=0.1); h.wait(2.0)
    print("memory", h.screenshot(OUT + "/v6-tt-memory.png", settle=0.8), flush=True)
    h.type("run sysinfo.app", delay=0.1); h.press("\n", delay=0.1); h.wait(4.5)
    print("sysinfo", h.screenshot(OUT + "/v6-tt-sysinfo.png", settle=0.8), flush=True)
finally:
    print("rc", h.close())
