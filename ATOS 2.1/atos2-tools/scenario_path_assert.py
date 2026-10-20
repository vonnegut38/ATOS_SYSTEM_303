"""Plan item 2: assertion tests for hostile PATH and process-memory reuse.

Unlike the older screenshot-only scenarios this one:
  * verifies the emulated ACIA actually delivered the typed line (type_checked),
  * fails on any kernel panic marker on the visible console,
  * checks that a builtin still works after a failed process start (the old
    PROCESS START FAILED / -2 thread).
Exit code is non-zero if any assertion fails.
"""
import os, sys, threading, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari

def _watchdog(sec):
    time.sleep(sec)
    print("WATCHDOG after %ss" % sec, flush=True)
    os._exit(99)

threading.Thread(target=_watchdog, args=(180,), daemon=True).start()

P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
HB = "/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536"
IMG = P + "/work/atos-v2.img"
OUT = "/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/assert"
NOACSI = []
for i in range(8):
    NOACSI += ["--acsi", "%d=none" % i]
EXTRA = ["--machine","megast","--cpulevel","3","--cpuclock","32","--memsize","4",
         "--ttram","64","--tf536","on","--graphics-card","vofa","--gfx-chip","et4000",
         "--mono","--ide-master",IMG] + NOACSI

PANIC = ("KERNEL PANIC", "SYSTEM EXCEPTION", "ILLEGAL INSTRUCTION",
         "BUS ERROR", "ADDRESS ERROR", "DOUBLE FAULT")

failures = []

def screen(h):
    lines = h.console_lines(rows=25) or []
    return "\n".join(lines).upper(), lines

def check(h, label, command, expect=None, wait=4.0):
    typed = h.type_checked(command, tries=4)
    h.press("\r", 0.2)
    time.sleep(wait)
    text, lines = screen(h)
    h.screenshot(OUT + "/" + label + ".png", settle=0.3)
    panics = [p for p in PANIC if p in text]
    has_expect = (expect is None) or (expect.upper() in text)
    ok = typed and not panics and has_expect
    print("ASSERT %-10s typed=%-5s panic=%-5s expect(%s)=%-5s -> %s"
          % (label, typed, bool(panics), expect, has_expect, "PASS" if ok else "FAIL"),
          flush=True)
    for ln in lines[-3:]:
        print("     |" + ln, flush=True)
    if not ok:
        failures.append(label)

h = Hatari(P, log=P + "/work/hatari-pathassert.log", window=True, image=IMG,
           binary=HB, cfg=True, extra=EXTRA)
try:
    time.sleep(30)
    check(h, "1-path", "path", None)                 # builtin before the sequence
    check(h, "2-badpath", "path \\NOPE", None, wait=6.0)  # hostile PATH
    check(h, "3-path2", "path", None)                # builtin again (reuse thread)
    check(h, "4-memory", "memory", "ST-RAM")         # builtin after failed process
    check(h, "5-nosuch", "run NOSUCH.APP", None)     # clean failure
finally:
    print("FAILURES:", failures if failures else "none", flush=True)
    print("rc", h.close(), flush=True)
sys.exit(1 if failures else 0)
