import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
OUT = "/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/menu"
h = Hatari(P, log=P + "/work/hatari-menu.log", window=True)
try:
    h.wait(9)
    h.type("run command.app", delay=0.1); h.press("\n", delay=0.1); h.wait(2.5)
    print("bar", h.screenshot(OUT + "/m0-bar.png"), flush=True)
    h.press("F9", delay=0.15); h.wait(0.5)
    print("left-menu", h.screenshot(OUT + "/m1-leftmenu.png"), flush=True)
    h.press("Down", delay=0.15); h.press("Return", delay=0.15); h.wait(0.8)
    print("date", h.screenshot(OUT + "/m2-date.png"), flush=True)
    h.press("Tab", delay=0.15); h.press("F9", delay=0.15); h.wait(0.4)
    h.press("Down", delay=0.15); h.press("Down", delay=0.15); h.press("Return", delay=0.15); h.wait(0.8)
    print("right-type", h.screenshot(OUT + "/m3-righttype.png"), flush=True)
finally:
    print("rc", h.close())
