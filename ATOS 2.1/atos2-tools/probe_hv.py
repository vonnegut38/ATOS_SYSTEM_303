import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
h = Hatari(P, log=P + "/work/hatari-hv.log", window=True)
try:
    h.wait(24)
    h.press("F4", delay=0.7); time.sleep(2.0)     # OPTIONS -> SETTINGS
    h.cmd("hatari-debug screenshot /tmp/hv-1.png"); time.sleep(2.0)
    h.press("F1", delay=0.7); time.sleep(2.0)     # pure hover over the dialog
    h.cmd("hatari-debug screenshot /tmp/hv-2.png"); time.sleep(2.0)
    print("done", flush=True)
finally:
    print("rc", h.close(), flush=True)
