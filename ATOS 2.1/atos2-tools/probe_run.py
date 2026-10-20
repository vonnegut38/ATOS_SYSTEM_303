import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
h = Hatari(P, log=P + "/work/hatari-run.log", window=True)
try:
    h.wait(18)
    h.type("run desktop.app", delay=0.2); h.press("\n", delay=0.1); h.wait(10.0)
    for ln in (h.console_lines(rows=25, settle=0.8) or []): print("|"+ln, flush=True)
    print("shot:", h.screenshot("/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/run-desktop.png", settle=0.6), flush=True)
finally:
    print("rc", h.close(), flush=True)
