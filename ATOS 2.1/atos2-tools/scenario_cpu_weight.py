import re, subprocess, sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
h = Hatari(P, log=P + "/work/hatari-cpu-weight.log", window=True, extra=[])

def foreground():
    try:
        subprocess.run(["osascript", "-e",
            'tell application "System Events" to set frontmost of (first process whose name contains "hatari") to true'],
            capture_output=True, timeout=5)
    except Exception:
        pass

def cursor():
    # The shell blink puts '_' in the cursor cell; strip it before matching.
    ls = h.console_lines(rows=1, settle=0.25) or []
    return ls[-1].rstrip().rstrip("_") if ls else ""

def send(line, tries=10):
    for _ in range(tries):
        h.type(line, delay=0.12)
        time.sleep(0.8)
        if cursor().endswith(line):
            h.press("\n", delay=0.15)
            return True
        for _ in range(len(line) + 8):
            h.press(chr(8), 0.05)
        time.sleep(0.3)
    return False

try:
    h.wait(14)
    print("start tts1:", send("start hang.app"), flush=True)
    time.sleep(1.5)
    print("tts2      :", send("tts2"), flush=True)
    time.sleep(1.5)
    print("start tts2:", send("start hang.app"), flush=True)
    time.sleep(1.5)
    print("tts1      :", send("tts1"), flush=True)
    time.sleep(14.0)
    print("jobs      :", send("jobs"), flush=True)
    time.sleep(2.5)
    screen = h.console_lines(rows=25, settle=0.35) or []
    h.screenshot("/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/cpu-weight.png", settle=0.4)
    print("=== screen ===", flush=True)
    for ln in screen:
        print("  |" + ln, flush=True)
    print("cpu ticks:", re.findall(r"cpu=(\d+)", "\n".join(screen)), flush=True)
    print("DONE", flush=True)
finally:
    print("rc", h.close(), flush=True)
