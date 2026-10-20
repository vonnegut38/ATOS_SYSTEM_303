import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
h = Hatari(P, log=P + "/work/hatari-singleton3.log", window=True)
try:
    h.wait(18)
    h.type("start desktop.app\nstart desktop.app\nstart desktop.app\n", delay=0.05)
    time.sleep(8.0)
    for ln in (h.console_lines(rows=25, settle=0.8) or []): print("|"+ln, flush=True)
finally:
    print("rc", h.close(), flush=True)
