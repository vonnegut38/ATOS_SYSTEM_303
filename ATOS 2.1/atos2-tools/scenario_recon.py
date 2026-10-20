#!/usr/bin/env python3
import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari

P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
OUT = "/Users/xxx/Documents/Deepseek_test/atos2-tools/shots"
h = Hatari(P, log=P + "/work/hatari-recon.log", window=True)
try:
    h.wait(7)
    for i, cmd in enumerate(["maps", "memory", "inputstat", "tasks"]):
        h.type(cmd, delay=0.10)
        h.press("\n", delay=0.10)
        h.wait(0.9)
        ok = h.screenshot(f"{OUT}/recon-{i}-{cmd}.png")
        print(cmd, ok)
finally:
    print("hatari rc=", h.close())
