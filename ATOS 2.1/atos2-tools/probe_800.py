import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
from PIL import Image
from collections import Counter
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
h = Hatari(P, log=P + "/work/hatari-800.log", window=True)
try:
    time.sleep(14)                      # boot log on screen
    h.cmd("hatari-debug screenshot /tmp/boot800.png"); time.sleep(3)
    im=Image.open("/tmp/boot800.png").convert("RGB"); px=im.load(); w,hh=im.size
    print("screenshot", im.size)
    white=sum(1 for x in range(0,w,3) for y in range(0,min(hh,400),3) if px[x,y]==(255,255,255))
    print("white px (text should be visible):", white)
finally:
    print("rc", h.close(), flush=True)
