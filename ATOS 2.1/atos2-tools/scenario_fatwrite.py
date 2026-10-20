import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
OUT = "/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/buf"
h = Hatari(P, log=P + "/work/hatari-fatwrite.log", window=True)
try:
    h.wait(8)
    h.type("run setup.app", delay=0.1); h.press("\n", delay=0.1); h.wait(2.5)
    print("setup", h.screenshot(OUT + "/s0-setup.png"), flush=True)
    h.press("Down", delay=0.2); h.press("Return", delay=0.2); h.wait(1.5)
    print("saved", h.screenshot(OUT + "/s1-saved.png"), flush=True)
    h.press("Escape", delay=0.2); h.wait(1.0)
    h.type("memory", delay=0.11); h.press("\n", delay=0.11); h.wait(1.0)
    print("mem", h.screenshot(OUT + "/s2-memory.png"), flush=True)
finally:
    print("rc", h.close())
