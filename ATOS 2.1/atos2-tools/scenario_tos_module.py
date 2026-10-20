#!/usr/bin/env python3
"""TOS.SYS module test: prove the persona really uses the module's buffer.

Runs TOSPROBE.PRG TWICE on a dedicated image (work/atos-tos-mod.img, desktop
disabled so the text shell owns the keyboard) and asserts:

  loads      kdbg_tos_compat_loads      == 1     (one module init, not two)
  adopted    kdbg_tos_adopted_module    == runs  (the persona used its buffer)
  fallbacks  kdbg_tos_screen_fallbacks  == 0     (no hidden second allocation)
  module     kdbg_tos_module_screen      != 0    (the published buffer address)

With --missing (an image WITHOUT TOS.SYS) the same scenario must show
fallbacks >= runs and adopted == 0, so the fallback can never silently stand in
for the module.

Every address comes from build/system/kernel.elf.  They used to be hardcoded and
drifted into async_results[] - the numbers in work/HANDOVER.md came from a
kernel that no longer existed, and the script kept going with booted=False.

Fresh-run protocol (system/tosprobe.c): the program clears DONE (0x818),
increments a run counter (0x81C) and only then writes START (0x814); the result
(mask 0x810, DONE 0x818) comes last.  A stale PASS from the previous run can
therefore never be read as this run's result: the harness waits for a NEW
counter value and only then for DONE.

Exit code: 0 when every assertion holds, 1 on any failure, 2 when the harness
cannot even start (no kernel.elf, no boot).
"""
import os
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from atos2_auto import Hatari  # noqa: E402

PROJECT = Path(__file__).resolve().parent.parent
ELF = PROJECT / "build" / "system" / "kernel.elf"
BINARY = "/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536"
IMAGE = Path(os.environ.get("ATOS_MOD_IMG", str(PROJECT / "work" / "atos-tos-mod.img")))
MISSING = "--missing" in sys.argv[1:]
RUNS = 2
BOOT_TIMEOUT_S = 220.0          # boot with the desktop takes 150-190 s
RUN_TIMEOUT_S = 40.0

# TOSPROBE marker block (big-endian longs).
MARK_MASK = 0x810
MARK_START = 0x814
MARK_DONE = 0x818
MARK_RUN = 0x81C
START_SENTINEL = 0x5A5AA5A5
DONE_PASS = 0x900D
DONE_FAIL = 0xBAAD

WANT = ("kdbg_tos_compat_loads", "kdbg_tos_module_screen",
        "kdbg_tos_screen_fallbacks", "kdbg_tos_adopted_module",
        "kdbg_loop_calls", "kdbg_uf_count")


def resolve_symbols():
    """Addresses of the TOS diagnostics, straight from the kernel in build/."""
    try:
        out = subprocess.run(["m68k-elf-nm", str(ELF)], capture_output=True, text=True)
    except FileNotFoundError:
        sys.exit("m68k-elf-nm not found: cannot resolve TOS diagnostics")
    if out.returncode != 0 or not out.stdout:
        sys.exit("cannot read %s (%s) - run 'gmake TOS=1 -j4 all' first"
                 % (ELF, (out.stderr or "").strip()))
    syms = {}
    for line in out.stdout.splitlines():
        parts = line.split()
        if len(parts) == 3 and parts[2] in WANT:
            syms[parts[2]] = int(parts[0], 16)
    missing = [w for w in WANT if w not in syms]
    if missing:
        sys.exit("kernel.elf has no %s - rebuild with TOS=1" % ", ".join(missing))
    return syms


EXTRA = [
    "--machine", "megast", "--cpulevel", "3", "--cpuclock", "32",
    "--memsize", "4", "--ttram", "64", "--tf536", "on",
    "--graphics-card", "vofa", "--gfx-chip", "et4000", "--mono",
    "--fast-forward", "off", "--ide-master", str(IMAGE),
]
for _i in range(8):
    EXTRA += ["--acsi", "%d=none" % _i]

failures = []


def check(name, cond, detail=""):
    print(("PASS  " if cond else "FAIL  ") + name + (("  " + detail) if detail else ""),
          flush=True)
    if not cond:
        failures.append(name)


