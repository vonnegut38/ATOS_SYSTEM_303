import sys, time
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
h = Hatari(P, log=P + "/work/hatari-alt.log", window=True, image=IMG, binary=HB, cfg=True, extra=EXTRA)
try:
    time.sleep(3.0)
    h.press("Alt", delay=0.2)
    h.wait(6)
    print("alt_probe:", hex(h.read_u32(0x000018f0) or 0), flush=True)
    print("rom_status:", hex(h.read_u32(0x00000d6c) or 0), flush=True)
    time.sleep(3.0)
    print("alt1", h.screenshot(OUT + "/alt1.png", settle=0.8), flush=True)
finally:
    print("rc", h.close())
