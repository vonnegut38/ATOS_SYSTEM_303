import sys, time, os
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
OUT = "/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/cmdmenu"
os.makedirs(OUT, exist_ok=True)
h = Hatari(P, log=P + "/work/hatari-cmdmenu.log", window=True)
try:
    h.wait(10)
    h.type("run command.app", delay=0.08); h.press("\n", delay=0.08)
    for t, w in [("t3",3),("t6",3),("t9",3),("t12",3)]:
        h.wait(w)
        print(t, h.screenshot(OUT + "/c-" + t + ".png"), flush=True)
    print("base", h.screenshot(OUT + "/c0-base.png"), flush=True)
    h.press("F9", delay=0.4); h.wait(1.0)
    print("f9", h.screenshot(OUT + "/c1-f9.png"), flush=True)
    h.press("Down", delay=0.3); h.wait(0.3)
    h.press("Return", delay=0.3); h.wait(1.5)
    print("left-date", h.screenshot(OUT + "/c2-leftdate.png"), flush=True)
    h.press("Tab", delay=0.3); h.wait(0.5)
    h.press("F9", delay=0.4); h.wait(0.8)
    h.press("Down", delay=0.3); h.press("Down", delay=0.3); h.press("Return", delay=0.3); h.wait(1.5)
    print("right-type", h.screenshot(OUT + "/c3-righttype.png"), flush=True)
finally:
    print("rc", h.close())
