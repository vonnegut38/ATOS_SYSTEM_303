import sys, struct, subprocess, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
S = {}
out = subprocess.run(["m68k-elf-nm", P + "/build/system/kernel.elf"], capture_output=True, text=True).stdout
for line in out.splitlines():
    p = line.split()
    if len(p) == 3: S[p[2]] = int(p[0], 16)
h = Hatari(P, log=P + "/work/hatari-stack.log", window=True, extra=["--cpulevel", "0"])
def u32(a):
    b = h.read_bytes(a, 4, settle=0.5)
    return struct.unpack(">I", b)[0] if b and len(b) == 4 else None
try:
    h.wait(22)
    print("end_pid", u32(S["kdbg_end_pid"]), "status", u32(S["kdbg_end_status"]),
          "image", hex(u32(S["kdbg_end_image"]) or 0), "pc", hex(u32(S["kdbg_end_pc"]) or 0), flush=True)
    b = h.read_bytes(0x79F00, 256, settle=1.0)
    for off in range(0, 256, 16):
        row = b[off:off+16]
        if len(row) < 16: break
        print("%06X:" % (0x79F00+off), " ".join("%08X" % struct.unpack(">I", row[i:i+4])[0] for i in range(0, 16, 4)), flush=True)
finally:
    print("rc", h.close(), flush=True)
