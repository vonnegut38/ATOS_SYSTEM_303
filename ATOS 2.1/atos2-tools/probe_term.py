import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
h = Hatari(P, log=P + "/work/hatari-term.log", window=True)
try:
    h.wait(20)
    h.press("F10", delay=0.3); time.sleep(0.8)
    h.press("Return", delay=0.3); time.sleep(1.5)   # OPEN TERMINAL (first item)
    print("shot:", h.screenshot("/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/desktop-terminal.png", settle=0.6), flush=True)
finally:
    print("rc", h.close(), flush=True)
