import sys, struct, time, subprocess
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
S = {}
out = subprocess.run(["m68k-elf-nm", P + "/build/system/kernel.elf"], capture_output=True, text=True).stdout
for line in out.splitlines():
    p = line.split()
    if len(p) == 3: S[p[2]] = int(p[0], 16)
h = Hatari(P, log=P + "/work/hatari-ste-mode.log", window=True)
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
    print("boot:", wins(), flush=True)
    h.press("F7", delay=1.0); time.sleep(1.5)
    print("after F7:", wins(), flush=True)
    h.cmd("hatari-debug screenshot /tmp/ste-mode-desktop.png"); time.sleep(1.5)
    # F10 OPTIONS, Down -> SET 800x600, Enter: must be refused on the Shifter
    h.press("F10", delay=1.0); time.sleep(1.0)
    h.press("Down", delay=0.5); h.press("Return", delay=1.0); time.sleep(2.0)
    h.cmd("hatari-debug screenshot /tmp/ste-mode-refused.png"); time.sleep(1.5)
    print("uf", u32(S["kdbg_uf_count"]), "wins", wins(), flush=True)
finally:
    print("rc", h.close(), flush=True)
