import sys, struct, subprocess, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
S = {}
out = subprocess.run(["m68k-elf-nm", P + "/build/system/kernel.elf"], capture_output=True, text=True).stdout
for line in out.splitlines():
    p = line.split()
    if len(p) == 3: S[p[2]] = int(p[0], 16)
h = Hatari(P, log=P + "/work/hatari-verify2.log", window=True)
def u32(a):
    b = h.read_bytes(a, 4, settle=0.4)
    return struct.unpack(">I", b)[0] if b and len(b) == 4 else None
try:
    h.wait(23)
    W = S["wm"]
    n = u32(W + 8 * 60) or 0
    print("wm.count", n, "uf_count", u32(S["kdbg_uf_count"]), "rel_end", u32(S["kdbg_rel_end"]), flush=True)
    for i in range(min(n, 4)):
        raw = h.read_bytes(W + i * 60, 60, settle=0.4)
        if raw and len(raw) == 60:
            fx, fy, fw, fh = struct.unpack(">hhHH", raw[0:8])
            cx, cy, cw, ch = struct.unpack(">hhHH", raw[8:16])
            pid = struct.unpack(">I", raw[16:20])[0]
            flags = raw[20]
            sx, sy = struct.unpack(">hh", raw[22:26])
            chh = struct.unpack(">H", raw[26:28])[0]
            title = raw[28:52].split(b"\x00")[0].decode("latin1")
            print("  win%d frame=(%d,%d,%d,%d) client=(%d,%d,%d,%d) pid=%#x flags=%#x src=(%d,%d) content_h=%d title=%r"
                  % (i, fx, fy, fw, fh, cx, cy, cw, ch, pid, flags, sx, sy, chh, title), flush=True)
    ok = h.screenshot(P + "/work/desktop-live.png", settle=2.5)
    print("screenshot", ok, flush=True)
finally:
    print("rc", h.close(), flush=True)
