import sys, time, datetime
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
h = Hatari(P, log=P + "/work/hatari-rtc2.log", window=True, extra=["--machine","megast"])
try:
    print("host", datetime.datetime.now().strftime("%H:%M"), flush=True)
    h.wait(18)
    for a in range(4):
        h.type("start desktop.app", delay=0.12); h.press("\n", delay=0.1); h.wait(6.0)
        b=h.read_bytes(0x3d311,1,settle=0.25)
        if b and b[0]==1: break
    time.sleep(2.0)
    print("shot:",h.screenshot("/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/desktop-rtc-mega.png",settle=0.6),flush=True)
finally:
    print("rc",h.close(),flush=True)
