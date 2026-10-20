import sys, os
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
IMG = P + "/work/atos-v2-tt.img"
OUT = "/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/ttram"
os.makedirs(OUT, exist_ok=True)
configs = [
  ("a-megast-tt0-16", ["--machine","megast","--ttram","0","--cpuclock","16"]),
  ("b-megast-tt64-16", ["--machine","megast","--ttram","64","--cpuclock","16"]),
  ("c-ste-tt64-16", ["--machine","ste","--ttram","64","--cpuclock","16"]),
  ("d-megast-tt64-32", ["--machine","megast","--ttram","64","--cpuclock","32"]),
]
for name, extra in configs:
    h = Hatari(P, log=P + "/work/hatari-ttprobe-" + name + ".log", window=True, image=IMG, extra=extra)
    try:
        h.wait(20)
        print(name, h.screenshot(OUT + "/probe-" + name + ".png", settle=0.8), flush=True)
    finally:
        print(name, "rc", h.close())
