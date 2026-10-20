import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
h = Hatari(P, log=P + "/work/hatari-sysinfo-req.log", window=True)
try:
    h.wait(15)
    h.type("run sysinfo.app", delay=0.12); h.press("\n", delay=0.1); h.wait(8.0)
    for ln in (h.console_lines(rows=25, settle=0.6) or []): print("|"+ln, flush=True)
    print("shot:", h.screenshot("/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/sysinfo-v3.png", settle=0.6), flush=True)
except Exception as e:
    print("ERR", e, flush=True)
finally:
    print("rc", h.close(), flush=True)
