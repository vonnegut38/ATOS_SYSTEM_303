import sys, struct, time, signal, subprocess
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
signal.signal(signal.SIGALRM, lambda *a: (_ for _ in ()).throw(SystemExit("TIMEOUT")))
signal.alarm(300)
def syms():
    out = subprocess.run(["m68k-elf-nm", P + "/build/system/kernel.elf"], capture_output=True, text=True).stdout
    return {p[2]: int(p[0], 16) for p in (l.split() for l in out.splitlines()) if len(p) == 3}
S = syms()
def A(n): return S.get(n)
KEYS = ("kdbg_wm_calls","kdbg_wm_composes","kdbg_outline_draws","kdbg_deco_draws","wm_frames","wm_damage_pixels")
REG = {k: A(k) for k in KEYS if A(k)}
print({k: hex(v) for k, v in REG.items()}, flush=True)
h = Hatari(P, log=P + "/work/hatari-rate.log", window=True)
def u32(a):
    b = h.read_bytes(a, 4, settle=0.15); return struct.unpack(">I", b)[0] if b and len(b) == 4 else None
def snap(): return {k: (u32(v) or 0) for k, v in REG.items()}
def show(tag, a, b, dt):
    print(tag, "dt=%.1fs" % dt, {k: "%d (%.0f/s)" % (b[k]-a[k], (b[k]-a[k])/dt) for k in REG}, flush=True)
try:
    h.wait(22)
    a = snap(); time.sleep(10.0); b = snap(); show("IDLE", a, b, 10.0)
    h.press("F5", delay=0.6)
    a = snap(); time.sleep(20.0); b = snap(); show("F5-drag", a, b, 20.0)
    a = snap(); time.sleep(5.0); b = snap(); show("after", a, b, 5.0)
finally:
    try: print("rc", h.close(), flush=True)
    except Exception as e: print("close-err", e, flush=True)
