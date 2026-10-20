import sys, struct, time, subprocess
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
S = {}
for line in subprocess.run(["m68k-elf-nm", P + "/build/system/kernel.elf"], capture_output=True, text=True).stdout.splitlines():
    p = line.split()
    if len(p) == 3: S[p[2]] = int(p[0], 16)
h = Hatari(P, log=P + "/work/hatari-acb.log", window=True)
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
    shot("/tmp/acb-1-toolbar.png"); print("toolbar", flush=True)
    # F9 parks the pointer over the COPY button (client 22,10); the click focuses AC
    h.press("F9", delay=0.6); time.sleep(1.0)
    h.cmd("hatari-event doubleclick"); time.sleep(1.8)
    shot("/tmp/acb-2-btnclick.png"); print("btnclick focus", u32(S["gui_focus_pids"]), flush=True)
    for i in range(7):
        h.press("Down", delay=0.35); time.sleep(0.25)
    time.sleep(0.6)
    h.press("F5", delay=0.6); time.sleep(2.0)   # copy selected file to other panel
    shot("/tmp/acb-3-copied.png"); print("copied", flush=True)
    h.press("F7", delay=0.6); time.sleep(1.0)   # mkdir prompt
    for ch in "NEWDIR":
        h.press(ch, delay=0.18); time.sleep(0.15)
    h.press("Return", delay=0.6); time.sleep(2.0)
    shot("/tmp/acb-4-mkdir.png"); print("mkdir", flush=True)
    print("uf", u32(S["kdbg_uf_count"]), flush=True)
finally:
    print("rc", h.close(), flush=True)
