import sys, os
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
IMG = P + "/work/atos-v2-tt.img"
OUT = "/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/ttram"
os.makedirs(OUT, exist_ok=True)
h = Hatari(P, log=P + "/work/hatari-ttram.log", window=True, image=IMG,
           extra=["--machine", "megast", "--ttram", "64", "--cpuclock", "32"])
try:
    for t, w in [("t5",5),("t8",3),("t11",3),("t15",4),("t19",4)]:
        h.wait(w)
        print(t, h.screenshot(OUT + "/tt-" + t + ".png", settle=0.8), flush=True)
    h.type("memory", delay=0.1); h.press("\n", delay=0.1); h.wait(2.0)
    print("memory", h.screenshot(OUT + "/tt-memory.png", settle=0.8), flush=True)
finally:
    print("rc", h.close())
