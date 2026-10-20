"""Boot-only diagnostic: resolve kdbg_tos_* from kernel.elf and report placement."""
import subprocess, sys, time
from pathlib import Path
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/ATOS 2.1/atos2-tools")
from atos2_auto import Hatari
P = Path("/Users/xxx/Documents/Deepseek_test/ATOS 2.1")
IMG = P / "work/atos-tos-mod.img"
want = ["kdbg_tos_compat_loads", "kdbg_tos_module_screen",
        "kdbg_tos_screen_fallbacks", "kdbg_tos_adopted_module"]
sym = {}
out = subprocess.run(["m68k-elf-nm", str(P/"build/system/kernel.elf")],
                     capture_output=True, text=True).stdout
for ln in out.splitlines():
    parts = ln.split()
    if len(parts) == 3 and parts[2] in want:
        sym[parts[2]] = int(parts[0], 16)
print("symbols:", {k: hex(v) for k, v in sym.items()}, flush=True)
N = []
for i in range(8):
    N += ["--acsi", "%d=none" % i]
E = ["--machine","megast","--cpulevel","3","--cpuclock","32","--memsize","4","--ttram","64",
     "--tf536","on","--graphics-card","vofa","--gfx-chip","et4000","--mono","--fast-forward","off",
     "--ide-master", str(IMG)] + N
h = Hatari(str(P), log=str(P/"work/h-bootdiag.log"), window=False, image=str(IMG),
           binary="/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536",
           cfg=True, extra=E, render=True)
def u32(a):
    b = h.read_bytes(a, 4, settle=0.9) or b""
    return int.from_bytes(b, "big") if len(b) == 4 else None
try:
    time.sleep(160)
    sc = u32(sym["kdbg_tos_module_screen"]) or 0
    print("loads=%s module_screen=0x%X (%s) fallbacks=%s adopted=%s" % (
        u32(sym["kdbg_tos_compat_loads"]), sc,
        "TT RAM" if sc >= 0x400000 else ("ST RAM" if sc else "none"),
        u32(sym["kdbg_tos_screen_fallbacks"]), u32(sym["kdbg_tos_adopted_module"])))
    lines = "\n".join(h.console_lines(rows=25) or [])
    print("console TOS.SYS:", [l.strip() for l in lines.splitlines() if "TOS.SYS" in l])
finally:
    print("rc", h.close())
