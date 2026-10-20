import sys, struct, time, signal, subprocess
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
SH = "/Users/xxx/Documents/Deepseek_test/atos2-tools/shots"
signal.signal(signal.SIGALRM, lambda *a: (_ for _ in ()).throw(SystemExit("TIMEOUT")))
signal.alarm(300)
def syms():
    out = subprocess.run(["m68k-elf-nm", P + "/build/system/kernel.elf"], capture_output=True, text=True).stdout
    return {p[2]: int(p[0], 16) for p in (l.split() for l in out.splitlines()) if len(p) == 3}
S = syms()
def A(n): return S.get(n)
WM = A("wm"); STR = 58
DBG = {k: A(k) for k in ("wm_dbg_press","wm_dbg_phit","wm_dbg_drag_start","wm_dbg_drag_end","wm_dbg_rsz_start",
                          "wm_dbg_pmx","wm_dbg_pmy","wm_dbg_nx","wm_dbg_ny") if A(k)}
h = Hatari(P, log=P + "/work/hatari-rsz.log", window=True)
def rd(a, n): return h.read_bytes(a, n, settle=0.2)
def u32(a):
    b = rd(a, 4); return struct.unpack(">I", b)[0] if b and len(b) == 4 else None
def cnt(): return u32(WM + 8 * 58) or 0
def w1():
    b = rd(WM + STR, 8); return struct.unpack(">hhHH", b) if b and len(b) == 8 else None
def d(): return {k: (u32(v) if u32(v) is None or u32(v) < 0x80000000 else u32(v) - 0x100000000) for k, v in DBG.items()}
try:
    h.wait(22)
    for a in range(3):
        if cnt() >= 2: break
        h.press("F10", delay=0.5); time.sleep(1.2)
        h.press("Down", delay=0.4); time.sleep(0.8)
        h.press("Return", delay=0.5); time.sleep(1.2)
        h.type("WINDEMO", delay=0.3); time.sleep(1.2)
        h.press("Return", delay=0.5); time.sleep(4.0)
    print("launched", w1(), flush=True)
    h.press("F2", delay=0.6)
    for a in range(40):
        time.sleep(4.0)
        dd = d()
        print("t=%ds" % (4*(a+1)), w1(), "rsz=%s press=%s phit=%s pmx=%s pmy=%s nx=%s ny=%s" % (
            dd.get("wm_dbg_rsz_start"), dd.get("wm_dbg_press"), dd.get("wm_dbg_phit"),
            dd.get("wm_dbg_pmx"), dd.get("wm_dbg_pmy"), dd.get("wm_dbg_nx"), dd.get("wm_dbg_ny")), flush=True)
        if (dd.get("wm_dbg_rsz_start") or 0) > 0:
            time.sleep(30.0); break
    print("final", w1(), d(), flush=True)
    h.screenshot(SH + "/wm-04-resize-final.png", settle=0.6)
finally:
    try: print("rc", h.close(), flush=True)
    except Exception as e: print("close-err", e, flush=True)
