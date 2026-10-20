import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
h = Hatari(P, log=P + "/work/hatari-sessval.log", image=P + "/work/atos-v2.img")
try:
    h.wait(7)
    print("current at boot:", h.read_u32("0xa3bd4"), flush=True)
    h.type("tts2", delay=0.09); h.press("\n", delay=0.09); h.wait(1.0)
    print("after tts2:", h.read_u32("0xa3bd4"), flush=True)
    h.type("tts3", delay=0.09); h.press("\n", delay=0.09); h.wait(1.0)
    print("after tts3:", h.read_u32("0xa3bd4"), flush=True)
    h.combo(["Shift", "Alt", "F10"], delay=0.15); h.wait(1.0)
    print("after one combo:", h.read_u32("0xa3bd4"), flush=True)
finally:
    print("rc", h.close())
