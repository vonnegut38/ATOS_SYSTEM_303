import sys, struct, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
h = Hatari(P, log=P + "/work/hatari-clock2.log", window=True)
def u32(a):
    b = h.read_bytes(a,4,settle=0.25)
    return struct.unpack(">I", b)[0] if b and len(b)==4 else None
def u8(a):
    b = h.read_bytes(a,1,settle=0.25)
    return b[0] if b else None
try:
    h.wait(18)
    for attempt in range(4):
        h.type("start desktop.app", delay=0.12); h.press("\n", delay=0.1); h.wait(6.0)
        if u8(0x3d055)==1: break
    print("desktop_is_active", u8(0x3d055), "attempt", attempt, flush=True)
    t0=u32(0x47180); print("applet_draws t0", t0, flush=True)
    time.sleep(12.0)
    t1=u32(0x47180); print("applet_draws t1", t1, flush=True)
    print("shot:", h.screenshot("/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/desktop-clock.png", settle=0.6), flush=True)
except Exception as e:
    print("ERR", e, flush=True)
finally:
    print("rc", h.close(), flush=True)
