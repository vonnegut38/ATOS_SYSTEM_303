import subprocess, sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
HB = "/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536"
IMG = P + "/work/atos-v2.img"
OUT = "/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/boot"
N = []
for i in range(8): N += ["--acsi", "%d=none" % i]
E = ["--machine","megast","--cpulevel","3","--cpuclock","32","--memsize","4","--ttram","64",
     "--tf536","on","--graphics-card","vofa","--gfx-chip","et4000","--mono","--ide-master",IMG]+N
def tap(h,k,d=0.2):
    h.keydown(k, delay=d); h.keyup(k, delay=d)
h = Hatari(P, log=P+"/work/hatari-setupclock.log", window=True, image=IMG, binary=HB, cfg=True, extra=E)
try:
    h.wait(4.2)
    h.keydown("F1", delay=0.7); h.keyup("F1", delay=0.2); h.wait(1.2)
    print("open", h.screenshot(OUT+"/setclock-open.png", settle=0.8), flush=True)
    for i in range(5): tap(h,"Right")
    print("clock", h.screenshot(OUT+"/setclock-field.png", settle=0.8), flush=True)
    tap(h,"Down"); time.sleep(0.3)
    print("changed", h.screenshot(OUT+"/setclock-changed.png", settle=0.8), flush=True)
    tap(h,"Right"); time.sleep(0.3)
    print("video", h.screenshot(OUT+"/setclock-video.png", settle=0.8), flush=True)
    tap(h,"Return"); h.wait(1.2)
    print("saved", h.screenshot(OUT+"/setclock-saved.png", settle=0.8), flush=True)
finally:
    print("rc", h.close(), flush=True)
subprocess.run(["python3","tools/probe_bios_cfg.py"], cwd=P, check=False)
