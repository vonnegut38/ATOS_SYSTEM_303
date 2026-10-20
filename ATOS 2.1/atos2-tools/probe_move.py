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
WM = A("wm"); STR = 58; FR = A("wm_frames"); PX = A("wm_damage_pixels")
h = Hatari(P, log=P + "/work/hatari-move.log", window=True)
def u32(a):
    b = h.read_bytes(a, 4, settle=0.2); return struct.unpack(">I", b)[0] if b and len(b) == 4 else None
def cnt(): return u32(WM + 8 * 58) or 0
def stat(): return (u32(FR), u32(PX))
try:
    h.wait(22)
    f0, p0 = stat()
    print("boot", f0, p0, flush=True)
    ok = False
    for k in range(10):
        h.press("F5", delay=0.6)
        for j in range(3):
            time.sleep(1.5)
            if (u32(FR) or 0) > f0:
                ok = True; break
        if ok: break
    print("F5 delivered" if ok else "F5 not delivered", flush=True)
    for k in range(6):
        time.sleep(5.0)
        f, p = stat()
        print("t=%ds frames=%d pixels=%d d_pixels=%d d_frames=%d px/frame=%s" % (
            5*(k+1), f, p, p - p0, f - f0, (p - p0)//max(1, f - f0)), flush=True)
    h.screenshot(SH + "/wm-move-desktop.png", settle=0.6)
finally:
    try: print("rc", h.close(), flush=True)
    except Exception as e: print("close-err", e, flush=True)
