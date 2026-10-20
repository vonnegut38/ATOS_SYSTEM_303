import sys, struct, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
h = Hatari(P, log=P + "/work/hatari-probe3.log", window=True)
def screen():
    return "\n".join(h.console_lines(rows=25, settle=0.5) or [])
try:
    h.wait(15)
    for _ in range(4):
        h.type("run desktop.app", delay=0.12); h.press("\n", delay=0.1); h.wait(7.0)
        if "PROCESS READY" in screen(): break
        time.sleep(0.5)
    print("desktop ready:", "PROCESS READY" in screen(), flush=True)
    h.type("start windemo.app", delay=0.12); h.press("\n", delay=0.1); h.wait(6.0)
    sc = h.read_bytes(0x10000, 32000, settle=2.0)
    open(P + "/work/wm_screen.bin", "wb").write(sc)
    print("shot:", h.screenshot("/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/desktop-appwindow.png", settle=0.6), flush=True)
    for ln in (h.console_lines(rows=25, settle=0.5) or []): print("|"+ln, flush=True)
except Exception as e:
    print("ERR", e, flush=True)
finally:
    print("rc", h.close(), flush=True)

