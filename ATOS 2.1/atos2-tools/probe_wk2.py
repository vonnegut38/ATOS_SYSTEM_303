import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
h = Hatari(P, log=P + "/work/hatari-wk2.log", window=True)
try:
    h.wait(24)
    h.press("F5", delay=0.7); time.sleep(4.0)      # launcher click -> spawn AC
    h.cmd("hatari-debug screenshot /tmp/wk2-a.png"); time.sleep(2.0)
    h.press("F5", delay=0.7); time.sleep(3.0)      # launcher click again -> should RESTORE
    h.cmd("hatari-debug screenshot /tmp/wk2-b.png"); time.sleep(2.0)
    print("done", flush=True)
finally:
    print("rc", h.close(), flush=True)
