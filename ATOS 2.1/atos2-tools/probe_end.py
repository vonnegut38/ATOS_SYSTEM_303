import sys, struct, time, subprocess
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"

def syms():
    out = subprocess.run(["m68k-elf-nm", P + "/build/system/kernel.elf"],
                         capture_output=True, text=True).stdout
    d = {}
    for line in out.splitlines():
        p = line.split()
        if len(p) == 3:
            d[p[2]] = int(p[0], 16)
    return d

S = syms()
def A(name):
    return S.get(name)

WM = A("wm"); STRIDE = 58
DBG = {k: A(k) for k in ("wm_dbg_press","wm_dbg_drag_start","wm_dbg_drag_move",
                          "wm_dbg_drag_end","wm_dbg_rsz_start") if A(k)}
END = {k: A(k) for k in ("kdbg_end_pid","kdbg_end_term","kdbg_end_name","kdbg_end_status",
                          "kdbg_rel_calls","kdbg_rel_clear","kdbg_rel_pid","kdbg_rel_slot",
                          "kdbg_rel_kill","kdbg_rel_end","kdbg_kbpid","kdbg_term","kdbg_cur",
                          "kdbg_wake_calls","kdbg_wake_keys","kdbg_route_gui",
                          "kdbg_route_shell") if A(k)}
print("addrs wm=%#x" % WM, {k: hex(v) for k, v in DBG.items()},
      {k: hex(v) for k, v in END.items()}, flush=True)
h = Hatari(P, log=P + "/work/hatari-end.log", window=True)

def rd(a, n):
    return h.read_bytes(a, n, settle=0.25)
def u32(a):
    b = rd(a, 4)
    return struct.unpack(">I", b)[0] if b and len(b) == 4 else None
def s32(a):
    b = rd(a, 4)
    return struct.unpack(">i", b)[0] if b and len(b) == 4 else None
def cnt():
    return u32(WM + 8 * 58) or 0
def rects():
    out = []
    n = min(cnt(), 8)
    for i in range(n):
        b = rd(WM + i * STRIDE, 8)
        if b and len(b) == 8:
            out.append(struct.unpack(">hhHH", b))
    return out
def dbg():
    return {k: u32(v) for k, v in DBG.items()}
def end():
    r = {k: u32(v) for k, v in END.items()}
    n = r.get("kdbg_end_name")
    if n is not None:
        r["name"] = "".join(chr((n >> sh) & 0xff) for sh in (24, 16, 8, 0))
    st = r.get("kdbg_end_status")
    if st is not None and st > 0x7fffffff:
        r["status"] = st - 0x100000000
    return r

try:
    h.wait(22)
    print("boot", cnt(), rects(), flush=True)
    for a in range(3):
        if cnt() >= 2: break
        h.press("F10", delay=0.5); time.sleep(1.2)
        h.press("Down", delay=0.4); time.sleep(0.8)
        h.press("Return", delay=0.5); time.sleep(1.2)
        h.type("WINDEMO", delay=0.3); time.sleep(1.2)
        h.press("Return", delay=0.5); time.sleep(5.0)
    print("launched", cnt(), rects(), end(), dbg(), flush=True)
    h.press("F1", delay=0.6); time.sleep(6.0)
    print("after F1", cnt(), rects(), end(), dbg(), flush=True)
    h.press("F2", delay=0.6); time.sleep(6.0)
    print("after F2", cnt(), rects(), end(), dbg(), flush=True)
    h.press("F3", delay=0.6); time.sleep(3.0)
    print("after F3", cnt(), rects(), end(), dbg(), flush=True)
finally:
    print("rc", h.close(), flush=True)
