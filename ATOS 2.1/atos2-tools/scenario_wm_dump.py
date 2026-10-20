import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
h = Hatari(P, log=P + "/work/hatari-wm-dump.log", window=True)
def u32(a):
    b = h.read_bytes(a, 4, settle=0.3)
    return int.from_bytes(b[:4], "big") if b and len(b) >= 4 else None
try:
    h.wait(15)
    h.type("run wmdemo.app", delay=0.12); h.press("\n", delay=0.1); h.wait(9.0)
    surf = [u32(0x45788 + i*4) for i in range(4)]
    print("surf ptrs:", [hex(x) if x else None for x in surf], flush=True)
    for name, a, n in [("surface_pitch",0x45786,2),("surface_w",0x45784,2),("surface_h",0x45782,2),
                       ("wm_bpp",0x45798,1),("wm_pitch",0x4579a,2),("wm_ready",0x457a0,1)]:
        print(name, h.read_bytes(a, n, settle=0.2), flush=True)
    app = surf[1]
    if app:
        sb = h.read_bytes(app, 32000, settle=3.0)
        sc = h.read_bytes(0x10000, 32000, settle=3.0)
        print("len", len(sb), len(sc), flush=True)
        open(P + "/work/wm_surface.bin", "wb").write(sb)
        open(P + "/work/wm_screen.bin", "wb").write(sc)
        n = min(len(sb), len(sc))
        diff = sum(1 for i in range(n) if sb[i] != sc[i])
        print("byte diffs", diff, "of", n, flush=True)
        # best horizontal byte shift of a row (80 bytes)
        srow = sb[0:80]; best = None
        for sh in range(80):
            m = sum(1 for i in range(80) if srow[i] == sc[(i+sh) % 80])
            if best is None or m > best[1]: best = (sh, m)
        print("best row shift", best, flush=True)
finally:
    print("rc", h.close(), flush=True)
