import sys, struct, subprocess, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
S = {}
out = subprocess.run(["m68k-elf-nm", P + "/build/system/kernel.elf"], capture_output=True, text=True).stdout
for line in out.splitlines():
    p = line.split()
    if len(p) == 3: S[p[2]] = int(p[0], 16)
h = Hatari(P, log=P + "/work/hatari-verify.log", window=True)
def u32(a):
    b = h.read_bytes(a, 4, settle=0.4)
    return struct.unpack(">I", b)[0] if b and len(b) == 4 else None
try:
    h.wait(23)
    print("uf_count", u32(S["kdbg_uf_count"]), "end_pid", u32(S["kdbg_end_pid"]),
          "end_status", u32(S["kdbg_end_status"]), "rel_end", u32(S["kdbg_rel_end"]), flush=True)
    W = S["wm"]
    n = u32(W + 8 * 60) or 0
    print("wm.count", n, flush=True)
    # wm_window_t: frame(x,y,w,h)=8, client(8), flags(2), src_x,src_y(4), content_h(2)
    for i in range(min(n, 4)):
        base = W + i * 60
        raw = h.read_bytes(base, 60, settle=0.4)
        if raw and len(raw) == 60:
            fx, fy, fw, fh = struct.unpack(">hhHH", raw[0:8])
            cx, cy, cw, ch = struct.unpack(">hhHH", raw[8:16])
            flags = struct.unpack(">H", raw[16:18])[0]
            sx, sy = struct.unpack(">hh", raw[18:22])
            chh = struct.unpack(">H", raw[22:24])[0]
            print("  win%d frame=(%d,%d,%d,%d) client=(%d,%d,%d,%d) flags=%#x src=(%d,%d) content_h=%d"
                  % (i, fx, fy, fw, fh, cx, cy, cw, ch, flags, sx, sy, chh), flush=True)
    time.sleep(3)
    print("late rel_end", u32(S["kdbg_rel_end"]), "uf_count", u32(S["kdbg_uf_count"]),
          "wm.count", u32(W + 8 * 60), flush=True)
finally:
    print("rc", h.close(), flush=True)
