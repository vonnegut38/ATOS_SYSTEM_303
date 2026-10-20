#!/usr/bin/env python3
"""Live Hatari demo of the shell_fs builtins (COPY/MOVE/DEL/MKDIR) + TYPE.

Runs with a real window (window=True), unlike the headless regression scripts,
so the operations are visible on screen.  Screenshots land in
atos2-tools/shots/demo-*.png.

    python3 atos2-tools/scenario_demo_fs.py
"""
import os, sys, time

sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari

PROJECT = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
SHOTS = "/Users/xxx/Documents/Deepseek_test/atos2-tools/shots"
h = Hatari(PROJECT, log=PROJECT + "/work/hatari-demo-fs.log", window=True, extra=[])


def cursor():
    lines = h.console_lines(rows=1, settle=0.2) or []
    return lines[-1].rstrip() if lines else ""


def send(line):
    for _ in range(6):
        h.type(line, delay=0.12)
        time.sleep(0.6)
        if cursor().endswith(line):
            h.press("\n", delay=0.15)
            return True
        for _ in range(len(line) + 8):
            h.press(chr(8), 0.05)
        time.sleep(0.25)
    return False


steps = [
    ("md \\DEMO", "mkdir"),
    ("dir \\", "dir-root"),
    ("copy \\SYSTEM\\FAULT.APP \\DEMO\\F.APP", "copy"),
    ("dir \\DEMO", "dir-after-copy"),
    ("move \\DEMO\\F.APP \\DEMO\\G.APP", "move"),
    ("dir \\DEMO", "dir-after-move"),
    ("type \\NOTES.TXT", "type"),
    ("del \\DEMO\\G.APP", "del-file"),
    ("del \\DEMO", "del-dir"),
    ("dir \\DEMO", "dir-gone"),
]
try:
    h.wait(14)
    for i, (line, name) in enumerate(steps, 1):
        ok = send(line)
        time.sleep(1.4)
        shot = h.screenshot("%s/demo-%02d-%s.png" % (SHOTS, i, name), settle=0.4)
        print("> %-42s typed=%s shot=%s" % (line, ok, shot), flush=True)
    time.sleep(2.0)
    print("screens:", flush=True)
    for ln in (h.console_lines(rows=25, settle=0.3) or []):
        print("  |" + ln, flush=True)
    print("DONE", flush=True)
finally:
    print("rc", h.close(), flush=True)
