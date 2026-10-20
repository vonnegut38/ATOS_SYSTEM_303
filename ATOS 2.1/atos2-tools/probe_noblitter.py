import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
h = Hatari(P, log=P + "/work/hatari-noblitter.log", window=True, extra=["--blitter","off"])
try:
    h.wait(24)
    print("shot:", h.screenshot("/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/noblitter.png", settle=0.8), flush=True)
    for ln in (h.console_lines(rows=10, settle=0.5) or [])[:6]: print("|"+ln, flush=True)
finally:
    print("rc", h.close(), flush=True)
