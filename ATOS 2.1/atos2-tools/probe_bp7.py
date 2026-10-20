import sys, struct, subprocess, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
S = {}
out = subprocess.run(["m68k-elf-nm", P + "/build/system/kernel.elf"], capture_output=True, text=True).stdout
for line in out.splitlines():
    p = line.split()
    if len(p) == 3: S[p[2]] = int(p[0], 16)
h = Hatari(P, log=P + "/work/hatari-bp7.log", window=True, extra=["--cpulevel", "0"])
CAP = str(P + "/work/hatari-bp7.log") + ".out"
def alltext():
    try:
        with open(CAP, "r", errors="replace") as f:
            return f.read()
    except Exception:
        return ""
def clean(s):
    return "\n".join(l for l in s.splitlines() if "Resource temporarily" not in l)
def u32(a):
    b = h.read_bytes(a, 4, settle=0.5)
    return struct.unpack(">I", b)[0] if b and len(b) == 4 else None
try:
    time.sleep(0.3)
    h.cmd("hatari-debug hi cpu 400")
    time.sleep(0.3)
    h.cmd("hatari-debug b pc=0x262f6")
    mark = len(alltext())
    for i in range(4):
        time.sleep(15 if i == 0 else 4)
        vals = {k: u32(S[k]) for k in ("kernel_fault_vector","kernel_fault_pc","kernel_fault_a0",
                                       "kernel_fault_d0","kernel_fault_sr","kdbg_end_pid","kdbg_end_status")}
        h.cmd("hatari-debug r")
        time.sleep(1.0)
        h.cmd("hatari-debug hi 45")
        time.sleep(2.0)
        print("=== user_fault hit %d ===" % i, {k: hex(v or 0) for k, v in vals.items()}, flush=True)
        print(clean(alltext()[mark:])[-4500:], flush=True)
        mark = len(alltext())
        h.cmd("hatari-debug c")
        time.sleep(0.5)
finally:
    print("rc", h.close(), flush=True)
