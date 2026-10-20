import sys, os
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
OUT = "/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/menu3"
os.makedirs(OUT, exist_ok=True)
h = Hatari(P, log=P + "/work/hatari-menu3.log", window=True)
try:
    h.wait(15)
    h.type("run command.app", delay=0.08); h.press("\n", delay=0.08); h.wait(8)
    print("base", h.screenshot(OUT + "/m0-base.png", settle=0.8), flush=True)
    h.press("F9", delay=0.4); h.wait(0.6)
    h.press("Right", delay=0.4); h.wait(0.6)
    print("filemenu", h.screenshot(OUT + "/m1-filemenu.png", settle=0.8), flush=True)
    h.press("Esc", delay=0.4); h.wait(0.5)
    # view the first file: move down a few rows then F3
    for _ in range(6):
        h.press("Down", delay=0.2)
    h.press("F3", delay=0.4); h.wait(1.2)
    print("view", h.screenshot(OUT + "/m2-view.png", settle=0.8), flush=True)
    h.press("Esc", delay=0.4); h.wait(0.6)
    h.press("F6", delay=0.4); h.wait(0.8)
    print("rename", h.screenshot(OUT + "/m3-rename.png", settle=0.8), flush=True)
    h.press("Esc", delay=0.4); h.wait(0.5)
    print("back", h.screenshot(OUT + "/m4-back.png", settle=0.8), flush=True)
finally:
    print("rc", h.close())
