import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
h = Hatari(P, log=P + "/work/hatari-ux.log", window=True)
def cmd(line, wait=2.5):
    h.type(line, delay=0.12); h.press("\n", delay=0.1); time.sleep(wait)
def screen():
    return "\n".join(h.console_lines(rows=25, settle=0.5) or [])
try:
    h.wait(15)
    for _ in range(3):
        cmd("run desktop.app", 7.0)
        if "PROCESS READY" in screen(): break
        time.sleep(1.0)
    cmd("start windemo.app", 4.0)
    print("shot:", h.screenshot("/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/desktop-ux.png", settle=0.6), flush=True)
    sc = h.read_bytes(0x10000, 32000, settle=3.0)
    open(P + "/work/wm_screen.bin", "wb").write(sc)
    print("dumped", len(sc), flush=True)
finally:
    print("rc", h.close(), flush=True)
