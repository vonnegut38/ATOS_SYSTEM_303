import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
OUT = "/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/menu"
h = Hatari(P, log=P + "/work/hatari-diag.log", window=True)
try:
    h.wait(9)
    for app in ["hello.app","sysinfo.app","command.app"]:
        h.type("run " + app, delay=0.09); h.press("\n", delay=0.09); h.wait(2.5)
        print(app, h.screenshot(OUT + "/diag-" + app + ".png"), flush=True)
        h.press("F10", delay=0.15); h.wait(1.5)
finally:
    print("rc", h.close())
