import os, sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
OUT = "/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/cpu"
APP = os.environ.get("APP", "divflt.app")
h = Hatari(P, log=P + "/work/hatari-68000-" + APP + ".log", window=True, extra=["--cpulevel","0"])
try:
    h.wait(13)
    h.type("run " + APP, delay=0.1); h.press("\n", delay=0.1); h.wait(2.5)
    print(APP, h.screenshot(OUT + "/c68000-" + APP + ".png"), flush=True)
finally:
    print("rc", h.close())
