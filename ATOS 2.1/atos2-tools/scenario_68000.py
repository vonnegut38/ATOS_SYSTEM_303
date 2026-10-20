import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
OUT = "/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/cpu"
h = Hatari(P, log=P + "/work/hatari-68000.log", window=True, extra=["--cpulevel","0"])
try:
    h.wait(10)
    print("boot", h.screenshot(OUT + "/c68000-boot.png"), flush=True)
    h.type("cpu", delay=0.1); h.press("\n", delay=0.1); h.wait(1.0)
    print("cpu", h.screenshot(OUT + "/c68000-cpu.png"), flush=True)
finally:
    print("rc", h.close())
