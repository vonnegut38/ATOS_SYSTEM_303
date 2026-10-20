import sys, struct, time, subprocess
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
S = {}
for line in subprocess.run(["m68k-elf-nm", P + "/build/system/kernel.elf"], capture_output=True, text=True).stdout.splitlines():
    p = line.split()
    if len(p) == 3: S[p[2]] = int(p[0], 16)
h = Hatari(P, log=P + "/work/hatari-scW.log", window=True)
def u32(a):
    b = h.read_bytes(a, 4, settle=0.25); return struct.unpack(">I", b)[0] if b and len(b) == 4 else None
def shot(nm):
    h.cmd("hatari-debug screenshot " + nm); time.sleep(1.8)
try:
    h.wait(24)
    h.press("F7", delay=0.7); time.sleep(2.0)      # file window (opcode 20)
    shot("/tmp/scw-1-filewin.png")
    h.press("F10", delay=0.7); time.sleep(1.5)     # OPTIONS menu
    shot("/tmp/scw-2-menu.png")
    h.press("Return", delay=0.7); time.sleep(2.5)  # SETTINGS -> SETUP window (opcode 20)
    shot("/tmp/scw-3-setup.png")
    print("uf", u32(S["kdbg_uf_count"]), flush=True)
finally:
    print("rc", h.close(), flush=True)
