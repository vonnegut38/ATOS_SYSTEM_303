import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
OUT = "/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/panic"
h = Hatari(P, log=P + "/work/hatari-panic2.log", window=True)
try:
    h.wait(9)
    h.cmd("hatari-debug cpureg sr 0x2500"); time.sleep(0.3)
    h.cmd("hatari-debug cpureg pc 0x00f00000"); time.sleep(1.5)
    print("panic", h.screenshot(OUT + "/p2-panic.png"), flush=True)
finally:
    print("rc", h.close())
