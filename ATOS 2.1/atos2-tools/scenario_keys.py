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
h = Hatari(P, log=P+"/work/hatari-keys.log", window=True, image=IMG, binary=HB, cfg=True, extra=EXTRA)
def line(text, tag, wait=4):
    for ch in text:
        h.keydown(ch, delay=0.12); h.keyup(ch, delay=0.12)
    time.sleep(0.5)
    print(tag+"-before", h.screenshot(OUT+"/ky-"+tag+"-before.png", settle=0.7), flush=True)
    h.keydown("\r", delay=0.12); h.keyup("\r", delay=0.4); time.sleep(wait)
    print(tag+"-after", h.screenshot(OUT+"/ky-"+tag+"-after.png", settle=0.7), flush=True)
try:
    time.sleep(30)
    line("pwd", "1-pwd")
    line("path", "2-path", 4)
finally:
    print("rc", h.close(), flush=True)
