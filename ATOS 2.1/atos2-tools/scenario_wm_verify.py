import sys
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
h = Hatari(P, log=P + "/work/hatari-wm-verify.log", window=True)
try:
    h.wait(15)
    h.type("run wmdemo.app", delay=0.12); h.press("\n", delay=0.1); h.wait(10.0)
    print("screenshot:", h.screenshot("/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/wm-verify.png", settle=0.6), flush=True)
    sc = h.read_bytes(0x10000, 32000, settle=3.0)
    open(P + "/work/wm_screen.bin", "wb").write(sc)
    print("screen bytes", len(sc), flush=True)
finally:
    print("rc", h.close(), flush=True)
