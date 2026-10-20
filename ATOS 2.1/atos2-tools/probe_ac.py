import sys, struct, time, subprocess
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
S = {}
for line in subprocess.run(["m68k-elf-nm", P + "/build/system/kernel.elf"], capture_output=True, text=True).stdout.splitlines():
    p = line.split()
    if len(p) == 3: S[p[2]] = int(p[0], 16)
h = Hatari(P, log=P + "/work/hatari-ac6.log", window=True)
STRIDE = 834
def u32(a):
    b = h.read_bytes(a, 4, settle=0.2); return struct.unpack(">I", b)[0] if b and len(b) == 4 else None
def procs(tag):
    base = S["process_table"]; out = []
    for i in range(8):
        raw = h.read_bytes(base + i*STRIDE, STRIDE, settle=0.4)
        if not raw or len(raw) < STRIDE: break
        pid = struct.unpack(">I", raw[8:12])[0]
        nm = raw[656:669].split(b"\x00")[0].decode("latin1")
        if pid: out.append((pid, raw[132], nm))
    print(tag, "procs", out, "focus", u32(S["gui_focus_pids"]), flush=True)
def esc():
    h.cmd("hatari-event keypress 01")   # scancode 1 (two chars -> keycode path)
try:
    h.wait(24)
    h.press("F4", delay=0.6); time.sleep(1.5)
    h.press("Down", delay=0.5); time.sleep(0.5)
    h.press("Return", delay=0.5); time.sleep(0.7)
    h.press("A", delay=0.2); h.press("C", delay=0.2); time.sleep(0.4)
    h.press("Return", delay=0.8); time.sleep(4.0)
    h.press("F9", delay=0.6); time.sleep(1.0)
    h.cmd("hatari-event doubleclick"); time.sleep(1.5)
    procs("focused")
    h.cmd("hatari-debug screenshot /tmp/ac6-1-focused.png"); time.sleep(1.5)
    esc(); time.sleep(2.5)
    procs("escape")
    h.cmd("hatari-debug screenshot /tmp/ac6-2-closed.png"); time.sleep(1.5)
    h.press("F10", delay=0.7); time.sleep(1.5)
    h.cmd("hatari-debug screenshot /tmp/ac6-3-menu.png"); time.sleep(1.5)
    procs("menu")
finally:
    print("rc", h.close(), flush=True)
