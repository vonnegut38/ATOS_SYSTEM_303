import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
h = Hatari(P, log=P + "/work/hatari-windows.log", window=True)
try:
    h.wait(15)
    h.type("run desktop.app", delay=0.12); h.press("\n", delay=0.1); h.wait(8.0)
    h.type("run windemo.app", delay=0.12); h.press("\n", delay=0.1); h.wait(7.0)
    print("shot:", h.screenshot("/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/desktop-appwindow.png", settle=0.6), flush=True)
    sc = h.read_bytes(0x10000, 32000, settle=3.0)
    open(P + "/work/wm_screen.bin", "wb").write(sc)
    for ln in (h.console_lines(rows=25, settle=0.5) or []): print("|" + ln, flush=True)
finally:
    print("rc", h.close(), flush=True)
