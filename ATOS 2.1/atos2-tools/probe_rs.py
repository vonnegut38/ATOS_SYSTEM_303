import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
h = Hatari(P, log=P + "/work/hatari-rs.log", window=True)
try:
    h.wait(24)
    h.press("F7", delay=0.8); time.sleep(2.5)     # DISK C file window
    h.cmd("hatari-debug screenshot /tmp/rs-1.png"); time.sleep(2.0)
    h.press("F2", delay=0.8); time.sleep(3.0)     # scripted resize
    h.cmd("hatari-debug screenshot /tmp/rs-2.png"); time.sleep(2.0)
    print("done", flush=True)
finally:
    print("rc", h.close(), flush=True)
