#!/usr/bin/env python3
"""Requests-per-frame measurement (ETAP 2.6).

Boots the desktop, starts an application window and reads the kernel's
requests-per-frame counters straight from guest memory.  Symbol addresses are
resolved from the freshly built ELF, so the script survives kernel rebuilds.

Counters (kernel_graphics.c / kernel_wm.c):
  gfx_requests        every serviced GFX request (broker opcode 42)
  wm_requests_total   sum of per-frame samples
  wm_requests_last    requests seen in the last composed frame
  wm_requests_max     peak requests in a single composed frame
  wm_frames           composed frames
"""
import shutil, struct, subprocess, sys, time

sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari

P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
WANT = ("gfx_requests", "wm_requests_total", "wm_requests_last",
        "wm_requests_max", "wm_frames")


def symbol_map(elf):
    nm = shutil.which("m68k-elf-nm") or "/opt/homebrew/opt/m68k-elf-binutils/bin/m68k-elf-nm"
    out = subprocess.check_output([nm, elf]).decode()
    syms = {}
    for line in out.splitlines():
        parts = line.split()
        if len(parts) == 3:
            try:
                syms[parts[2]] = int(parts[0], 16)
            except ValueError:
                pass
    return syms


def main():
    syms = symbol_map(P + "/build/system/kernel.elf")
    missing = [s for s in WANT if s not in syms]
    if missing:
        print("missing symbols:", missing)
        return 2
    h = Hatari(P, log=P + "/work/hatari-reqframe.log", window=True)

    def u32(name):
        b = h.read_bytes(syms[name], 4, settle=0.25)
        return struct.unpack(">I", b)[0] if b and len(b) == 4 else None

    def snap(tag):
        row = {s: u32(s) for s in WANT}
        print("%-9s %s" % (tag, row), flush=True)

    try:
        h.wait(15)
        h.type("start desktop.app", delay=0.12); h.press("\n", delay=0.1); h.wait(8.0)
        snap("desktop")
        h.type("start windemo.app", delay=0.12); h.press("\n", delay=0.1); h.wait(6.0)
        snap("windemo")
        time.sleep(1.0)
        snap("idle")
        total = u32("wm_requests_total")
        frames = u32("wm_frames") or 1
        print("requests/frame avg = %.3f" % (total / frames), flush=True)
    finally:
        print("rc", h.close(), flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
