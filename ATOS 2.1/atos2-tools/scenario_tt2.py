import sys, os
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
IMG = P + "/work/atos-v2-tt.img"
OUT = "/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/ttram"
os.makedirs(OUT, exist_ok=True)
configs = [
  ("st",     ["--machine","st","--cpulevel","3","--cpuclock","16","--ttram","64"]),
  ("megast", ["--machine","megast","--cpulevel","3","--cpuclock","16","--ttram","64"]),
]
for name, extra in configs:
    h = Hatari(P, log=P + "/work/hatari-tt2-" + name + ".log", window=True, image=IMG, extra=extra)
    try:
        h.wait(19)
        h.type("memory", delay=0.1); h.press("\n", delay=0.1); h.wait(2.0)
        print(name, "memory", h.screenshot(OUT + "/v2-" + name + "-memory.png", settle=0.8), flush=True)
        h.type("run sysinfo.app", delay=0.1); h.press("\n", delay=0.1); h.wait(4.5)
        print(name, "sysinfo", h.screenshot(OUT + "/v2-" + name + "-sysinfo.png", settle=0.8), flush=True)
    finally:
        print(name, "rc", h.close())
