import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
for run in range(4):
    h = Hatari(P, log=P + ("/work/hatari-rf%d.log"%run), window=True)
    try:
        h.wait(20)
        h.press("F10", delay=0.4); time.sleep(1.2)
        h.press("Down", delay=0.3); time.sleep(0.8)
        h.press("Return", delay=0.4); time.sleep(1.2)
        h.type("WINDEMO", delay=0.25); time.sleep(1.0)
        h.press("Return", delay=0.4); time.sleep(4.0)
        print("run%d"%run, h.screenshot("/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/rf-%d.png"%run, settle=0.6), flush=True)
    finally:
        h.close()
