import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
from PIL import Image
from collections import Counter
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
h = Hatari(P, log=P + "/work/hatari-gt.log", window=True)
try:
    h.wait(24)
    h.press("F7", delay=0.8); time.sleep(2.5)      # DISK C file window
    h.cmd("hatari-debug screenshot /tmp/gt-1.png"); time.sleep(2.0)
    h.press("F2", delay=0.8); time.sleep(3.0)      # grow
    h.cmd("hatari-debug screenshot /tmp/gt-2.png"); time.sleep(2.0)
    print("done", flush=True)
finally:
    print("rc", h.close(), flush=True)
for n in (1,2):
    im=Image.open("/tmp/gt-%d.png"%n).convert("RGB"); px=im.load()
    wh=sum(1 for x in range(0,640,2) for y in range(0,400,2) if px[x,y]==(255,255,255))
    print("gt-%d white px = %d" % (n, wh))
