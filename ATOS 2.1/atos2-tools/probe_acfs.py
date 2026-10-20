import sys, struct, time, subprocess
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
S = {}
for line in subprocess.run(["m68k-elf-nm", P + "/build/system/kernel.elf"], capture_output=True, text=True).stdout.splitlines():
    p = line.split()
    if len(p) == 3: S[p[2]] = int(p[0], 16)
h = Hatari(P, log=P + "/work/hatari-fs.log", window=True)
def u32(a):
    b = h.read_bytes(a, 4, settle=0.25); return struct.unpack(">I", b)[0] if b and len(b) == 4 else None
def shot(nm):
    h.cmd("hatari-debug screenshot " + nm); time.sleep(1.8)
try:
    h.wait(24)
    # FILE -> CLOSE leaves the desktop back to the text shell
    h.press("F4", delay=0.7); time.sleep(1.5)
    h.press("Down", delay=0.5); time.sleep(0.5)
    h.press("Down", delay=0.5); time.sleep(0.5)
    h.press("Return", delay=0.7); time.sleep(3.0)
    shot("/tmp/fs-1-shell.png"); print("shell", flush=True)
    for ch in "AC":
        h.press(ch, delay=0.25); time.sleep(0.25)
    h.press("Return", delay=0.8); time.sleep(4.5)
    shot("/tmp/fs-2-ac.png"); print("ac", flush=True)
    print("uf", u32(S["kdbg_uf_count"]), "kbpid", u32(S["kdbg_kbpid"]), flush=True)
finally:
    print("rc", h.close(), flush=True)
