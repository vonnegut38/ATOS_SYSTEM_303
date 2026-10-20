import sys, struct, time, signal, subprocess
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
SH = "/Users/xxx/Documents/Deepseek_test/atos2-tools/shots"
signal.signal(signal.SIGALRM, lambda *a: (_ for _ in ()).throw(SystemExit("TIMEOUT")))
signal.alarm(240)
def syms():
    out = subprocess.run(["m68k-elf-nm", P + "/build/system/kernel.elf"], capture_output=True, text=True).stdout
    return {p[2]: int(p[0], 16) for p in (l.split() for l in out.splitlines()) if len(p) == 3}
S = syms()
def A(n): return S.get(n)
WM = A("wm"); STR = 60
DBG = {k: A(k) for k in ("wm_dbg_drag_end","wm_dbg_drag_start","wm_dbg_rsz_start") if A(k)}
h = Hatari(P, log=P + "/work/hatari-wmwin.log", window=True)
def u32(a):
    b = h.read_bytes(a, 4, settle=0.2); return struct.unpack(">I", b)[0] if b and len(b) == 4 else None
def cnt(): return u32(WM + 8 * 60) or 0
def rects(hh):
    out = []
    for i in range(min(cnt(), 8)):
        b = hh.read_bytes(WM + i * STR, 50, settle=0.2)
        if b and len(b) >= 50:
            x, y, w, h = struct.unpack(">hhHH", b[0:8]); fl = b[20]
            t = b[26:50].split(b"\x00")[0].decode("latin1")
            out.append((x, y, w, h, fl, t))
    return out
try:
    h.wait(22); print("boot", cnt(), rects(h), flush=True)
    h.screenshot(SH + "/wm-10-desktop.png", settle=0.6)
    h.press("F5", delay=0.6)
    for k in range(10):
        time.sleep(1.4)
        print("live t=%d" % ((k+1)*14//10), rects(h), flush=True)
        h.screenshot("%s/wm-live-%d.png" % (SH, k), settle=0.3)
    time.sleep(2.0); print("after F5", cnt(), rects(h), flush=True)
    h.screenshot(SH + "/wm-11-moved.png", settle=0.6)
finally:
    try: print("rc", h.close(), flush=True)
    except Exception as e: print("close-err", e, flush=True)
