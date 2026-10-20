#!/usr/bin/env python3
"""Punkt 1 planu: dlugi stress na obrazie z poprawka SR, w oknie, z fizyczna
mysza uzytkownika. Automat robi sesje/SYSINFO(F5)/Commander/INPUTSTAT."""
import os, sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari

P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
IMG = os.environ.get("ATOS_IMG", P + "/work/atos-v2.img")
OUT = "/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/mouse"
CYCLES = int(sys.argv[1]) if len(sys.argv) > 1 else 36
h = Hatari(P, log=P + "/work/hatari-mouse.log", window=True, image=IMG)
os.makedirs(OUT, exist_ok=True)
t0 = time.monotonic()
def stamp(m): print(f"[{time.monotonic()-t0:7.1f}s] {m}", flush=True)
def sw(): h.combo(["Shift", "Alt", "F10"], delay=0.15)
def shot(n): ok = h.screenshot(f"{OUT}/{n}.png"); stamp(f"shot {n} {ok}")

try:
    stamp("image " + IMG)
    h.wait(7)
    h.type("memory", delay=0.09); h.press("\n", delay=0.09); h.wait(0.7)
    h.type("inputstat", delay=0.09); h.press("\n", delay=0.09); h.wait(0.7)
    shot("L0-baseline")
    h.type("tts2", delay=0.09); h.press("\n", delay=0.09); h.wait(0.8)
    h.type("run sysinfo.app", delay=0.09); h.press("\n", delay=0.09); h.wait(4.0)
    h.press("F5", delay=0.1); h.wait(3.0)
    sw(); h.wait(1.5)
    h.type("tts3", delay=0.09); h.press("\n", delay=0.09); h.wait(0.8)
    h.type("run command.app", delay=0.09); h.press("\n", delay=0.09); h.wait(3.5)
    stamp("setup done - mozna ruszac mysza")
    for i in range(1, CYCLES + 1):
        sw(); h.wait(1.6)
        h.type("inputstat", delay=0.11); h.press("\n", delay=0.11); h.wait(0.9)
        if i % 6 == 0:
            shot(f"L{i:02d}-inputstat")
        sw(); h.wait(1.1)
        h.press("F5", delay=0.1); h.wait(2.6)
        sw(); h.wait(1.1)
        for k in ("Down", "Down", "Up", "Tab", "KP+"):
            h.press(k, delay=0.12)
        h.mouse("doubleclick", delay=0.2)
        h.mouse("rightdown", delay=0.15); h.mouse("rightup", delay=0.15)
        stamp(f"cycle {i}/{CYCLES}")
    sw(); h.wait(1.6)
    h.type("inputstat", delay=0.12); h.press("\n", delay=0.12); h.wait(1.0)
    shot("L-final-inputstat")
    h.type("inputstat", delay=0.12); h.press("\n", delay=0.12); h.wait(1.0)
    shot("L-final-inputstat2")
    h.type("memory", delay=0.09); h.press("\n", delay=0.09); h.wait(0.8)
    shot("L-final-memory")
finally:
    stamp("closing")
    print("hatari rc=", h.close())
