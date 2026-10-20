import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
h = Hatari(P, log=P + "/work/hatari-launch.log", window=True)
try:
    h.wait(20)
    h.press("F10", delay=0.3); time.sleep(0.8)
    h.press("Down", delay=0.2); time.sleep(0.5)
    h.press("Return", delay=0.3); time.sleep(0.8)   # RUN...
    h.type("WINDEMO", delay=0.15); time.sleep(0.5)
    h.press("Return", delay=0.3); time.sleep(5.0)   # spawn
    print("shot:", h.screenshot("/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/desktop-run.png", settle=0.6), flush=True)
finally:
    print("rc", h.close(), flush=True)
