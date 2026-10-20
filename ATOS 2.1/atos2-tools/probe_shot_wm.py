import sys, struct, time, signal, subprocess
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
SH = "/Users/xxx/Documents/Deepseek_test/atos2-tools/shots"

signal.signal(signal.SIGALRM, lambda *a: (_ for _ in ()).throw(SystemExit("TIMEOUT")))
signal.alarm(420)

def syms():
    out = subprocess.run(["m68k-elf-nm", P + "/build/system/kernel.elf"], capture_output=True, text=True).stdout
    return {p[2]: int(p[0], 16) for p in (l.split() for l in out.splitlines()) if len(p) == 3}
S = syms()
def A(n): return S.get(n)
WM = A("wm"); STR = 58
DBG = {k: A(k) for k in ("wm_dbg_press","wm_dbg_phit","wm_dbg_drag_start","wm_dbg_drag_end","wm_dbg_rsz_start") if A(k)}
print("wm=%#x" % WM, flush=True)
h = Hatari(P, log=P + "/work/hatari-shot.log", window=True)
def rd(a, n): return h.read_bytes(a, n, settle=0.2)
def u32(a):
    b = rd(a, 4); return struct.unpack(">I", b)[0] if b and len(b) == 4 else None
def cnt(): return u32(WM + 8 * 58) or 0
def rects():
    out = []
    for i in range(min(cnt(), 8)):
        b = rd(WM + i * STR, 8)
        if b and len(b) == 8: out.append(struct.unpack(">hhHH", b))
    return out
def shot(name):
    ok = h.screenshot("%s/%s.png" % (SH, name), settle=0.6)
    print("shot", name, ok, rects(), flush=True)
def run_script(key, field, live, wait_s):
    """Press key, grab a few live shots, then poll until field>0."""
    h.press(key, delay=0.6)
    for k in range(3):
        time.sleep(1.2)
        h.screenshot("%s/wm-%s-%d.png" % (SH, live, k), settle=0.4)
        if (u32(DBG[field]) or 0) > 0: break
    end = time.time() + wait_s
    while time.time() < end:
        if (u32(DBG[field]) or 0) > 0: return True
        time.sleep(2.0)
    return (u32(DBG[field]) or 0) > 0
try:
    h.wait(22)
    for a in range(3):
        if cnt() >= 2: break
        h.press("F10", delay=0.5); time.sleep(1.2)
        h.press("Down", delay=0.4); time.sleep(0.8)
        h.press("Return", delay=0.5); time.sleep(1.2)
        h.type("WINDEMO", delay=0.3); time.sleep(1.2)
        h.press("Return", delay=0.5); time.sleep(4.0)
    shot("wm-01-launch")
    print("F1...", flush=True)
    if not run_script("F1", "wm_dbg_drag_end", "drag-mid", 60):
        print("F1 retry", flush=True); run_script("F1", "wm_dbg_drag_end", "drag-mid2", 60)
    time.sleep(2.0); shot("wm-02-after-drag")
    print("F2...", flush=True)
    if not run_script("F2", "wm_dbg_rsz_start", "rsz-mid", 60):
        print("F2 retry", flush=True); run_script("F2", "wm_dbg_rsz_start", "rsz-mid2", 60)
    time.sleep(150.0); shot("wm-03-after-resize")
finally:
    print("closing", flush=True)
    try: print("rc", h.close(), flush=True)
    except Exception as e: print("close-err", e, flush=True)
