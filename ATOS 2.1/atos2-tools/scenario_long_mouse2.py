#!/usr/bin/env python3
"""Punkt 1, proba 2: dlugi stress z fizyczna mysza, odporny na desynchronizacje
sesji. Przed kazdym krokiem sprawdza ksessions_current (0xa3bd4) i koryguje
Shift+Alt+F10; przed komenda wysyla keyup dla uzywanych klawiszy, zeby zdjety
'zawieszony' klawisz po stracie break."""
import os, sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari

P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
IMG = os.environ.get("ATOS_IMG", P + "/work/atos-v2.img")
OUT = "/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/mouse2"
CYCLES = int(sys.argv[1]) if len(sys.argv) > 1 else 30
CURRENT = "0x00043e82"
KEYS = [0x17, 0x31, 0x19, 0x16, 0x14, 0x1F, 0x1E, 0x1C]  # i n p u t s a enter

h = Hatari(P, log=P + "/work/hatari-mouse2.log", window=True, image=IMG)
os.makedirs(OUT, exist_ok=True)
t0 = time.monotonic()
def stamp(m): print(f"[{time.monotonic()-t0:7.1f}s] {m}", flush=True)
def cur(): return h.read_u32(CURRENT, settle=0.35)
def goto(target, tries=4):
    for _ in range(tries):
        c = cur()
        if c == target:
            return True
        h.combo(["Shift", "Alt", "F10"], delay=0.15)
        h.wait(0.9)
    return cur() == target
def clear_keys():
    for code in KEYS:
        h.cmd(f"hatari-event keyup {code}")
    h.wait(0.12)
def say(text):
    clear_keys()
    h.type(text, delay=0.11)
    h.keydown("Return", delay=0.05); h.wait(0.12); h.keyup("Return", delay=0.05)
    h.wait(0.8)
def shot(n):
    ok = h.screenshot(f"{OUT}/{n}.png"); stamp(f"shot {n} {ok}")

try:
    stamp("image " + IMG)
    h.wait(7)
    say("memory"); shot("L0-baseline")
    say("tts2"); h.wait(0.6)
    say("run sysinfo.app"); h.wait(4.0)
    h.press("F5", delay=0.1); h.wait(3.0)
    goto(0)
    say("tts3"); h.wait(0.6)
    say("run command.app"); h.wait(3.5)
    stamp(f"setup done, current={cur()} - ruszaj mysza")
    for i in range(1, CYCLES + 1):
        got = goto(0)
        say("inputstat")
        c0 = cur()
        if i % 5 == 0:
            shot(f"L{i:02d}-inputstat-cur{c0}")
        if not got:
            stamp(f"cycle {i}: nie udalo sie wrocic do tts1 (cur={c0})")
        goto(1); h.press("F5", delay=0.1); h.wait(2.6)
        goto(2)
        for k in ("Down", "Down", "Up", "Tab", "KP+"):
            h.press(k, delay=0.12)
        h.mouse("doubleclick", delay=0.2)
        h.mouse("rightdown", delay=0.15); h.mouse("rightup", delay=0.15)
        stamp(f"cycle {i}/{CYCLES}")
    goto(0)
    say("inputstat"); shot("L-final-inputstat")
    say("inputstat"); shot("L-final-inputstat2")
    say("memory"); shot("L-final-memory")
finally:
    stamp("closing")
    print("hatari rc=", h.close())
