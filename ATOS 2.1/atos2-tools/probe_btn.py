import sys, struct, time, signal, subprocess
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
signal.signal(signal.SIGALRM, lambda *a: (_ for _ in ()).throw(SystemExit("TIMEOUT")))
signal.alarm(120)
def syms():
    out = subprocess.run(["m68k-elf-nm", P + "/build/system/kernel.elf"], capture_output=True, text=True).stdout
    return {p[2]: int(p[0], 16) for p in (l.split() for l in out.splitlines()) if len(p) == 3}
S = syms()
def A(n): return S.get(n)
DBG = {k: A(k) for k in ("kdbg_real_mouse","kdbg_real_btn","kdbg_real_dxdy")}
h = Hatari(P, log=P + "/work/hatari-btn.log", window=True)
def u32(a):
    b = h.read_bytes(a, 4, settle=0.2); return struct.unpack(">I", b)[0] if b and len(b) == 4 else None
def d(): return {k: u32(v) for k, v in DBG.items()}
try:
    h.wait(22); print("boot", d(), flush=True)
    h.cmd("hatari-event rightdown"); time.sleep(1.0); print("rightdown", d(), flush=True)
    h.cmd("hatari-event rightup"); time.sleep(1.0); print("rightup", d(), flush=True)
    h.cmd("hatari-event doubleclick"); time.sleep(1.5); print("doubleclick", d(), flush=True)
finally:
    try: print("rc", h.close(), flush=True)
    except Exception as e: print("close-err", e, flush=True)
