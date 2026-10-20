import sys, struct, time, subprocess
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
S = {}
for line in subprocess.run(["m68k-elf-nm", P + "/build/system/kernel.elf"], capture_output=True, text=True).stdout.splitlines():
    p = line.split()
    if len(p) == 3: S[p[2]] = int(p[0], 16)
h = Hatari(P, log=P + "/work/hatari-ace.log", window=True)
STRIDE = 834
def u32(a):
    b = h.read_bytes(a, 4, settle=0.25); return struct.unpack(">I", b)[0] if b and len(b) == 4 else None
def acstate(tag):
    base = S["process_table"]
    for i in range(8):
        raw = h.read_bytes(base + i*STRIDE, STRIDE, settle=0.35)
        if not raw or len(raw) < STRIDE: continue
        if raw[656:669].split(b"\x00")[0].decode("latin1") == "AC.APP":
            pc = struct.unpack(">I", raw[20:24])[0]
            print(tag, "state", raw[132], "pc", hex(pc), "uf", u32(S["kdbg_uf_count"]), flush=True); return
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
    h.press("F9", delay=0.6); time.sleep(1.0)
    h.cmd("hatari-event doubleclick"); time.sleep(2.0)
    shot("/tmp/ace-0-before.png")
    h.press("F7", delay=0.6); time.sleep(1.2)
    for ch in "TST":
        h.press(ch, delay=0.2); time.sleep(0.2)
    h.press("Return", delay=0.6); time.sleep(2.5)
    acstate("after-mkdir")
    shot("/tmp/ace-1-mkdir.png")
    h.press("Down", delay=0.6); time.sleep(1.5)
    acstate("after-down")
    shot("/tmp/ace-2-down.png")
finally:
    print("rc", h.close(), flush=True)
