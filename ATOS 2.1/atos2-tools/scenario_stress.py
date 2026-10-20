#!/usr/bin/env python3
"""ATOS 2.0 input-loss stress: three sessions, SYSINFO + Commander, many cycles.

Session order (kernel_sessions.c): Shift+Alt+F10 moves to the next existing
session in circular order.  After setup: tts1 shell, tts2 SYSINFO, tts3 Commander,
so one combo from tts3 lands on tts1, then tts2, then tts3.
"""
import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari

P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
OUT = "/Users/xxx/Documents/Deepseek_test/atos2-tools/shots"
CYCLES = int(sys.argv[1]) if len(sys.argv) > 1 else 12

h = Hatari(P, log=P + "/work/hatari-stress.log", window=True)
t0 = time.monotonic()

def stamp(msg):
    print(f"[{time.monotonic()-t0:7.1f}s] {msg}", flush=True)

def shot(name):
    ok = h.screenshot(f"{OUT}/{name}.png")
    stamp(f"shot {name} {ok}")
    return ok

def sw():
    h.combo(["Shift", "Alt", "F10"], delay=0.15)

try:
    h.wait(7)
    shot("s0-boot")
    h.type("memory", delay=0.09); h.press("\n", delay=0.09); h.wait(0.7)
    h.type("inputstat", delay=0.09); h.press("\n", delay=0.09); h.wait(0.7)
    shot("s1-baseline")

    # tts2 + SYSINFO
    h.type("tts2", delay=0.09); h.press("\n", delay=0.09); h.wait(0.8)
    h.type("run sysinfo.app", delay=0.09); h.press("\n", delay=0.09); h.wait(4.0)
    h.press("F5", delay=0.1); h.wait(3.0)
    shot("s2-sysinfo-up")
    # back to tts1 shell, then tts3 + Commander
    sw(); h.wait(1.5)
    h.type("tts3", delay=0.09); h.press("\n", delay=0.09); h.wait(0.8)
    h.type("run command.app", delay=0.09); h.press("\n", delay=0.09); h.wait(3.5)
    shot("s3-commander-up")

    # Phase A: shell + SYSINFO + Commander
    for i in range(1, CYCLES + 1):
        sw(); h.wait(1.5)                      # -> tts1 shell (the guarded case)
        h.type("inputstat", delay=0.11); h.press("\n", delay=0.11); h.wait(0.8)
        if i in (2, 4, 8, 12):
            shot(f"sA{i}-tts1-inputstat")
        sw(); h.wait(1.0)                      # -> tts2 SYSINFO
        h.press("F5", delay=0.1); h.wait(2.8)
        sw(); h.wait(1.0)                      # -> tts3 Commander
        for k in ("Down", "Down", "Up", "Tab"):
            h.press(k, delay=0.12)
        h.mouse("doubleclick", delay=0.2)
        h.mouse("rightdown", delay=0.15); h.mouse("rightup", delay=0.15)
        stamp(f"cycle A{i} done")

    # Phase B: swap SYSINFO for a second Commander
    sw(); h.wait(1.2)                          # -> tts1
    sw(); h.wait(1.2)                          # -> tts2 (SYSINFO)
    h.press("F10", delay=0.15); h.wait(1.5)    # quit SYSINFO
    shot("s4-sysinfo-quit")
    h.type("run command.app", delay=0.09); h.press("\n", delay=0.09); h.wait(3.0)
    shot("s5-two-commanders")

    for i in range(1, CYCLES + 1):
        sw(); h.wait(1.2)                      # -> tts1
        h.type("inputstat", delay=0.11); h.press("\n", delay=0.11); h.wait(0.8)
        if i in (4, 8, 12):
            shot(f"sB{i}-tts1-inputstat")
        sw(); h.wait(1.0)                      # -> tts2 Commander
        for k in ("Down", "Down", "Tab", "Up", "KP+", "KP+"):
            h.press(k, delay=0.12)
        h.mouse("doubleclick", delay=0.2)
        sw(); h.wait(1.0)                      # -> tts3 Commander
        for k in ("Down", "Up", "Tab", "KP+"):
            h.press(k, delay=0.12)
        h.mouse("rightdown", delay=0.15); h.mouse("rightup", delay=0.15)
        stamp(f"cycle B{i} done")

    # Final reading from tts1
    sw(); h.wait(1.5)
    h.type("inputstat", delay=0.12); h.press("\n", delay=0.12); h.wait(1.0)
    shot("s6-final-inputstat")
    h.type("memory", delay=0.09); h.press("\n", delay=0.09); h.wait(0.8)
    shot("s7-final-memory")
finally:
    stamp("closing")
    print("hatari rc=", h.close())
