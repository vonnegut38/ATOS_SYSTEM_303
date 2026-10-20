import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
from PIL import Image
from collections import Counter
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
G=0x00046744; VM=0x0004a838
h = Hatari(P, log=P + "/work/hatari-v8.log", window=True, extra=["--ttram","64"])
def st(tag):
    raw=h.read_bytes(G,16)
    w=int.from_bytes(raw[0:2],'big');hh=int.from_bytes(raw[2:4],'big')
    lb=int.from_bytes(raw[4:6],'big');pl=raw[6];mode=raw[7]
    print("%s w=%d h=%d line_bytes=%d planes=%d mode=%d vofa_mode=%d"%(
        tag,w,hh,lb,pl,mode,int.from_bytes(h.read_bytes(VM,2),'big')),flush=True)
try:
    h.wait(30); st("boot")
    h.cmd("hatari-debug screenshot /tmp/v8boot.png"); time.sleep(3.0)
    im=Image.open("/tmp/v8boot.png").convert("RGB"); px=im.load(); w,hh=im.size
    print("screenshot", im.size)
    print("row 300:", Counter(px[x,300] for x in range(0,w,16)).most_common(3))
    print("dither:", Counter(px[x,y] for x in range(100,200,4) for y in range(300,340,4)).most_common(3))
finally:
    print("rc", h.close(), flush=True)
