import subprocess, sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
HB = "/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536"
IMG = P + "/work/atos-v2.img"
FLOP = "/tmp/atos-boot.st"
OUT = "/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/boot"
NOACSI = []
for i in range(8): NOACSI += ["--acsi", "%d=none" % i]
EXTRA = ["--machine","megast","--cpulevel","3","--cpuclock","32","--memsize","4",
         "--ttram","64","--tf536","on","--graphics-card","vofa","--gfx-chip","et4000","--mono",
         "--ide-master", IMG, "--disk-a", FLOP] + NOACSI
def tap(h,k):
    h.keydown(k); time.sleep(0.5); h.keyup(k); time.sleep(0.35)
subprocess.run(["python3","tools/make_bios_cfg.py","work/BIOS.CFG","--auto-tos","0","--boot-delay","25"], cwd=P, check=True)
subprocess.run(["python3","tools/install_boot_sys.py","work/atos-v2.img","work/BIOS.CFG","--directory","BIOS","--data"], cwd=P, check=True)
h = Hatari(P, log=P+"/work/hatari-bootmenufdd3.log", window=True, image=IMG, binary=HB, cfg=True, extra=EXTRA)
try:
    time.sleep(5)
    for i in range(4):
        tap(h,"F4")
    time.sleep(1.0)
    print("menu", h.screenshot(OUT+"/bm3-menu.png", settle=0.8), flush=True)
    tap(h,"Right"); time.sleep(0.6)
    print("right", h.screenshot(OUT+"/bm3-right.png", settle=0.8), flush=True)
    tap(h,"Return"); time.sleep(1.5)
    print("saved", h.screenshot(OUT+"/bm3-saved.png", settle=0.8), flush=True)
finally:
    print("rc", h.close(), flush=True)
subprocess.run(["python3","tools/probe_bios_cfg.py"], cwd=P, check=True)
subprocess.run(["python3","tools/make_bios_cfg.py","work/BIOS.CFG","--auto-tos","0","--boot-delay","1"], cwd=P, check=True)
for extra in (["--directory","BIOS","--data"],["--root","--data"]):
    subprocess.run(["python3","tools/install_boot_sys.py","work/atos-v2.img","work/BIOS.CFG"]+extra, cwd=P, check=True)
print("DONE", flush=True)
