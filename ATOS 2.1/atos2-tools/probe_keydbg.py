import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
h = Hatari(P, log=P + "/work/hatari-keydbg.log", window=True)
try:
    h.wait(22)
    h.press("F1", delay=0.4); time.sleep(2.0)
    print("shot:", h.screenshot("/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/keydbg.png", settle=0.8), flush=True)
finally:
    print("rc", h.close(), flush=True)
