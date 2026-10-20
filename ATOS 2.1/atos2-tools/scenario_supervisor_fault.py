#!/usr/bin/env python3
"""Regression: a supervisor-mode bus error must still panic the kernel.

The user-fault containment must not swallow a kernel fault.  Forcing SR=0x2500
(S=1) and PC=0xF00000 makes the CPU take a bus error in supervisor mode; the
kernel must report it (kernel_fault_report clears kernel_fault_vector after
printing) instead of terminating a user process.  CPU=0 is the 68000 group-1
frame, CPU=3 the 68030 SR/PC frame.  Exit status is non-zero on failure.
"""
import os, subprocess, sys, time

sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari

PROJECT = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
ELF = PROJECT + "/build/system/kernel.elf"
CPU = os.environ.get("CPU", "0")


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
h = Hatari(PROJECT, log=PROJECT + "/work/hatari-supervisor-" + CPU + ".log",
           window=False, extra=["--cpulevel", CPU])
try:
    h.wait(12)
    h.cmd("hatari-debug r sr=0x2500")
    time.sleep(0.3)
    h.cmd("hatari-debug r pc=0x00f00000")
    time.sleep(2.0)
    got_pc = h.read_u32(S["kernel_fault_pc"], settle=0.4) or 0
    got_sr = h.read_u32(S["kernel_fault_sr"], settle=0.4) or 0
    got_vec = h.read_u32(S["kernel_fault_vector"], settle=0.4) or 0
    # kernel_fault_report() clears the vector after printing the panic screen.
    # On the 68000 the captured value is the group-1 fault address, which the
    # prefetch can advance a few bytes past the instruction PC set here.
    ok = 0x00F00000 <= got_pc <= 0x00F00006 and (got_sr & 0x2000) != 0 and got_vec == 0
    print("supervisor fault: pc=%08X sr=%08X vec=%s  %s"
          % (got_pc, got_sr, got_vec, "PASS" if ok else "FAIL"), flush=True)
    print("RESULT:", "OK" if ok else "FAIL")
    sys.exit(0 if ok else 1)
finally:
    h.close()
