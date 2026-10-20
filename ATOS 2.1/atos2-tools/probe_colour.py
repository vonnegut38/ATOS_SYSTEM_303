import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
from PIL import Image
from collections import Counter
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
h = Hatari(P, log=P + "/work/hatari-col.log", window=True, extra=["--ttram","64"])
try:
    h.wait(26)
    h.press("F7", delay=1.0); time.sleep(2.5)     # DISK C window (mono)
    h.press("F3", delay=1.0); time.sleep(3.0)     # TEST: switch to 800x600x8
    h.cmd("hatari-debug screenshot /tmp/col.png"); time.sleep(3.0)
    im=Image.open("/tmp/col.png").convert("RGB"); px=im.load(); w,hh=im.size
    print("size", im.size)
    print("sample row 300:", Counter(px[x,300] for x in range(0,w,8)).most_common(4))
    print("sample row 100:", Counter(px[x,100] for x in range(0,w,8)).most_common(4))
    print("top strip:", Counter(px[x,y] for x in range(int(w*0.75),w,2) for y in range(0,16)).most_common(3))
    print("done", flush=True)
finally:
    print("rc", h.close(), flush=True)
