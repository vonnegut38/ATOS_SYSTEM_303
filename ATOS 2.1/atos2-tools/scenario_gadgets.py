import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
h = Hatari(P, log=P + "/work/hatari-mw2.log", window=True)
def cursor():
    ls = h.console_lines(rows=1, settle=0.3) or []
    return ls[-1].rstrip().rstrip("_") if ls else ""
def send(line, tries=15, delay=0.16):
    for _ in range(tries):
        h.type(line, delay=delay); time.sleep(0.9)
        if cursor().endswith(line):
            h.press("\n", delay=0.15); return True
        for _ in range(len(line) + 8): h.press(chr(8), 0.05)
        time.sleep(0.4)
    return False
try:
    h.wait(15)
    print("run desktop:", send("run desktop.app"), flush=True); time.sleep(7.0)
    print("start windemo:", send("start windemo.app"), flush=True); time.sleep(5.0)
    print("shot:", h.screenshot("/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/desktop-gadgets.png", settle=0.6), flush=True)
finally:
    print("rc", h.close(), flush=True)
