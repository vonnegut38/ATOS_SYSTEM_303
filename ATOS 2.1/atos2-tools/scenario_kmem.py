import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
OUT = "/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/kmem"
h = Hatari(P, log=P + "/work/hatari-kmem.log", window=True)
try:
    h.wait(9)
    print("boot", h.screenshot(OUT + "/k0-boot.png"), flush=True)
finally:
    print("rc", h.close())
