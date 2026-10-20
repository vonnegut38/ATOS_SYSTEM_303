#!/usr/bin/env python3
import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
OUT = "/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/vofa"
h = Hatari(P, log=P + "/work/hatari-vofa-boot.log", window=True)
try:
    h.wait(8)
    print("boot", h.screenshot(OUT + "/v0-boot.png"), flush=True)
    h.type("memory", delay=0.09); h.press("\n", delay=0.09); h.wait(0.8)
    h.type("inputstat", delay=0.09); h.press("\n", delay=0.09); h.wait(0.8)
    print("cmd", h.screenshot(OUT + "/v1-memory-inputstat.png"), flush=True)
finally:
    print("rc", h.close())
