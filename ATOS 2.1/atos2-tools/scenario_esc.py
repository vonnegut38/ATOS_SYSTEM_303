import sys, os
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
OUT = "/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/esc"
os.makedirs(OUT, exist_ok=True)
h = Hatari(P, log=P + "/work/hatari-esc.log", window=True)
try:
    h.wait(15)
    h.type("run command.app", delay=0.08); h.press("\n", delay=0.08); h.wait(8)
    print("base", h.screenshot(OUT + "/e0-base.png", settle=0.8), flush=True)
    h.press("Esc", delay=0.4); h.wait(2.0)
    print("esc", h.screenshot(OUT + "/e1-esc.png", settle=0.8), flush=True)
finally:
    print("rc", h.close())
