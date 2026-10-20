import re, subprocess, sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
HB = "/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536"
IMG = P + "/work/atos-v2.img"
FLOP = "/tmp/atos-floppy.st"
NOACSI = []
for i in range(8): NOACSI += ["--acsi", "%d=none" % i]
EXTRA = ["--machine","megast","--cpulevel","3","--cpuclock","32","--memsize","4",
         "--ttram","64","--tf536","on","--graphics-card","vofa","--gfx-chip","et4000","--mono",
         "--ide-master", IMG, "--disk-a", FLOP, "--trace", "fdc"] + NOACSI
def tap(h,k):
    h.keydown(k); time.sleep(0.3); h.keyup(k); time.sleep(0.25)
subprocess.run(["python3","tools/make_bios_cfg.py","work/BIOS.CFG","--auto-tos","0","--boot-delay","15"], cwd=P, check=True)
subprocess.run(["python3","tools/install_boot_sys.py","work/atos-v2.img","work/BIOS.CFG","--directory","BIOS","--data"], cwd=P, check=True)
h = Hatari(P, log=P+"/work/hatari-fdctrace.log", window=False, image=IMG, binary=HB, cfg=True, extra=EXTRA)
try:
    time.sleep(4.5); tap(h,"F3"); time.sleep(3.0)
finally:
    print("rc", h.close(), flush=True)
subprocess.run(["python3","tools/make_bios_cfg.py","work/BIOS.CFG","--auto-tos","0","--boot-delay","1"], cwd=P, check=True)
for extra in (["--directory","BIOS","--data"],["--root","--data"]):
    subprocess.run(["python3","tools/install_boot_sys.py","work/atos-v2.img","work/BIOS.CFG"]+extra, cwd=P, check=True)
print("DONE", flush=True)
