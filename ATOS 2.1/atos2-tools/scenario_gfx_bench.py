"""Phase 1 graphics benchmark: run GFXBENCH.APP in a windowed Hatari and
parse the on-screen report.

Usage: python3 scenario_gfx_bench.py [shifter|et4000]
"""
import re, subprocess, sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari

P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
IMG = P + "/work/atos-v2.img"
HB = "/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536"
profile = (sys.argv[1] if len(sys.argv) > 1 else "shifter").lower()

NOACSI = []
for i in range(8):
    NOACSI += ["--acsi", "%d=none" % i]

binary, image, extra = "hatari", None, []
if profile == "et4000":
    binary, image = HB, IMG
    extra = ["--machine", "megast", "--cpulevel", "3", "--cpuclock", "32",
             "--memsize", "4", "--ttram", "64", "--tf536", "on",
             "--graphics-card", "vofa", "--gfx-chip", "et4000", "--mono",
             "--ide-master", IMG] + NOACSI
elif profile == "shifter-noblit":
    extra = ["--blitter", "off"]
elif profile == "rgb":
    extra = ["--monitor", "rgb"]
elif profile == "megast-noblit":
    extra = ["--machine", "megast", "--blitter", "off"]
elif profile != "shifter":
    print("unknown profile:", profile, flush=True)
    sys.exit(2)

h = Hatari(P, log=P + "/work/hatari-gfxbench-" + profile + ".log",
           window=True, extra=extra, binary=binary, image=image)

def foreground():
    try:
        subprocess.run(["osascript", "-e",
            'tell application "System Events" to set frontmost of (first process whose name contains "hatari") to true'],
            capture_output=True, timeout=5)
    except Exception:
        pass

def screen_lines(rows=25, settle=0.35):
    return h.console_lines(rows=rows, settle=settle) or []

def cursor():
    # The shell blink leaves '_' in the cursor cell; strip it before matching.
    ls = h.console_lines(rows=1, settle=0.25) or []
    return ls[-1].rstrip().rstrip("_") if ls else ""

def send(line, tries=15):
    for _ in range(tries):
        h.type(line, delay=0.10)
        time.sleep(0.7)
        if cursor().endswith(line):
            h.press("\n", delay=0.15)
            return True
        for _ in range(len(line) + 8):
            h.press(chr(8), 0.05)
        time.sleep(0.3)
    return False

try:
    h.wait(16)
    foreground()
    print("typed run:", send("run gfxbench.app"), flush=True)
    deadline = time.time() + 120
    lines = []
    last = 0.0
    while time.time() < deadline:
        time.sleep(2.0)
        lines = screen_lines()
        if time.time() - last > 10.0:
            last = time.time()
            nonempty = [l for l in lines if l.strip()]
            print("  ... last=%r" % (nonempty[-1] if nonempty else ""), flush=True)
        if "GFXBENCH DONE" in "\n".join(lines):
            break
    h.screenshot("/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/gfxbench-" + profile + ".png", settle=0.4)
    text = "\n".join(lines)
    print("=== profile: " + profile + " ===", flush=True)
    for ln in lines:
        print("  |" + ln, flush=True)
    m = re.search(r"GFXBENCH W=(\d+) H=(\d+) PLANES=(\d+) MODE=(\d+) LINE=(\d+) CAPS=(\d+)", text)
    print("HEADER w/h/planes/mode/line/caps:", m.groups() if m else None, flush=True)
    base = None
    for name, it, tk in re.findall(r"([A-Z0-9]+)\s+ITER=(\d+) CNT=(\d+)", text):
        it, tk = int(it), int(tk)
        if not tk:
            continue
        us = tk * 1000000.0 / 12288.0 / it
        if name == "POLL":
            base = us
        net = ("%9.1f" % (us - base)) if base is not None and name != "POLL" else "        -"
        print("BENCH %-9s n=%-6d cnt=%-6d us/op=%9.1f net=%s" % (
            name, it, tk, us, net), flush=True)
    print("DONE", flush=True)
finally:
    print("rc", h.close(), flush=True)
