import sys, struct, time, subprocess
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
S = {}
out = subprocess.run(["m68k-elf-nm", P + "/build/system/kernel.elf"], capture_output=True, text=True).stdout
for line in out.splitlines():
    p = line.split()
    if len(p) == 3: S[p[2]] = int(p[0], 16)
h = Hatari(P, log=P + "/work/hatari-options.log", window=True)
def u32(a):
    b = h.read_bytes(a, 4, settle=0.3); return struct.unpack(">I", b)[0] if b and len(b) == 4 else None
try:
    h.wait(22)
    for k in range(5):
        h.press("F10", delay=0.5); time.sleep(1.0)
    h.cmd("hatari-debug screenshot /tmp/opt-open.png"); time.sleep(2.0)
    print("menu open", flush=True)
    h.press("Down", delay=0.4); time.sleep(0.6)
    h.press("Return", delay=0.5); time.sleep(1.5)
    h.cmd("hatari-debug screenshot /tmp/opt-800.png"); time.sleep(2.0)
    h.press("F10", delay=0.5); time.sleep(0.8)
    h.press("Down", delay=0.4); time.sleep(0.4)
    h.press("Down", delay=0.4); time.sleep(0.4)
    h.press("Return", delay=0.5); time.sleep(1.5)
    h.cmd("hatari-debug screenshot /tmp/opt-1024.png"); time.sleep(2.0)
    print("uf", u32(S["kdbg_uf_count"]), "rel_end", u32(S["kdbg_rel_end"]), flush=True)
finally:
    print("rc", h.close(), flush=True)
