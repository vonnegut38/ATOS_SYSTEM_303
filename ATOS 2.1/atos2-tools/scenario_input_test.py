#!/usr/bin/env python3
import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari

P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
OUT = "/Users/xxx/Documents/Deepseek_test/atos2-tools/shots"
h = Hatari(P, log=P + "/work/hatari-inputtest.log", window=True)
try:
    h.wait(7)
    print("boot shot", h.screenshot(f"{OUT}/in-00-boot.png"))
    h.type("tts2", delay=0.10)
    h.press("\n", delay=0.10)
    h.wait(1.0)
    print("tts2 shot", h.screenshot(f"{OUT}/in-01-tts2.png"))
    h.type("inputstat", delay=0.10)
    h.press("\n", delay=0.10)
    h.wait(1.0)
    print("inputstat shot", h.screenshot(f"{OUT}/in-02-inputstat.png"))
finally:
    print("hatari rc=", h.close())
