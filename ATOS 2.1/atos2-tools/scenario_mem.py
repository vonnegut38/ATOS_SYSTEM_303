#!/usr/bin/env python3
"""Memory balance: baseline, run+quit SYSINFO and Commander, read again."""
import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari

P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
OUT = "/Users/xxx/Documents/Deepseek_test/atos2-tools/shots"
h = Hatari(P, log=P + "/work/hatari-mem.log", window=True)
t0 = time.monotonic()
def stamp(m): print(f"[{time.monotonic()-t0:6.1f}s] {m}", flush=True)
def shot(n): ok = h.screenshot(f"{OUT}/{n}.png"); stamp(f"shot {n} {ok}")
def sw(): h.combo(["Shift", "Alt", "F10"], delay=0.15)
try:
    h.wait(7)
    h.type("memory", delay=0.09); h.press("\n", delay=0.09); h.wait(0.7)
    h.type("inputstat", delay=0.09); h.press("\n", delay=0.09); h.wait(0.7)
    shot("m0-baseline")
    h.type("tts2", delay=0.09); h.press("\n", delay=0.09); h.wait(0.8)
    h.type("run sysinfo.app", delay=0.09); h.press("\n", delay=0.09); h.wait(4.0)
    h.press("F5", delay=0.1); h.wait(2.5)
    h.press("F10", delay=0.15); h.wait(1.5)
    stamp("sysinfo quit")
    h.type("tts3", delay=0.09); h.press("\n", delay=0.09); h.wait(0.8)
    h.type("run command.app", delay=0.09); h.press("\n", delay=0.09); h.wait(3.0)
    for k in ("Down", "Down", "Tab", "KP+"):
        h.press(k, delay=0.12)
    h.press("F10", delay=0.15); h.wait(1.5)
    stamp("commander quit")
    sw(); h.wait(1.5)
    h.type("memory", delay=0.09); h.press("\n", delay=0.09); h.wait(0.7)
    h.type("inputstat", delay=0.09); h.press("\n", delay=0.09); h.wait(0.7)
    shot("m1-after-close")
finally:
    stamp("closing")
    print("hatari rc=", h.close())
