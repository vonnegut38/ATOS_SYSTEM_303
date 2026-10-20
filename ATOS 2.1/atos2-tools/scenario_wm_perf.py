import sys, time, struct
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
h = Hatari(P, log=P + "/work/hatari-wm-perf.log", window=True)
def u32(a):
    b = h.read_bytes(a, 4, settle=0.4)
    return int.from_bytes(b[:4], "big") if b and len(b) >= 4 else None
try:
    h.wait(15)
    h.type("run windemo.app", delay=0.12); h.press("\n", delay=0.1)
    time.sleep(8.0)
    f1 = u32(0x46462); d1 = u32(0x4645e)
    t0 = time.time()
    time.sleep(3.0)
    f2 = u32(0x46462); d2 = u32(0x4645e)
    dt = time.time() - t0
    print("wm_frames", f1, "->", f2, "delta", (f2 or 0)-(f1 or 0), "in %.1fs" % dt, flush=True)
    print("wm_damage_pixels", d1, "->", d2, "delta", (d2 or 0)-(d1 or 0), flush=True)
    print("frames/s", ((f2 or 0)-(f1 or 0))/dt, flush=True)
finally:
    print("rc", h.close(), flush=True)
