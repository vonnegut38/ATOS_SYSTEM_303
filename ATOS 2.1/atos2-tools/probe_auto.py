import sys, struct, time, signal, subprocess
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
signal.signal(signal.SIGALRM, lambda *a: (_ for _ in ()).throw(SystemExit("TIMEOUT")))
signal.alarm(240)
def syms():
    out = subprocess.run(["m68k-elf-nm", P + "/build/system/kernel.elf"], capture_output=True, text=True).stdout
    return {p[2]: int(p[0], 16) for p in (l.split() for l in out.splitlines()) if len(p) == 3}
S = syms()
def A(n): return S.get(n)
REG = {k: A(k) for k in ("kdbg_autodrag","kdbg_wm_calls","kdbg_wm_composes","wm_frames","wm_damage_pixels","wm_dbg_drag_end",
                        "kdbg_recompose_cycles","kdbg_deco_cycles","kdbg_cursor_cycles",
                        "kdbg_total_cycles","kdbg_outline_cycles","kdbg_loop_cycles","kdbg_loop_calls") if A(k)}
print({k: hex(v) for k, v in REG.items()}, flush=True)
h = Hatari(P, log=P + "/work/hatari-auto.log", window=True)
def u32(a):
    b = h.read_bytes(a, 4, settle=0.1); return struct.unpack(">I", b)[0] if b and len(b) == 4 else None
snap = lambda: {k: (u32(v) or 0) for k, v in REG.items()}
try:
    h.wait(22)
    started = False
    for k in range(8):
        h.press("F6", delay=0.5); time.sleep(0.7)
        if (u32(REG["kdbg_autodrag"]) or 0) > 0: started = True; break
    print("autodrag started" if started else "F6 not delivered", flush=True)
    a = snap(); t0 = time.time()
    while time.time() - t0 < 30.0:
        if (u32(REG["kdbg_autodrag"]) or 0) == 0: break
        time.sleep(0.05)
    t1 = time.time(); b = snap()
    dt = max(0.001, t1 - t0)
    d = {k: b[k] - a[k] for k in REG}
    print("autodrag took %.3fs" % dt, d, flush=True)
    c = max(1, d.get("kdbg_wm_composes", 0))
    for k in ("kdbg_total_cycles", "kdbg_recompose_cycles", "kdbg_deco_cycles", "kdbg_cursor_cycles", "kdbg_outline_cycles"):
        if k in d: print("  %s: %.2f ms/compose" % (k, d[k] / 12.288 / c), flush=True)
    if d.get("kdbg_loop_calls"):
        print("  loop: %.2f ms/iter" % (d["kdbg_loop_cycles"] / 12.288 / d["kdbg_loop_calls"]), flush=True)
    time.sleep(2.0); c = snap()
    print("settle", {k: c[k] - b[k] for k in REG}, flush=True)
finally:
    try: print("rc", h.close(), flush=True)
    except Exception as e: print("close-err", e, flush=True)
