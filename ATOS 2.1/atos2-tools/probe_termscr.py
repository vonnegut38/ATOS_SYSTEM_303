import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
h = Hatari(P, log=P + "/work/hatari-term.log", window=True)
try:
    h.wait(24)
    h.press("F4", delay=0.7); time.sleep(1.5)
    h.press("Return", delay=0.7); time.sleep(3.0)
    h.cmd("hatari-debug screenshot /tmp/term-1.png"); time.sleep(2.0)
    print("done", flush=True)
finally:
    print("rc", h.close(), flush=True)