def main():
    if not ELF.exists():
        sys.exit("missing %s - build the TOS=1 profile first" % ELF)
    if not IMAGE.exists():
        sys.exit("missing image %s (override with ATOS_MOD_IMG=...)" % IMAGE)
    syms = resolve_symbols()
    print("symbols:", {k: hex(v) for k, v in syms.items()}, flush=True)
    print("image:", IMAGE, "mode:", "missing-module" if MISSING else "module", flush=True)

    h = Hatari(str(PROJECT), log=str(PROJECT / "work" / "h-tosmodule.log"),
               window=False, image=str(IMAGE), binary=BINARY, cfg=True,
               extra=EXTRA, render=True)

    def ctext():
        lines = h.console_lines(rows=25)
        return "\n".join(lines) if lines else ""

    def u32(addr):
        b = h.read_bytes(addr, 4, settle=0.6) or b""
        return int.from_bytes(b, "big") if len(b) == 4 else None

    def diag():
        return {k: u32(v) for k, v in syms.items()}

    def marks():
        b = h.read_bytes(MARK_MASK, 16, settle=0.8) or b""
        if len(b) < 16:
            return None
        return {"mask": int.from_bytes(b[0:4], "big"),
                "start": int.from_bytes(b[4:8], "big"),
                "done": int.from_bytes(b[8:12], "big"),
                "run": int.from_bytes(b[12:16], "big")}

    def submit(text, wait):
        h.type(text)
        time.sleep(0.4)
        h.press("Return")
        time.sleep(wait)

    completed = 0
    try:
        # ---- readiness -------------------------------------------------------
        # The shell banner is NOT a usable signal: the text console is covered by
        # the boot board and the reader only sees the kernel's later notices
        # (checking for it here made the old script continue with booted=False).
        # Wait for the kernel MAIN LOOP to run instead - kdbg_loop_calls comes
        # from build/system/kernel.elf, so it cannot drift - then let the shell
        # settle.  A run that never reaches the loop still fails loudly.
        t0 = time.time()
        booted = False
        while time.time() - t0 < BOOT_TIMEOUT_S:
            time.sleep(2.0)
            lc = u32(syms["kdbg_loop_calls"])
            if lc:
                booted = True
                break
        check("kernel main loop reached within %.0fs" % BOOT_TIMEOUT_S, booted,
              "console tail: %r" % ctext()[-80:])
        if not booted:
            print("harness cannot start: the kernel never reached its loop", flush=True)
            return 2
        time.sleep(12.0)                      # let the shell settle
        print("baseline:", diag(), flush=True)

        # ---- two launches, each proven fresh by the run counter ---------------
        for run in (1, RUNS):
            before = marks()
            submit("run \\SYSTEM\\TEST\\TOSPROBE.PRG", 2.0)
            started = None
            t0 = time.time()
            while time.time() - t0 < RUN_TIMEOUT_S:
                m = marks()
                if m and m["start"] == START_SENTINEL and \
                   (before is None or m["run"] != before["run"]):
                    started = m
                    break
                time.sleep(1.0)
            check("run%d started (fresh counter %s)"
                  % (run, "?" if before is None else hex(before["run"])),
                  started is not None,
                  "marks=%s" % (started if started else marks()))
            if started is None:
                continue

            finished = None
            t0 = time.time()
            while time.time() - t0 < RUN_TIMEOUT_S:
                m = marks()
                if m and m["done"] in (DONE_PASS, DONE_FAIL):
                    finished = m
                    break
                time.sleep(1.0)
            check("run%d completed" % run, finished is not None,
                  "marks=%s" % (finished if finished else marks()))
            if finished is None:
                continue
            completed += 1
            check("run%d TOSPROBE all pass" % run,
                  finished["done"] == DONE_PASS and finished["mask"] == 0,
                  "mask=0x%X done=0x%X" % (finished["mask"], finished["done"]))

        final = diag()
        print("final:", final, flush=True)
        module_screen = final.get("kdbg_tos_module_screen") or 0
        loads = final.get("kdbg_tos_compat_loads")
        adopted = final.get("kdbg_tos_adopted_module")
        fallbacks = final.get("kdbg_tos_screen_fallbacks")
        check("both runs completed", completed == RUNS, "completed=%d" % completed)
        check("no user-process faults during the runs",
              (final.get("kdbg_uf_count") or 0) == 0,
              "kdbg_uf_count=%s" % final.get("kdbg_uf_count"))
        if MISSING:
            check("module absent: no load counted", loads == 0, "loads=%s" % loads)
            check("module absent: nothing adopted", adopted == 0, "adopted=%s" % adopted)
            check("module absent: fallback used per run",
                  fallbacks is not None and fallbacks >= completed,
                  "fallbacks=%s runs=%d" % (fallbacks, completed))
        else:
            check("module loaded exactly once", loads == 1, "loads=%s" % loads)
            check("module screen published", bool(module_screen),
                  "module_screen=%s" % (hex(module_screen) if module_screen else None))
            check("every run adopted the module buffer", adopted == completed,
                  "adopted=%s runs=%d" % (adopted, completed))
            check("no silent fallback allocation", fallbacks == 0,
                  "fallbacks=%s" % fallbacks)
    finally:
        try:
            print("rc", h.close(), flush=True)
        except Exception as exc:                     # pragma: no cover - teardown
            print("close failed:", exc, flush=True)

    if failures:
        print("FAILURES: %d -> %s" % (len(failures), ", ".join(failures)), flush=True)
        return 1
    print("ALL CHECKS PASS", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
