import subprocess, sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
HB = "/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536"
IMG = P + "/work/atos-v2.img"
OUT = "/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/boot"
NOACSI = []
for i in range(8): NOACSI += ["--acsi", "%d=none" % i]
EXTRA = ["--machine","megast","--cpulevel","3","--cpuclock","32","--memsize","4",
         "--ttram","64","--tf536","on","--graphics-card","vofa","--gfx-chip","et4000","--mono",
         "--ide-master", IMG] + NOACSI

def shot(tag, wait, name):
    h = Hatari(P, log=P+"/work/hatari-"+tag+".log", window=True, image=IMG, binary=HB, cfg=True, extra=EXTRA)
    try:
        time.sleep(wait)
        print(name, h.screenshot(OUT+"/"+name, settle=0.8), flush=True)
    finally:
        print("rc", tag, h.close(), flush=True)

print("== 1) healthy card boot ==", flush=True)
shot("demoA", 28.0, "demo1-card-shell.png")

print("== 2) latch a failed boot (video_pending=1, boot_delay=8) ==", flush=True)
subprocess.run(["python3","tools/make_bios_cfg.py","work/BIOS.CFG","--auto-tos","0",
                "--boot-delay","8","--video-pending","1"], cwd=P, check=True)
subprocess.run(["python3","tools/install_boot_sys.py","work/atos-v2.img","work/BIOS.CFG",
                "--directory","BIOS","--data"], cwd=P, check=True)

h = Hatari(P, log=P+"/work/hatari-demoB.log", window=True, image=IMG, binary=HB, cfg=True, extra=EXTRA)
try:
    time.sleep(4.0)
    print("demo2", h.screenshot(OUT+"/demo2-failover-notice.png", settle=0.8), flush=True)
    time.sleep(38.0)
    print("demo3", h.screenshot(OUT+"/demo3-shifter-shell.png", settle=0.8), flush=True)
finally:
    print("rc demoB", h.close(), flush=True)

print("== 3) restore clean config ==", flush=True)
subprocess.run(["python3","tools/make_bios_cfg.py","work/BIOS.CFG","--auto-tos","0",
                "--boot-delay","1"], cwd=P, check=True)
for extra in (["--directory","BIOS","--data"], ["--root","--data"]):
    subprocess.run(["python3","tools/install_boot_sys.py","work/atos-v2.img","work/BIOS.CFG"]+extra,
                   cwd=P, check=True)
print("DONE", flush=True)
