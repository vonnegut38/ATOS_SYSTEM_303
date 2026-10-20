import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
OUT = "/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/menu"
h = Hatari(P, log=P + "/work/hatari-menu2.log", window=True)
try:
    h.wait(9)
    h.type("run command.app", delay=0.1); h.press("\n", delay=0.1)
    for t in (3,6,10):
        h.wait(3 if t==3 else (3 if t==6 else 4))
        print(t, h.screenshot(OUT + f"/wait{t}.png"), flush=True)
finally:
    print("rc", h.close())
