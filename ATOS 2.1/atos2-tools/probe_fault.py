import sys, struct, subprocess, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"

def syms():
    out = subprocess.run(["m68k-elf-nm", P + "/build/system/kernel.elf"],
                         capture_output=True, text=True).stdout
    d = {}
    for line in out.splitlines():
        p = line.split()
        if len(p) == 3:
            d[p[2]] = int(p[0], 16)
    return d
S = syms()
def A(n): return S.get(n)
WANT = ("kernel_fault_vector","kernel_fault_pc","kernel_fault_sr","kernel_fault_a0",
        "kernel_fault_d0","kernel_fault_usp","kernel_fault_frame",
        "kdbg_end_pid","kdbg_end_term","kdbg_end_name","kdbg_end_status",
        "kdbg_end_pc","kdbg_end_image","kdbg_kbpid","kdbg_rel_calls","kdbg_rel_end",
        "kdbg_rel_kill","kdbg_rel_pid","wm","wm_ready","kdbg_end_site")
T = {k: A(k) for k in WANT if A(k)}
print("addrs", {k: hex(v) for k, v in T.items()}, flush=True)
h = Hatari(P, log=P + "/work/hatari-fault.log", window=True)
def rd(a, n): return h.read_bytes(a, n, settle=0.25)
def u32(a):
    b = rd(a, 4)
    return struct.unpack(">I", b)[0] if b and len(b) == 4 else None
def s32(a):
    b = rd(a, 4)
    return struct.unpack(">i", b)[0] if b and len(b) == 4 else None
def dump(tag):
    d = {}
    for k, v in T.items():
        d[k] = s32(v) if k.endswith("status") else u32(v)
    fr = [u32(T["kernel_fault_frame"] + 4 * i) for i in range(6)] if "kernel_fault_frame" in T else []
    n = d.get("kdbg_end_name") or 0
    nm = "".join(chr((n >> sh) & 0xff) for sh in (24, 16, 8, 0))
    print(tag, "end_pid=%s name=%r status=%s pc=%s image=%s" % (
        d.get("kdbg_end_pid"), nm, d.get("kdbg_end_status"),
        hex(d.get("kdbg_end_pc") or 0), hex(d.get("kdbg_end_image") or 0)), flush=True)
    print(tag, "fault vec=%s pc=%s sr=%s a0=%s d0=%s usp=%s frame=%s" % (
        d.get("kernel_fault_vector"), hex(d.get("kernel_fault_pc") or 0),
        hex(d.get("kernel_fault_sr") or 0), hex(d.get("kernel_fault_a0") or 0),
        hex(d.get("kernel_fault_d0") or 0), hex(d.get("kernel_fault_usp") or 0),
        [hex(x or 0) for x in fr]), flush=True)
    print(tag, "rel_calls=%s rel_end=%s rel_kill=%s rel_pid=%s kbpid=%s" % (
        d.get("kdbg_rel_calls"), d.get("kdbg_rel_end"), d.get("kdbg_rel_kill"),
        d.get("kdbg_rel_pid"), d.get("kdbg_kbpid")), flush=True)
    wm = d.get("wm")
    if wm:
        print(tag, "wm.count=%s" % u32(wm + 8 * 60), flush=True)
try:
    h.wait(22)
    dump("boot")
    h.screenshot(P + "/work/fault-boot.png", settle=0.4)
    time.sleep(2)
    dump("boot2")
finally:
    print("rc", h.close(), flush=True)
