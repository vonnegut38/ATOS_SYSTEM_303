import sys, struct, subprocess, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
def syms():
    out = subprocess.run(["m68k-elf-nm", P + "/build/system/kernel.elf"], capture_output=True, text=True).stdout
    d = {}
    for line in out.splitlines():
        p = line.split()
        if len(p) == 3: d[p[2]] = int(p[0], 16)
    return d
S = syms()
for cpu in ("0",):
    h = Hatari(P, log=P + "/work/hatari-cpu0.log", window=True, extra=["--cpulevel", cpu])
    def u32(a):
        b = h.read_bytes(a, 4, settle=0.4)
        return struct.unpack(">I", b)[0] if b and len(b) == 4 else None
    def s32(a):
        b = h.read_bytes(a, 4, settle=0.4)
        return struct.unpack(">i", b)[0] if b and len(b) == 4 else None
    try:
        h.wait(22)
        n = u32(S["kdbg_end_name"]) or 0
        nm = "".join(chr((n >> sh) & 0xff) for sh in (24, 16, 8, 0))
        print("cpulevel", cpu, "cpu_type", h.read_bytes(S["kernel_cpu_type"], 1, settle=0.4),
              "end_pid", u32(S["kdbg_end_pid"]), "name", repr(nm),
              "status", s32(S["kdbg_end_status"]),
              "wm.count", u32(S["wm"] + 8 * 60), "rel_end", u32(S["kdbg_rel_end"]), flush=True)
        time.sleep(3)
        print("  late wm.count", u32(S["wm"] + 8 * 60), "end_pid", u32(S["kdbg_end_pid"]), flush=True)
    finally:
        print("rc", h.close(), flush=True)
