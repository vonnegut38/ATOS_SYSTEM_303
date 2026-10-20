import sys, os
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
HB = "/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536"
IMG = P + "/work/atos-v2.img"
NOACSI = []
for i in range(8): NOACSI += ["--acsi", "%d=none" % i]
EXTRA = ["--machine","megast","--cpulevel","3","--cpuclock","32","--memsize","4",
         "--ttram","64","--tf536","on","--graphics-card","vofa","--gfx-chip","et4000","--mono",
         "--ide-master", IMG] + NOACSI
h = Hatari(P, log=P + "/work/hatari-dbg.log", window=True, image=IMG, binary=HB, cfg=True, extra=EXTRA)
try:
    h.wait(10)
    for addr, name in [(0x00c00000, "card-vram"), (0x00010000, "shifter-scr"), (0x00d00000, "vofa-reg")]:
        raw = h.read_mem(addr, 32, settle=0.6)
        lines = [l for l in raw.splitlines() if ":" in l and l.strip()[0] in "0123456789ABCDEFabcdef"]
        print(name, "->", lines[0].strip() if lines else raw[-120:].replace(chr(10)," "), flush=True)
finally:
    print("rc", h.close())
