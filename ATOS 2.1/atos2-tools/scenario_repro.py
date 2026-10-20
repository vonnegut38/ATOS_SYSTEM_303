#!/usr/bin/env python3
import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari

P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
OUT = "/Users/xxx/Documents/Deepseek_test/atos2-tools/shots"
h = Hatari(P, log=P + "/work/hatari-repro.log", window=True)

def shot(name):
    ok = h.screenshot(f"{OUT}/{name}.png")
    print(name, ok, flush=True)

try:
    h.wait(7)
    shot("r0-boot")
    h.type("memory", delay=0.10); h.press("\n", delay=0.10); h.wait(0.8); shot("r1-memory-before")
    h.type("inputstat", delay=0.10); h.press("\n", delay=0.10); h.wait(0.8); shot("r2-inputstat-before")

    h.type("tts2", delay=0.10); h.press("\n", delay=0.10); h.wait(1.0)
    h.type("run sysinfo.app", delay=0.10); h.press("\n", delay=0.10)
    h.wait(5.0); shot("r3-sysinfo-full")

    h.press("F5", delay=0.10); h.wait(4.0); shot("r4-sysinfo-f5")

    h.combo(["Shift", "Alt", "F10"], delay=0.2); h.wait(2.5); shot("r5-back-tts1")

    h.type("inputstat", delay=0.12); h.press("\n", delay=0.12); h.wait(1.2); shot("r6-inputstat-after")
    h.type("memory", delay=0.10); h.press("\n", delay=0.10); h.wait(0.8); shot("r7-memory-after")
finally:
    print("hatari rc=", h.close())
