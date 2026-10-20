import sys, os
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
OUT = "/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/editor"
os.makedirs(OUT, exist_ok=True)
h = Hatari(P, log=P + "/work/hatari-editor.log", window=True)
try:
    h.wait(15)
    h.type("run command.app", delay=0.08); h.press("\n", delay=0.08); h.wait(8)
    for _ in range(9):
        h.press("Down", delay=0.2)
    print("sel", h.screenshot(OUT + "/e0-selected.png", settle=0.8), flush=True)
    h.press("F4", delay=0.4); h.wait(1.5)
    print("editor", h.screenshot(OUT + "/e1-editor.png", settle=0.8), flush=True)
    h.type("X", delay=0.3); h.wait(0.6)
    print("typed", h.screenshot(OUT + "/e2-typed.png", settle=0.8), flush=True)
    h.press("F2", delay=0.4); h.wait(1.8)
    print("saved", h.screenshot(OUT + "/e3-saved.png", settle=0.8), flush=True)
    h.press("F10", delay=0.4); h.wait(1.0)
    print("back", h.screenshot(OUT + "/e4-back.png", settle=0.8), flush=True)
finally:
    print("rc", h.close())
