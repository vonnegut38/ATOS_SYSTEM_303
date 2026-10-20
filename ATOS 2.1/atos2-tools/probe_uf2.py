import sys, struct, subprocess, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
S = {}
out = subprocess.run(["m68k-elf-nm", P + "/build/system/kernel.elf"], capture_output=True, text=True).stdout
for line in out.splitlines():
    p = line.split()
    if len(p) == 3: S[p[2]] = int(p[0], 16)
KEYS = ("kdbg_uf_count","kdbg_uf_pid","kdbg_uf_vector","kdbg_uf_pc","kdbg_uf_a0",
        "kdbg_uf_d0","kdbg_uf_sr","kdbg_uf_usp","kdbg_uf_image","kdbg_uf_savedpc",
        "kdbg_uf_state","kdbg_uf_w4","kdbg_uf_w6","kdbg_uf_w8","kdbg_uf_w10","kdbg_uf_w12")
def run(cpu):
    h = Hatari(P, log=P + "/work/hatari-uf2_%s.log" % cpu, window=True, extra=["--cpulevel", cpu])
    def u32(a):
        b = h.read_bytes(a, 4, settle=0.5)
        return struct.unpack(">I", b)[0] if b and len(b) == 4 else None
    try:
        h.wait(23)
        vals = {k: u32(S[k]) for k in KEYS}
        print("=== cpulevel", cpu, "===", flush=True)
        for k in KEYS:
            print("  %-18s %08X" % (k, vals[k] or 0), flush=True)
    finally:
        print("rc", h.close(), flush=True)
for cpu in ("0", "3"):
    run(cpu)
