import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
h = Hatari(P, log=P + "/work/hatari-wk.log", window=True)
try:
    h.wait(24)
    h.press("F4", delay=0.7); time.sleep(1.2); h.press("Return", delay=0.7); time.sleep(3.0)
    h.cmd("hatari-debug screenshot /tmp/wk-term.png"); time.sleep(2.0)
    h.press("F5", delay=0.7); time.sleep(4.0)      # launcher click -> spawn AC
    h.cmd("hatari-debug screenshot /tmp/wk-ac1.png"); time.sleep(2.0)
    h.press("F5", delay=0.7); time.sleep(3.0)      # launcher click again -> restore
    h.cmd("hatari-debug screenshot /tmp/wk-ac2.png"); time.sleep(2.0)
    print("done", flush=True)
finally:
    print("rc", h.close(), flush=True)
