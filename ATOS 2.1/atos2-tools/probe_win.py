import sys, struct, time, signal, subprocess
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"

signal.signal(signal.SIGALRM, lambda *a: (_ for _ in ()).throw(SystemExit("TIMEOUT")))
signal.alarm(600)

def syms():
    out = subprocess.run(["m68k-elf-nm", P + "/build/system/kernel.elf"],
                         capture_output=True, text=True).stdout
    return {p[2]: int(p[0], 16) for p in (l.split() for l in out.splitlines()) if len(p) == 3}
S = syms()
def A(n): return S.get(n)
WM = A("wm"); STR = 58
DBG = {k: A(k) for k in ("wm_dbg_press","wm_dbg_drag_start","wm_dbg_drag_move","wm_dbg_drag_end",
                          "wm_dbg_rsz_start","wm_dbg_dx","wm_dbg_dy","wm_dbg_nx","wm_dbg_ny",
                          "wm_dbg_mx","wm_dbg_my","wm_dbg_mvx","wm_dbg_mvy","wm_dbg_mvwin","wm_dbg_rszw","wm_dbg_rszh",
                          "wm_dbg_pmx","wm_dbg_pmy","wm_dbg_phit",
                          "kdbg_inject_calls","kdbg_inject_xy","kdbg_inject_btn","kdbg_inject_ok",
                          "kdbg_real_mouse","kdbg_real_dxdy","kdbg_real_btn",
                          "kdbg_frames","wm_dbg_press_frame","kdbg_inject_ring_n",
                          "kdbg_sleep_calls","kdbg_sleep_park","kdbg_sleep_zero",
                          "kdbg_yield_calls","kdbg_yield_ok","kdbg_yield_bad",
                          "kdbg_kbpid","kdbg_term","kdbg_cur","kdbg_wake_keys",
                          "kdbg_route_gui","kdbg_route_shell","kdbg_end_pid","kdbg_rel_clear",
                          "kdbg_end_name","kdbg_end_status","kdbg_end_term","kdbg_end_pc","kdbg_end_image") if A(k)}
print("wm=%#x" % WM, {k: hex(v) for k, v in DBG.items()}, flush=True)
h = Hatari(P, log=P + "/work/hatari-win.log", window=True)
def rd(a, n): return h.read_bytes(a, n, settle=0.2)
def u32(a):
    b = rd(a, 4); return struct.unpack(">I", b)[0] if b and len(b) == 4 else None
def s32(a):
    b = rd(a, 4); return struct.unpack(">i", b)[0] if b and len(b) == 4 else None
def cnt(): return u32(WM + 8 * 58) or 0
def dump(tag):
    n = min(cnt(), 8); out = []
    for i in range(n):
        b = rd(WM + i * STR, 50)
        if b and len(b) >= 50:
            x, y, w, h = struct.unpack(">hhHH", b[0:8])
            cx, cy, cw, ch = struct.unpack(">hhHH", b[8:16])
            pid = struct.unpack(">I", b[16:20])[0]
            fl = b[20]
            t = b[26:50].split(b"\x00")[0].decode("latin1")
            out.append((x, y, w, h, cx, cy, cw, ch, hex(pid), fl, t))
    d = {k: u32(v) for k, v in DBG.items()}
    rn = (d.get("kdbg_inject_ring_n") or 0)
    ring = []
    for q in range(max(0, rn - 8), rn):
        idx = (q & 15) * 2
        f = u32(A("kdbg_inject_ring") + idx * 4)
        xy = u32(A("kdbg_inject_ring") + (idx + 1) * 4)
        ring.append((f, (xy >> 16), xy & 0xffff))
    print(tag, "cnt", n, out, d, "ring", ring, flush=True)
try:
    h.wait(22)
    dump("boot")
    for a in range(3):
        if cnt() >= 2: break
        h.press("F10", delay=0.5); time.sleep(1.2)
        h.press("Down", delay=0.4); time.sleep(0.8)
        h.press("Return", delay=0.5); time.sleep(1.2)
        h.type("WINDEMO", delay=0.3); time.sleep(1.2)
        h.press("Return", delay=0.5); time.sleep(4.0)
    dump("launched")
    h.press("F1", delay=0.6)
    for a in range(60):
        time.sleep(2.0)
        if (u32(DBG["wm_dbg_drag_end"]) or 0) > 0: break
    time.sleep(2.0)
    dump("afterF1")
    for a in range(60):
        h.press("F2", delay=0.6)
        time.sleep(2.0)
        if (u32(DBG["wm_dbg_rsz_start"]) or 0) > 0:
            time.sleep(45.0); break
    dump("afterF2")
finally:
    print("closing", flush=True)
    try: print("rc", h.close(), flush=True)
    except Exception as e: print("close-err", e, flush=True)
