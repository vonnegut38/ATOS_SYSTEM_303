import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
from PIL import Image
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
BIN = "/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-src/build/src/hatari"
CARD = ["--machine","megast","--cpuclock","32","--ttram","64","--tf536","on",
        "--graphics-card","vofa","--gfx-chip","et4000","--monitor","mono"]
h = Hatari(P, log=P + "/work/hatari-card3.log", window=True, extra=CARD, binary=BIN)
try:
    time.sleep(5)
    for i in range(3):
        h.cmd("hatari-debug screenshot /tmp/early%d.png" % i); time.sleep(2.0)
    im=Image.open("/tmp/early1.png").convert("RGB")
    print("early shot", im.size)
finally:
    print("rc", h.close(), flush=True)
