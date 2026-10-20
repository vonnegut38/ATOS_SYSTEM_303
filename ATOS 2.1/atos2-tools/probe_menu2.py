import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
h = Hatari(P, log=P + "/work/hatari-menu2.log", window=True)
try:
    h.wait(30)
    h.press("F10", delay=0.5); time.sleep(2.0)
    print("shot:", h.screenshot("/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/desktop-menu.png", settle=1.0), flush=True)
finally:
    print("rc", h.close(), flush=True)
