import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
HB = "/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536"
OUT = "/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/boot"
EXTRA = ["--machine","megast","--cpulevel","3","--cpuclock","32","--memsize","4",
         "--ttram","64","--tf536","on","--graphics-card","vofa","--gfx-chip","et4000","--mono"]
h = Hatari(P, log=P+"/work/hatari-hddpath.log", window=True, image=P+"/work/atos-v2.img",
           binary=HB, cfg=True, extra=EXTRA)
try:
    time.sleep(45)
    h.type("command", delay=0.14); h.press("\r", delay=0.5); time.sleep(8)
    print("cmd", h.screenshot(OUT+"/hddpath-command.png", settle=0.8), flush=True)
    h.type("path", delay=0.14); h.press("\r", delay=0.5); time.sleep(3)
    print("path", h.screenshot(OUT+"/hddpath-path.png", settle=0.8), flush=True)
finally:
    print("rc", h.close(), flush=True)
