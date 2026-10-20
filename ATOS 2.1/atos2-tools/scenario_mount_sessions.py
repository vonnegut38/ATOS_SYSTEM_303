import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
h = Hatari(P, log=P + "/work/hatari-mount-sessions.log", window=True, extra=[])
def cursor():
    ls = h.console_lines(rows=1, settle=0.25) or []
    return ls[-1].rstrip().rstrip("_") if ls else ""
def send(line, tries=10):
    for _ in range(tries):
        h.type(line, delay=0.12); time.sleep(0.7)
        if cursor().endswith(line):
            h.press("\n", delay=0.15); return True
        for _ in range(len(line)+8): h.press(chr(8), 0.05)
        time.sleep(0.25)
    return False
def screen(tag):
    print("--- " + tag + " ---", flush=True)
    for ln in (h.console_lines(rows=25, settle=0.3) or [])[-6:]:
        print("  |" + ln, flush=True)
try:
    h.wait(14)
    send("dir \\"); time.sleep(1.2); screen("tts1 dir (C:)")
    send("tts2"); time.sleep(1.0); screen("tts2 active")
    send("d:"); time.sleep(1.5); screen("tts2 d:")
    send("dir \\"); time.sleep(1.2); screen("tts2 dir (D:)")
    send("tts1"); time.sleep(1.0); screen("back to tts1")
    send("dir \\"); time.sleep(1.2); screen("tts1 dir again (C:)")
    print("DONE", flush=True)
finally:
    print("rc", h.close(), flush=True)
