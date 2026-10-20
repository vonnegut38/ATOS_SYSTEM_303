"""Hardware-verification matrix (PRZEGLAD-PRACY R2/R3/R5/R7/R8): boot the ROM in
several machine profiles and assert the emulator-checkable part of each risk.

- R7: an ST/68000 profile must boot to POST with no fault (the 68030 MMU code
  must never execute on a 68000).
- R5/R8: bank1 (ST, 0xfc0000) and bank2 (MSTE, 0xe00000) each boot.
- R2/R3: the MegaSTE profile exercises the new FDC 0xffff860e write and the IDE
  retry path without regressing POST.

Reads the ROM console mirror (console_text, char[80*25], resolved from the
built ROM ELF because BSS offsets move).  Exit code non-zero if any profile
fails.
"""
import os, sys, threading, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari

P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
HB = "/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536"
IMG = P + "/work/atos-v2.img"


def rom_console_addr(elf):
    """console_text moves whenever ROM BSS changes, so resolve it per build."""
    import subprocess
    out = subprocess.run(["m68k-elf-nm", elf], capture_output=True, text=True).stdout
    for ln in out.splitlines():
        p = ln.split()
        if len(p) == 3 and p[2] == "console_text":
            return int(p[0], 16)
    raise SystemExit("console_text not found in " + elf)

PROFILES = [
    ("st68000",  ["--machine", "st",      "--cpulevel", "0", "--cpuclock", "8",
                  "--memsize", "4", "--ttram", "0", "--monitor", "mono",
                  "--tos", P + "/build/bios_st.bin",   "--acsi", "0=none"], "ATARI ST"),
    # NOTE: no STE profile.  The ST bank has header version 0x0104, which
    # Hatari treats as TOS <=1.4 and refuses outside ST/68000 mode; an STE
    # needs a header >= 0x0162 (or the MSTE/TT bank).  See
    # docs/HARDWARE-VERIFICATION.md (R5/R7).
    ("megaste",  ["--machine", "megaste", "--cpulevel", "3", "--cpuclock", "16",
                  "--memsize", "4", "--ttram", "0", "--monitor", "mono",
                  "--tos", P + "/build/bios_mste.bin", "--acsi", "0=none"], "ATARI MEGA STE"),
]


def _watchdog(sec):
    time.sleep(sec)
    print("WATCHDOG", flush=True)
    os._exit(99)


threading.Thread(target=_watchdog, args=(200,), daemon=True).start()


def console_text(h, addr):
    raw = h.read_bytes(addr, 80 * 25, settle=0.9) or b""
    return "\n".join("".join(chr(c) if 32 <= c < 127 else " "
                              for c in raw[r * 80:(r + 1) * 80]).rstrip()
                      for r in range(25))


fails = []
for name, extra, machine in PROFILES:
    elf = extra[extra.index("--tos") + 1].replace(".bin", ".elf")
    addr = rom_console_addr(elf)
    h = Hatari(P, log=P + "/work/hatari-hw-%s.log" % name, window=True,
               image=IMG, binary=HB, cfg=True, extra=extra)
    try:
        # Poll: post_run() draws the summary and the 4 s auto-boot countdown
        # keeps it on screen, but the diskless scan later scrolls it away.
        # Keep the first sample that carries the POST summary.
        text = ""
        deadline = time.time() + 14
        while time.time() < deadline:
            time.sleep(0.5)
            sample = console_text(h, addr)
            if "ROM:" in sample:
                text = sample
                break
            if sample:
                text = sample
        checks = {
            "machine": machine in text,
            "rom": "ROM:PASS" in text,
            "all_pass": "ALL PASS" in text,
            "no_failure": "POST FAILURE" not in text,
            "no_panic": "PANIC" not in text,
        }
        print("%-9s %s" % (name, checks), flush=True)
        for ln in text.splitlines():
            if "MACHINE" in ln or "POST" in ln or "ROM:" in ln:
                print("     |" + ln, flush=True)
        if not all(checks.values()):
            fails.append(name)
    finally:
        h.close()
print("FAILS:", fails if fails else "none", flush=True)
sys.exit(1 if fails else 0)
