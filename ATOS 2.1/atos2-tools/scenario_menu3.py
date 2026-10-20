import sys, os
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
OUT = "/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/menu4"
os.makedirs(OUT, exist_ok=True)
h = Hatari(P, log=P + "/work/hatari-menu4.log", window=True)
try:
    h.wait(15)
    h.type("run command.app", delay=0.08); h.press("\n", delay=0.08); h.wait(8)
    for _ in range(7):
        h.press("Down", delay=0.2)
    h.press("F3", delay=0.4); h.wait(1.5)
    print("view", h.screenshot(OUT + "/v1-view.png", settle=0.8), flush=True)
    h.press("Down", delay=0.3); h.press("Down", delay=0.3); h.wait(0.6)
    print("viewscrolled", h.screenshot(OUT + "/v2-scrolled.png", settle=0.8), flush=True)
    h.press("F3", delay=0.4); h.wait(0.8)
    print("viewback", h.screenshot(OUT + "/v3-back.png", settle=0.8), flush=True)
    h.press("F9", delay=0.4); h.wait(0.6)
    h.press("Right", delay=0.4); h.wait(0.6)
    print("filemenu", h.screenshot(OUT + "/v4-filemenu.png", settle=0.8), flush=True)
    h.press("F9", delay=0.4); h.wait(0.6)
    h.press("F6", delay=0.4); h.wait(1.0)
    print("rename", h.screenshot(OUT + "/v5-rename.png", settle=0.8), flush=True)
finally:
    print("rc", h.close())
