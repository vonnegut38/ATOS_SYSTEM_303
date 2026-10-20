import sys, struct, subprocess, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
S = {}
out = subprocess.run(["m68k-elf-nm", P + "/build/system/kernel.elf"], capture_output=True, text=True).stdout
for line in out.splitlines():
    p = line.split()
    if len(p) == 3: S[p[2]] = int(p[0], 16)
h = Hatari(P, log=P + "/work/hatari-shot.log", window=True)
def u32(a):
    b = h.read_bytes(a, 4, settle=0.4)
    return struct.unpack(">I", b)[0] if b and len(b) == 4 else None
try:
    h.wait(23)
    W = S["wm"]
    raw = h.read_bytes(W + 60, 60, settle=0.5)
    print("win1 raw:", raw.hex(), flush=True)
    print("src_x", struct.unpack(">h", raw[22:24])[0], "src_y", struct.unpack(">h", raw[24:26])[0],
          "content_h", struct.unpack(">H", raw[26:28])[0], flush=True)
    h.cmd("hatari-debug screenshot /tmp/atos-desktop.png")
    time.sleep(2.5)
finally:
    print("rc", h.close(), flush=True)
