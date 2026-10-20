#!/usr/bin/env python3
"""Confirmation: shell + SYSINFO(F5) + Commander, N cycles, read INPUTSTAT in tts1."""
import os, sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari

P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.0"
OUT = "/Users/xxx/Documents/Deepseek_test/atos2-tools/shots"
CYCLES = int(sys.argv[1]) if len(sys.argv) > 1 else 16
IMG = os.environ.get("ATOS_IMG", P + "/work/atos-v2.img")
print("image:", IMG, flush=True)
h = Hatari(P, log=P + "/work/hatari-confirm.log", window=True, image=IMG)
t0 = time.monotonic()

def stamp(m): print(f"[{time.monotonic()-t0:7.1f}s] {m}", flush=True)
def shot(n): ok = h.screenshot(f"{OUT}/{n}.png"); stamp(f"shot {n} {ok}")
def sw(): h.combo(["Shift", "Alt", "F10"], delay=0.15)

try:
    h.wait(7)
    h.type("memory", delay=0.09); h.press("\n", delay=0.09); h.wait(0.7)
    h.type("inputstat", delay=0.09); h.press("\n", delay=0.09); h.wait(0.7)
    shot("c0-baseline")
    h.type("tts2", delay=0.09); h.press("\n", delay=0.09); h.wait(0.8)
    h.type("run sysinfo.app", delay=0.09); h.press("\n", delay=0.09); h.wait(4.0)
    h.press("F5", delay=0.1); h.wait(3.0)
    sw(); h.wait(1.5)
    h.type("tts3", delay=0.09); h.press("\n", delay=0.09); h.wait(0.8)
    h.type("run command.app", delay=0.09); h.press("\n", delay=0.09); h.wait(3.5)
    stamp("setup done")
    for i in range(1, CYCLES + 1):
        sw(); h.wait(1.5)
        h.type("inputstat", delay=0.11); h.press("\n", delay=0.11); h.wait(0.8)
        if i in (8, CYCLES):
            shot(f"c{i}-tts1-inputstat")
        sw(); h.wait(1.0)
        h.press("F5", delay=0.1); h.wait(2.8)
        sw(); h.wait(1.0)
        for k in ("Down", "Down", "Up", "Tab"):
            h.press(k, delay=0.12)
        h.mouse("doubleclick", delay=0.2)
        h.mouse("rightdown", delay=0.15); h.mouse("rightup", delay=0.15)
        stamp(f"cycle {i} done")
    sw(); h.wait(1.5)
    h.type("inputstat", delay=0.12); h.press("\n", delay=0.12); h.wait(1.0)
    shot("c-final-inputstat")
    h.type("inputstat", delay=0.12); h.press("\n", delay=0.12); h.wait(1.0)
    shot("c-final-inputstat2")
    h.type("memory", delay=0.09); h.press("\n", delay=0.09); h.wait(0.8)
    shot("c-final-memory")
finally:
    stamp("closing")
    print("hatari rc=", h.close())
