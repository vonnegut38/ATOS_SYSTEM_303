import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
from PIL import Image
from collections import Counter
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
BIN = "/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-src/build/src/hatari"
CARD = ["--machine","megast","--cpuclock","32","--ttram","64","--tf536","on",
        "--graphics-card","vofa","--gfx-chip","et4000","--monitor","mono"]
h = Hatari(P, log=P + "/work/hatari-card2.log", window=True, extra=CARD, binary=BIN)
try:
    time.sleep(18)
    h.cmd("hatari-debug screenshot /tmp/card2.png"); time.sleep(4)
    im=Image.open("/tmp/card2.png").convert("RGB"); px=im.load(); w,hh=im.size
    print("shot", im.size)
    print("colours:", Counter(px[x,y] for x in range(0,w,7) for y in range(0,min(hh,560),7)).most_common(6))
finally:
    print("rc", h.close(), flush=True)
