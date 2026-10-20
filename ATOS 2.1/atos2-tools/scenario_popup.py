import sys, os
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
OUT = "/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/cmdmenu2"
os.makedirs(OUT, exist_ok=True)
h = Hatari(P, log=P + "/work/hatari-popup.log", window=True)
try:
    h.wait(10)
    h.type("run command.app", delay=0.08); h.press("\n", delay=0.08); h.wait(14)
    print("base", h.screenshot(OUT + "/p0-base.png", settle=1.0), flush=True)
    h.press("F9", delay=0.5); h.wait(2.5)
    print("f9a", h.screenshot(OUT + "/p1-f9a.png", settle=1.0), flush=True)
    h.wait(2.0)
    print("f9b", h.screenshot(OUT + "/p2-f9b.png", settle=1.0), flush=True)
    h.press("Down", delay=0.4); h.wait(0.5); h.press("Down", delay=0.4); h.wait(1.5)
    print("sel", h.screenshot(OUT + "/p3-type-sel.png", settle=1.0), flush=True)
    h.press("Return", delay=0.4); h.wait(2.0)
    print("applied", h.screenshot(OUT + "/p4-lefttype.png", settle=1.0), flush=True)
finally:
    print("rc", h.close())
