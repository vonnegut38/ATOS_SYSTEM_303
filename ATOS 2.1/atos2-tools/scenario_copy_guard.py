import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
h = Hatari(P, log=P + "/work/hatari-copy-guard.log", window=False, extra=[])
def cursor():
    ls = h.console_lines(rows=1, settle=0.2) or []
    return ls[-1].rstrip() if ls else ""
def send(line):
    for _ in range(6):
        h.type(line, delay=0.10)
        time.sleep(0.6)
        if cursor().endswith(line):
            h.press("\n", delay=0.15)
            return True
        for _ in range(len(line) + 8): h.press(chr(8), 0.04)
        time.sleep(0.25)
    return False
try:
    h.wait(14)
    for line in ["md \\D2",
                 "copy \\SYSTEM\\FAULT.APP \\D2\\A.APP",
                 "copy \\SYSTEM\\FAULT.APP \\D2\\A.APP",
                 "del \\D2\\A.APP",
                 "del \\D2"]:
        ok = send(line); time.sleep(1.1)
        print("> %-44s typed=%s" % (line, ok), flush=True)
    for ln in (h.console_lines(rows=25, settle=0.3) or [])[-9:]:
        print("  |" + ln, flush=True)
    print("DONE", flush=True)
finally:
    print("rc", h.close(), flush=True)
