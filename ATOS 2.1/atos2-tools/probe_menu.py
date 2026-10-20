import sys, struct, time, signal, subprocess
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
SH = "/Users/xxx/Documents/Deepseek_test/atos2-tools/shots"
signal.signal(signal.SIGALRM, lambda *a: (_ for _ in ()).throw(SystemExit("TIMEOUT")))
signal.alarm(200)
def syms():
    out = subprocess.run(["m68k-elf-nm", P + "/build/system/kernel.elf"], capture_output=True, text=True).stdout
    return {p[2]: int(p[0], 16) for p in (l.split() for l in out.splitlines()) if len(p) == 3}
S = syms()
def A(n): return S.get(n)
WM = A("wm"); STR = 58
h = Hatari(P, log=P + "/work/hatari-menu.log", window=True)
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
try:
    h.wait(22)
    h.screenshot(SH + "/wm-menu-before.png", settle=0.6)
    print("before", cnt(), rects(), flush=True)
    for k in range(3):
        h.press("F4", delay=0.6)
        time.sleep(4.0)
        h.screenshot(SH + "/wm-menu-after%d.png" % k, settle=0.6)
        print("after F4 #%d" % k, cnt(), rects(), flush=True)
finally:
    try: print("rc", h.close(), flush=True)
    except Exception as e: print("close-err", e, flush=True)
