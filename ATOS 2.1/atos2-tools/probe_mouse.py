import sys, struct, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
h = Hatari(P, log=P + "/work/hatari-mouse.log", window=True)
def s32(a):
    b=h.read_bytes(a,4,settle=0.25); return struct.unpack(">i",b)[0] if b and len(b)==4 else None
try:
    h.wait(20)
    print("session mouse_x", s32(0x3af14), "mouse_y", s32(0x3af10), flush=True)
    print("wm_mouse_x", s32(0x47872), "wm_mouse_y", s32(0x4786e), flush=True)
finally:
    print("rc", h.close(), flush=True)
