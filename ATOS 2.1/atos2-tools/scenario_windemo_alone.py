import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
h = Hatari(P, log=P + "/work/hatari-windemo-alone.log", window=True)
try:
    h.wait(15)
    h.type("run windemo.app", delay=0.12); h.press("\n", delay=0.1); h.wait(6.0)
    print("shot:", h.screenshot("/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/windemo-alone.png", settle=0.6), flush=True)
    for ln in (h.console_lines(rows=25, settle=0.5) or []): print("|" + ln, flush=True)
finally:
    print("rc", h.close(), flush=True)
