import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
h = Hatari(P, log=P + "/work/hatari-desktop-console.log", window=True)
try:
    h.wait(15)
    h.type("run desktop.app", delay=0.12); h.press("\n", delay=0.1); h.wait(9.0)
    print("shot1:", h.screenshot("/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/desktop-console1.png", settle=0.6), flush=True)
    h.type("help", delay=0.15); h.press("\n", delay=0.15); h.wait(3.5)
    print("shot2:", h.screenshot("/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/desktop-console2.png", settle=0.6), flush=True)
    for ln in (h.console_lines(rows=25, settle=0.5) or []):
        print("|" + ln, flush=True)
finally:
    print("rc", h.close(), flush=True)
