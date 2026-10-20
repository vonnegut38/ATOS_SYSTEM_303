import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
h = Hatari(P, log=P + "/work/hatari-mw-dump.log", window=True)
def cmd(line, wait=2.5):
    h.type(line, delay=0.12); h.press("\n", delay=0.1); time.sleep(wait)
try:
    h.wait(15)
    cmd("run desktop.app", 7.0)
    cmd("start windemo.app", 4.0)
    sc = h.read_bytes(0x10000, 32000, settle=3.0)
    open(P + "/work/wm_screen.bin", "wb").write(sc)
    print("dumped", len(sc), flush=True)
finally:
    print("rc", h.close(), flush=True)
