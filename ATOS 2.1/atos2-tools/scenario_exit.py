import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
OUT = "/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/exit"
h = Hatari(P, log=P + "/work/hatari-exit.log", window=True)
try:
    h.wait(8)
    for i in (1,2):
        h.type("run hello.app", delay=0.1); h.press("\n", delay=0.1); h.wait(1.8)
        print("run", i, h.screenshot(f"{OUT}/e{i}.png"), flush=True)
finally:
    print("rc", h.close())
