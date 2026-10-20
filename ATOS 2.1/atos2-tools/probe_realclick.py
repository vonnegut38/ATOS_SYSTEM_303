import sys, struct, time, subprocess
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
S = {}
out = subprocess.run(["m68k-elf-nm", P + "/build/system/kernel.elf"], capture_output=True, text=True).stdout
for line in out.splitlines():
    p = line.split()
    if len(p) == 3: S[p[2]] = int(p[0], 16)
h = Hatari(P, log=P + "/work/hatari-realclick.log", window=True)
def u32(a):
    b = h.read_bytes(a, 4, settle=0.3); return struct.unpack(">I", b)[0] if b and len(b) == 4 else None
def wins():
    W = S["wm"]; n = u32(W + 8 * 60) or 0; out = []
    for i in range(n):
        raw = h.read_bytes(W + i * 60, 60, settle=0.3)
        if raw and len(raw) == 60:
            out.append((i,) + struct.unpack(">hhHH", raw[0:8]) + (raw[28:52].split(b"\x00")[0].decode("latin1"),))
    return out
try:
    h.wait(22)
    print("boot", wins(), flush=True)
    for k in range(6):
        h.press("F9", delay=0.6); time.sleep(0.8)
    print("pointer parked", flush=True)
    ok = False
    for k in range(4):
        h.cmd("hatari-event doubleclick"); time.sleep(1.5)
        w = wins()
        if len(w) >= 3: ok = True; break
    print("real doubleclick ->", w, "OPENED" if ok else "NO WINDOW", flush=True)
    h.cmd("hatari-debug screenshot /tmp/atos-realclick.png"); time.sleep(2.0)
    print("uf", u32(S["kdbg_uf_count"]), "rel_end", u32(S["kdbg_rel_end"]), flush=True)
finally:
    print("rc", h.close(), flush=True)
