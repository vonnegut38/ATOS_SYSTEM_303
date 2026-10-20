import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
OUT = "/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/buf"
h = Hatari(P, log=P + "/work/hatari-buf-mem.log", window=True)
try:
    h.wait(8)
    for i in (1, 2):
        h.type("memory", delay=0.11); h.press("\n", delay=0.11); h.wait(1.0)
        print("mem", i, h.screenshot(f"{OUT}/m{i}.png"), flush=True)
finally:
    print("rc", h.close())
