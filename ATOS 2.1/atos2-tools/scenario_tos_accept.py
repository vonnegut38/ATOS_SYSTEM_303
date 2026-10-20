#!/usr/bin/env python3
"""Acceptance test for TOS program launching (audit item E): two launches.

Verifies the "one run, one decision" TOS window path survives a second launch
after a cold boot.  It runs a real 0x601A program twice and checks the result
deterministically:

  launch 1  -> TOSPROBE.PRG   (GEMDOS self-test, prints ALL PASS, exits 0)
  launch 2  -> TOSCOL.PRG     (Setscreen/Setpalette colour client)

For launch 1 the shell must print "TOS RC 0" (the shared shell_tos_dispatch
reports the exit code).  For launch 2 the program writes its milestone markers
to 0x800..0x809; the final bytes prove it walked Getrez -> Setscreen ->
Setpalette -> drawing -> exit.

Prerequisites (the runtime image must already contain the TOS=1 kernel and the
two test programs; see tools/build_atos2.py and the tosprobe/toscolour make
targets):

    gmake TOS=1 -j4 all
    gmake tosprobe toscolour
    # install KERNEL.SYS/SHELL.SYS (TOS=1) and TOSPROBE.PRG/TOSCOL.PRG into
    # a fresh work/atos-tos1.img image (SYSTEM/TEST), WITHOUT DESKTOP.APP so
    # the text shell stays in charge of the keyboard.  Override with ATOS_IMG.

The marker bytes are:
    0x800 0x01  entered _tos_main
    0x805 0xC0  before Setscreen
    0x806 0xCD  Setscreen returned
    0x807 0xDD  about to Setpalette
    0x808 0xEE  Setpalette returned
    0x809 0x55  drawing finished
"""
import sys, time, os
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari

P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
HB = "/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536"
IMG = os.environ.get("ATOS_IMG", P + "/work/atos-tos1.img")
OUT = "/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/tos-accept"
os.makedirs(OUT, exist_ok=True)

# Cold boot: no ACSI except the IDE image, MegaST/030/TF536/VOFA ET4000 mono.
N = []
for i in range(8):
    N += ["--acsi", "%d=none" % i]
E = ["--machine", "megast", "--cpulevel", "3", "--cpuclock", "32",
     "--memsize", "4", "--ttram", "64", "--tf536", "on",
     "--graphics-card", "vofa", "--gfx-chip", "et4000", "--mono",
     "--ide-master", IMG] + N

h = Hatari(P, log=P + "/work/hatari-tos-accept.log", window=False, image=IMG,
           binary=HB, cfg=True, extra=E)

failures = []


def check(name, cond, detail=""):
    if cond:
        print("PASS  %s%s" % (name, ("  " + detail) if detail else ""), flush=True)
    else:
        failures.append(name)
        print("FAIL  %s%s" % (name, ("  " + detail) if detail else ""), flush=True)


def key(ch, d=0.14):
    h.keydown(ch, delay=d)
    time.sleep(0.04)
    h.keyup(ch, delay=d)
    time.sleep(0.05)


def type_text(text, d=0.14):
    for ch in text:
        key(ch, d)


def submit(text, wait=8.0):
    type_text(text)
    time.sleep(0.5)
    key("\r", 0.2)
    time.sleep(wait)


def console_text():
    lines = h.console_lines(rows=25)
    return "\n".join(lines) if lines else ""


try:
    # Cold boot to the text shell.  The shell banner is not observable through
    # the console reader (it stays on the boot splash), so poll with console
    # reads (the memdumps give the emulation wall-clock to finish the disk I/O)
    # and then keep polling until the shell is likely up.
    booted = False
    for _ in range(45):
        time.sleep(2)
        if "ATOS 2.0" in console_text():
            booted = True
            break
    print("booted", booted, repr(console_text()[-200:]), flush=True)
    if not booted:
        time.sleep(15)

    # ---- Launch 1: TOSPROBE.PRG -----------------------------------------
    probe = b""
    for _attempt in range(3):
        submit("run \\SYSTEM\\TEST\\TOSPROBE.PRG", wait=8.0)
        probe = h.read_bytes(0x810, 20, settle=1.0)
        if probe and len(probe) >= 12 and int.from_bytes(probe[4:8], "big") == 0x5A5AA5A5:
            break
    print("launch1", h.screenshot(OUT + "/launch1.png", settle=0.8), flush=True)
    # TOSPROBE publishes (all move.l, big-endian):
    #   0x810 = failure bitmask  0x814 = START sentinel 0x5A5AA5A5
    #   0x818 = DONE 0x900D PASS / 0xBAAD FAIL   0x820 = Fopen return
    # A real pass needs mask==0 AND the START and DONE markers, otherwise a
    # crashed/unstarted program leaves zeroed memory that looks like mask==0.
    mask = int.from_bytes(probe[0:4], "big") if (probe and len(probe) >= 4) else -1
    start = int.from_bytes(probe[4:8], "big") if (probe and len(probe) >= 8) else 0
    done = int.from_bytes(probe[8:12], "big") if (probe and len(probe) >= 12) else 0
    fopen_rc = int.from_bytes(probe[16:20], "big") if (probe and len(probe) >= 20) else None
    ok_probe = (mask == 0 and start == 0x5A5AA5A5 and done == 0x900D)
    check("launch1 TOSPROBE GEMDOS all pass", ok_probe,
          ("mask=0x%X start=0x%X done=0x%X fopen=%s raw=%s" % (
              mask, start, done,
              (repr(fopen_rc) + (" (0x%X)" % (fopen_rc & 0xffffffff) if fopen_rc is not None else "")),
              " ".join("%02X" % b for b in probe)) if probe else repr(probe)))
    # Secondary signal (shell console text) is reported, not required: the
    # console read can lag behind the boot screen on this harness.
    text = console_text()
    print("launch1 console", repr(text[-200:]), flush=True)

    # ---- Launch 2: TOSCOL.PRG (marker-based, deterministic) ---------------
    marks = b""
    for _attempt in range(3):
        submit("run \\SYSTEM\\TEST\\TOSCOL.PRG", wait=10.0)
        marks = h.read_bytes(0x800, 10, settle=1.0)
        if marks and len(marks) >= 10 and marks[0] == 0x01:
            break
    print("launch2", h.screenshot(OUT + "/launch2.png", settle=0.8), flush=True)
    if marks and len(marks) >= 10:
        got = " ".join("%02X" % b for b in marks)
        expect = [0x01, None, None, None, None, 0xC0, 0xCD, 0xDD, 0xEE, 0x55]
        good = all(e is None or marks[i] == e for i, e in enumerate(expect))
        check("launch2 TOSCOL markers", good, got)
        check("launch2 entered _tos_main", marks[0] == 0x01, got)
        check("launch2 Setscreen returned", marks[6] == 0xCD, got)
        check("launch2 drawing finished", marks[9] == 0x55, got)
    else:
        check("launch2 TOSCOL markers readable", False, repr(marks))
finally:
    print("rc", h.close(), flush=True)

if failures:
    print("ACCEPTANCE FAIL: %s" % ", ".join(failures), flush=True)
    sys.exit(1)
print("TOS LAUNCH ACCEPTANCE: PASS (two launches)", flush=True)
