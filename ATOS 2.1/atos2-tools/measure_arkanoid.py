#!/usr/bin/env python3
"""Measure Arkanoid frame/sim rate on the emulator.

Launches ARKANOID.APP through the desktop RUN dialog, then reads the counters
the game publishes at fixed scratch addresses (0x800 = rendered frames,
0x804 = sim steps) and computes FPS.  Screenshots are saved for the record.

Usage: python3 atos2-tools/measure_arkanoid.py <variant-name>
"""
import sys, time, os
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari

P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
HB = "/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536"
IMG = P + "/work/atos-v2.img"
OUT = P + "/work/arkanoid-measure"
os.makedirs(OUT, exist_ok=True)

variant = sys.argv[1] if len(sys.argv) > 1 else "current"
fast = (len(sys.argv) > 2 and sys.argv[2] == "fast")

N = []
for i in range(8):
    N += ["--acsi", "%d=none" % i]
E = ["--machine", "megast", "--cpulevel", "3", "--cpuclock", "32",
     "--memsize", "4", "--ttram", "64", "--tf536", "on",
     "--graphics-card", "vofa", "--gfx-chip", "et4000", "--mono",
     "--disable-video", "off", "--fast-forward", "on" if fast else "off",
     "--ide-master", IMG] + N

h = Hatari(P, log=P + "/work/hatari-ark-%s.log" % variant, window=True,
           image=IMG, binary=HB, cfg=False, extra=E)


def read32(addr):
    for _ in range(4):
        v = h.read_u32(addr)
        if v is not None:
            return v
        time.sleep(0.4)
    return None


try:
    time.sleep(38)                       # cold boot to the desktop
    # Desktop FILE menu -> RUN... -> type ARKANOID -> Return (retry if flaky).
    for attempt in range(4):
        h.press('F4', delay=0.2); time.sleep(0.7)
        h.press('Down', delay=0.2); h.press('Return', delay=0.2); time.sleep(0.7)
        h.type('ARKANOID', delay=0.12); time.sleep(0.4); h.press('Return', delay=0.2)
        time.sleep(5)
        f = read32(0x800)
        print("%s launch attempt %d: frames=%s" % (variant, attempt, f), flush=True)
        if f is not None and f > 0:
            break
    time.sleep(2)                        # settle
    samples = []
    for i in range(8):
        time.sleep(3)
        f = read32(0x800); s = read32(0x804)
        if f is None or s is None:
            print("%s sample %d: READ FAILED" % (variant, i), flush=True)
            continue
        samples.append((time.monotonic(), f, s))
        print("%s sample %d: frames=%d sim=%d" % (variant, i, f, s), flush=True)
        h.screenshot(OUT + "/%s-%d.png" % (variant, i), settle=0.4)
    # Per-pair rate and overall rate (first -> last non-frozen sample).
    print("%s per-pair fps:" % variant, flush=True)
    best = 0.0
    for i in range(1, len(samples)):
        t0, f0, _ = samples[i - 1]
        t1, f1, _ = samples[i]
        dt = t1 - t0
        if dt > 0 and f1 > f0:
            fps = (f1 - f0) / dt
            best = max(best, fps)
            print("  pair %d: %.1f fps" % (i, fps), flush=True)
        else:
            print("  pair %d: frozen (game over / paused)" % i, flush=True)
    t0, f0, s0 = samples[0]
    t1, f1, s1 = samples[-1]
    dt = t1 - t0
    print("%s OVERALL: frames %d->%d (%+.1f fps), sim %d->%d (%+.1f sim/s) over %.1fs"
          % (variant, f0, f1, (f1 - f0) / dt if dt else 0,
             s0, s1, (s1 - s0) / dt if dt else 0, dt), flush=True)
finally:
    print("rc", h.close(), flush=True)
