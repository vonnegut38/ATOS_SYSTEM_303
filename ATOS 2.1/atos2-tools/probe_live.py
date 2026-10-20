import sys, struct, time, subprocess
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
S = {}
out = subprocess.run(["m68k-elf-nm", P + "/build/system/kernel.elf"], capture_output=True, text=True).stdout
for line in out.splitlines():
    p = line.split()
    if len(p) == 3: S[p[2]] = int(p[0], 16)
REG = {k: S[k] for k in ("kdbg_autodrag","kdbg_wm_composes","wm_frames","wm_damage_pixels",
                         "kdbg_recompose_cycles","kdbg_deco_cycles","kdbg_cursor_cycles",
                         "kdbg_total_cycles","kdbg_loop_cycles","kdbg_loop_calls",
                         "kblit_calls","kblit_hw","kblit_cpu") if k in S}
h = Hatari(P, log=P + "/work/hatari-live.log", window=True)
def u32(a):
    b = h.read_bytes(a, 4, settle=0.1); return struct.unpack(">I", b)[0] if b and len(b) == 4 else None
snap = lambda: {k: (u32(v) or 0) for k, v in REG.items()}
try:
    h.wait(22)
    W = S["wm"]
    print("boot wm.count", u32(W + 8 * 60), "uf", u32(S["kdbg_uf_count"]), "rel_end", u32(S["kdbg_rel_end"]), flush=True)
    h.cmd("hatari-debug screenshot /tmp/atos-before.png"); time.sleep(2.0)
    started = False
    for k in range(8):
        h.press("F6", delay=0.5); time.sleep(0.7)
        if (u32(REG["kdbg_autodrag"]) or 0) > 0: started = True; break
    print("autodrag started" if started else "F6 not delivered", flush=True)
    a = snap(); t0 = time.time()
    while time.time() - t0 < 40.0:
        if (u32(REG["kdbg_autodrag"]) or 0) == 0: break
        time.sleep(0.05)
    t1 = time.time(); b = snap()
    d = {k: b[k] - a[k] for k in REG}
    print("autodrag took %.3fs" % (t1 - t0), flush=True)
    print("composes", d["kdbg_wm_composes"], "damage_px", d["wm_damage_pixels"],
          "blits", d["kblit_calls"], "hw", d["kblit_hw"], "cpu", d["kblit_cpu"], flush=True)
    c = max(1, d["kdbg_wm_composes"])
    for k in ("kdbg_total_cycles","kdbg_recompose_cycles","kdbg_deco_cycles","kdbg_cursor_cycles"):
        print("  %s: %.2f ms/compose" % (k, d[k] / 12.288 / c), flush=True)
    time.sleep(1.0)
    h.cmd("hatari-debug screenshot /tmp/atos-after.png"); time.sleep(2.0)
    print("uf", u32(S["kdbg_uf_count"]), "rel_end", u32(S["kdbg_rel_end"]), flush=True)
    raw = h.read_bytes(W + 60, 60, settle=0.5)
    if raw and len(raw) == 60:
        fx, fy, fw, fh = struct.unpack(">hhHH", raw[0:8])
        print("win1 frame", (fx, fy, fw, fh), flush=True)
finally:
    print("rc", h.close(), flush=True)
