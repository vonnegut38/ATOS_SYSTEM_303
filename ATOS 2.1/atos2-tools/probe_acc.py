import sys, struct, time, subprocess
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
S = {}
for line in subprocess.run(["m68k-elf-nm", P + "/build/system/kernel.elf"], capture_output=True, text=True).stdout.splitlines():
    p = line.split()
    if len(p) == 3: S[p[2]] = int(p[0], 16)
h = Hatari(P, log=P + "/work/hatari-acc.log", window=True)
STRIDE = 834
def u32(a):
    b = h.read_bytes(a, 4, settle=0.25); return struct.unpack(">I", b)[0] if b and len(b) == 4 else None
def acstate(tag):
    base = S["process_table"]
    for i in range(8):
        raw = h.read_bytes(base + i*STRIDE, STRIDE, settle=0.35)
        if not raw or len(raw) < STRIDE: continue
        pid = struct.unpack(">I", raw[8:12])[0]
        if raw[656:669].split(b"\x00")[0].decode("latin1") == "AC.APP":
            print(tag, "AC pid", pid, "state", raw[132], "focus", u32(S["gui_focus_pids"]), flush=True); return
    print(tag, "AC gone", flush=True)
def shot(nm):
    h.cmd("hatari-debug screenshot " + nm); time.sleep(1.8)
try:
    h.wait(24)
    h.press("F4", delay=0.6); time.sleep(1.5)
    h.press("Down", delay=0.5); time.sleep(0.5)
    h.press("Return", delay=0.5); time.sleep(0.7)
    h.press("A", delay=0.2); h.press("C", delay=0.2); time.sleep(0.4)
    h.press("Return", delay=0.8); time.sleep(4.0)
    shot("/tmp/acc-1-toolbar.png"); print("toolbar", flush=True)
    h.press("F9", delay=0.6); time.sleep(1.0)
    h.cmd("hatari-event doubleclick"); time.sleep(2.0)
    acstate("after-click")
    shot("/tmp/acc-2-btnclick.png"); print("btnclick", flush=True)
    for i in range(7):
        h.press("Down", delay=0.35); time.sleep(0.25)
    time.sleep(0.8)
    shot("/tmp/acc-3-selected.png"); print("selected", flush=True)
    h.press("F5", delay=0.6); time.sleep(2.2)
    acstate("after-copy")
    shot("/tmp/acc-4-copied.png"); print("copied", flush=True)
    h.press("F7", delay=0.6); time.sleep(1.2)
    shot("/tmp/acc-5-prompt.png"); print("prompt", flush=True)
    for ch in "NEWDIR":
        h.press(ch, delay=0.2); time.sleep(0.2)
    time.sleep(0.5)
    shot("/tmp/acc-6-typed.png"); print("typed", flush=True)
    h.press("Return", delay=0.6); time.sleep(2.2)
    acstate("after-mkdir")
    shot("/tmp/acc-7-mkdir.png"); print("mkdir", flush=True)
    print("uf", u32(S["kdbg_uf_count"]), flush=True)
finally:
    print("rc", h.close(), flush=True)
