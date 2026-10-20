#!/usr/bin/env python3
"""Launch a TOS program in a window and capture a screenshot.

Boots the TOS=1 image, runs TOSCOL.PRG through the shared shell dispatcher
(a .PRG routes to the TOS personality and opens a window on the 8bpp desktop),
then captures the framebuffer so the window is visible.
"""
import sys, time, os
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari

P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
HB = "/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536"
IMG = os.environ.get("ATOS_IMG", P + "/work/atos-tos1.img")
# NOTE: keep the screenshot path free of spaces — the `hatari-debug screenshot`
# command splits its argument on whitespace, so "ATOS 2.1" breaks it.
OUT = "/tmp/tos-window"
os.makedirs(OUT, exist_ok=True)

N = []
for i in range(8):
    N += ["--acsi", "%d=none" % i]
E = ["--machine", "megast", "--cpulevel", "3", "--cpuclock", "32",
     "--memsize", "4", "--ttram", "64", "--tf536", "on",
     "--graphics-card", "vofa", "--gfx-chip", "et4000", "--mono",
     "--fast-forward", "off", "--ide-master", IMG] + N

# window=True opens the real SDL window (also makes `hatari-debug screenshot`
# capture the actual rendered frame).  The acceptance test proved the command
# works; this just adds a visible capture.
h = Hatari(P, log=P + "/work/hatari-tos-show.log", window=True, image=IMG,
           binary=HB, cfg=True, extra=E)


def key(ch, d=0.14):
    h.keydown(ch, delay=d)
    time.sleep(0.04)
    h.keyup(ch, delay=d)
    time.sleep(0.05)


def typ(t, d=0.14):
    for ch in t:
        key(ch, d)


try:
    # Boot to the text shell (no DESKTOP.APP on this image).  The shell is not
    # observable through the console reader (it stays on the boot splash), so
    # use a generous fixed wait; the acceptance test needs ~120s of wall time.
    time.sleep(150)
    typ("run \\SYSTEM\\TEST\\TOSCOL.PRG")
    time.sleep(0.5)
    key("\r", 0.2)
    # Capture while the program's window is on screen.
    for i in range(5):
        time.sleep(2)
        marks = h.read_bytes(0x800, 10, settle=0.6)
        print("marks %d: %s" % (i, " ".join("%02X" % b for b in marks) if marks else None), flush=True)
        shot = h.screenshot(OUT + "/toscol-%d.png" % i, settle=0.8)
        print("shot %d: %s" % (i, shot), flush=True)
finally:
    print("rc", h.close(), flush=True)
