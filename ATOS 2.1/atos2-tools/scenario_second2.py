import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
h = Hatari(P, log=P + "/work/hatari-second2.log", window=True)
def cmd(line, wait=2.5):
    h.type(line, delay=0.12); h.press("\n", delay=0.1); time.sleep(wait)
try:
    h.wait(15)
    cmd("run desktop.app", 7.0)
    cmd("run \\SYSTEM\\APP\\WINDEMO.APP", 5.0)
    cmd("run sysinfo.app", 5.0)
    for ln in (h.console_lines(rows=25, settle=0.5) or []): print("|" + ln, flush=True)
finally:
    print("rc", h.close(), flush=True)
