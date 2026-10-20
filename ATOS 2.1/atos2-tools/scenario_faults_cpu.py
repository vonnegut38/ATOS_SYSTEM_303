#!/usr/bin/env python3
"""Regression: user CPU faults must be contained on the 68000 and the 68030.

The kernel exception stubs capture the hardware frame and decide whether the
fault came from user mode.  On the 68000 a bus error (vector 2) and an address
error (vector 3) use the six-word group-1 frame:

    +0 mode word   +2 fault address   +6 opcode   +8 SR   (no stacked PC)

while every other frame (and all 68010+ frames) stack SR first.  The mode word
is not the SR, and its undocumented opcode bits can imitate the supervisor bit,
so reading SR from +0 made the kernel panic instead of terminating the process.

CPU=0 runs the 68000 and CPU=3 the 68030; both are expected to contain all ten
fault applications.  Exit status is non-zero on any mismatch.
"""
import os, re, subprocess, sys, time

sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari

PROJECT = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
ELF = PROJECT + "/build/system/kernel.elf"
CPU = os.environ.get("CPU", "0")

APPS = [
    ("busflt.app", 2), ("adrflt.app", 3), ("divflt.app", 5), ("chkflt.app", 6),
    ("trapvflt.app", 7), ("privflt.app", 8), ("linea.app", 10), ("linef.app", 11),
    ("trap1.app", 33), ("trap15.app", 47),
]


def symbols(names):
    out = subprocess.run(["m68k-elf-nm", ELF], capture_output=True, text=True).stdout
    found = {}
    for line in out.splitlines():
        parts = line.split()
        if len(parts) == 3 and parts[2] in names:
            found[parts[2]] = int(parts[0], 16)
    missing = sorted(n for n in names if n not in found)
    if missing:
        raise SystemExit("missing symbols: " + ", ".join(missing))
    return found


S = symbols(["kernel_fault_vector", "kernel_fault_pc", "kernel_fault_sr"])
h = Hatari(PROJECT, log=PROJECT + "/work/hatari-faults-" + CPU + ".log",
           window=False, extra=["--cpulevel", CPU])


def cursor_line():
    lines = h.console_lines(rows=1) or []
    return lines[-1].rstrip() if lines else ""


def send_run(app):
    """Type 'run <app>' slowly, retrying until the cursor row shows it.

    The emulated ACIA occasionally drops or duplicates a keystroke, so a plain
    type() is not reliable enough for an automated regression."""
    want = "run " + app
    for _ in range(12):
        h.type(want, delay=0.16)
        for _ in range(14):
            time.sleep(0.25)
            if cursor_line().endswith(want):
                h.press("\n", delay=0.15)
                return True
        for _ in range(30):
            h.press(chr(8), 0.05)
        time.sleep(0.4)
    return False


def last_fault_code(text):
    matches = re.findall(r"FAULT\s+([0-9]+)", text)
    return int(matches[-1]) if matches else None


failures = []
try:
    h.wait(14)
    for app, vector in APPS:
        if not send_run(app):
            failures.append(app + ": could not type command")
            continue
        time.sleep(2.5)
        screen = "\n".join(h.console_lines(rows=24) or [])
        if "PANIC" in screen.upper():
            failures.append(app + ": kernel panicked")
            continue
        code = last_fault_code(screen)
        got_vec = h.read_u32(S["kernel_fault_vector"], settle=0.4)
        got_sr = h.read_u32(S["kernel_fault_sr"], settle=0.4)
        got_pc = h.read_u32(S["kernel_fault_pc"], settle=0.4)
        expected = 0x100 + vector
        ok = code == expected and got_vec == vector and (got_sr & 0x2000) == 0
        print("%-12s vec=%-2s sr=%08X pc=%08X shell_fault=%-4s expected=%-4s %s"
              % (app, got_vec, got_sr, got_pc, code, expected,
                 "PASS" if ok else "FAIL"), flush=True)
        if not ok:
            failures.append("%s: vec=%s sr=%08X shell_fault=%s"
                            % (app, got_vec, got_sr, code))
finally:
    h.close()

print("RESULT:", "OK" if not failures else "FAIL")
for item in failures:
    print("  -", item)
sys.exit(1 if failures else 0)
