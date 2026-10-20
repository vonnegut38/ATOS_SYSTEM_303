#!/usr/bin/env python3
"""5-minute live stress test in a real Hatari window.

Every typed command is verified on the cursor row and then confirmed with an
explicit ENTER.  The load mixes visible stats, CPU-fault containment,
shell_fs mutations, session switching and process kill, so the window shows
continuous activity.

    python3 atos2-tools/scenario_stress_live.py [seconds]
"""
import re, sys, time

sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari

PROJECT = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
SHOTS = "/Users/xxx/Documents/Deepseek_test/atos2-tools/shots"
DURATION = int(sys.argv[1]) if len(sys.argv) > 1 else 300
h = Hatari(PROJECT, log=PROJECT + "/work/hatari-stress-live.log", window=True, extra=[])


def cursor():
    lines = h.console_lines(rows=1, settle=0.22) or []
    return lines[-1].rstrip().rstrip("_") if lines else ""


def send(line, tries=8):
    """Type the command, verify it, then tap ENTER.  Returns True on ENTER."""
    for _ in range(tries):
        h.type(line, delay=0.10)
        time.sleep(0.55)
        if cursor().endswith(line):
            h.press("\n", delay=0.22)
            print("   $ %-38s ENTER" % line, flush=True)
            return True
        for _ in range(len(line) + 8):
            h.press(chr(8), 0.04)
        time.sleep(0.2)
    print("   ! %-38s NOT TYPED" % line, flush=True)
    return False


def screen(rows=25):
    return "\n".join(h.console_lines(rows=rows, settle=0.3) or [])


def shot(tag):
    ok = h.screenshot("%s/stress-%s.png" % (SHOTS, tag), settle=0.4)
    print("   [shot %s %s]" % (tag, ok), flush=True)


def hang_pids():
    return [int(p) for p in re.findall(r"hang\.app\s+PID (\d+)", screen())]


try:
    h.wait(14)
    # Wait for the shell prompt before the first command: a previous Hatari
    # instance may still be closing and the console read can lag the boot.
    for _ in range(24):
        if ">" in cursor():
            break
        time.sleep(0.5)
    started = time.monotonic()
    print("STRESS start, %d s, window=on" % DURATION, flush=True)
    cycle = 0
    while time.monotonic() - started < DURATION:
        cycle += 1
        elapsed = time.monotonic() - started
        print("--- cycle %d  t=%.0fs ---" % (cycle, elapsed), flush=True)
        for cmd in ("memory", "inputstat", "tasks"):
            send(cmd)
        send("run busflt.app")
        send("run adrflt.app")
        send("run divflt.app")
        send("md \\S")
        send("copy \\SYSTEM\\FAULT.APP \\S\\A.APP")
        send("dir \\S")
        send("move \\S\\A.APP \\S\\B.APP")
        send("del \\S\\B.APP")
        send("del \\S")
        send("run \\NOPE.APP")
        send("del \\NOPE")
        if cycle % 3 == 0:
            send("start hang.app")
            time.sleep(1.5)
            send("jobs")
            for pid in hang_pids():
                send("kill %d" % pid)
        send("tts2")
        send("dir \\")
        send("tts1")
        if cycle % 2 == 0:
            shot("cycle-%02d" % cycle)
    print("STRESS done after %d cycles" % cycle, flush=True)
    print("DONE", flush=True)
finally:
    print("rc", h.close(grace=6.0), flush=True)
