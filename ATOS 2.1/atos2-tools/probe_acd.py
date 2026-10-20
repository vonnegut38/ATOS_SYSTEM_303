import sys, struct, time, subprocess
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
S = {}
for line in subprocess.run(["m68k-elf-nm", P + "/build/system/kernel.elf"], capture_output=True, text=True).stdout.splitlines():
    p = line.split()
    if len(p) == 3: S[p[2]] = int(p[0], 16)
h = Hatari(P, log=P + "/work/hatari-acd.log", window=True)
def u32(a):
    b = h.read_bytes(a, 4, settle=0.25); return struct.unpack(">I", b)[0] if b and len(b) == 4 else None
def shot(nm):
    h.cmd("hatari-debug screenshot " + nm); time.sleep(1.8)
try:
    h.wait(24)
    h.press("F4", delay=0.6); time.sleep(1.5)
    h.press("Down", delay=0.5); time.sleep(0.5)
    h.press("Return", delay=0.5); time.sleep(0.7)
    h.press("A", delay=0.2); h.press("C", delay=0.2); time.sleep(0.4)
    h.press("Return", delay=0.8); time.sleep(4.0)
    h.press("F9", delay=0.6); time.sleep(1.0)         # park over COPY button
    h.cmd("hatari-event doubleclick"); time.sleep(2.0) # focus AC (COPY on a dir: harmless)
    for i in range(6):
        h.press("Down", delay=0.4); time.sleep(0.35)   # -> AUTOEXEC.PRG
    time.sleep(0.8)
    shot("/tmp/acd-1-selected.png")
    h.press("F5", delay=0.6); time.sleep(2.5)          # copy file to the other panel
    shot("/tmp/acd-2-copied.png"); print("copied", flush=True)
    h.press("F7", delay=0.6); time.sleep(1.2)
    for ch in "PACK":
        h.press(ch, delay=0.2); time.sleep(0.2)
    h.press("Return", delay=0.6); time.sleep(2.5)
    shot("/tmp/acd-3-mkdir.png"); print("mkdir", flush=True)
    print("uf", u32(S["kdbg_uf_count"]), flush=True)
finally:
    print("rc", h.close(), flush=True)
