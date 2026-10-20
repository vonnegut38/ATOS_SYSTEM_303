import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
from PIL import Image
from collections import Counter
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
G=0x00046614; MR=0x0003ec38; VM=0x0004a708
h = Hatari(P, log=P + "/work/hatari-col7.log", window=True, extra=["--ttram","64"])
def s32(a): return int.from_bytes(h.read_bytes(a,4),"big",signed=True)
def st(tag):
    raw=h.read_bytes(G,16)
    w=int.from_bytes(raw[0:2],'big');hh=int.from_bytes(raw[2:4],'big')
    lb=int.from_bytes(raw[4:6],'big');mode=raw[7]
    print("%s w=%d h=%d line=%d mode=%d | result=%d vofa_mode=%d"%(
        tag,w,hh,lb,mode,s32(MR),int.from_bytes(h.read_bytes(VM,2),'big')),flush=True)
try:
    h.wait(30); st("after boot")
    h.cmd("hatari-debug screenshot /tmp/colboot.png"); time.sleep(3.0)
    im=Image.open("/tmp/colboot.png").convert("RGB"); px=im.load(); w,hh=im.size
    print("shot", im.size)
    print("row 300:", Counter(px[x,300] for x in range(0,w,8)).most_common(4))
    print("row 60 :", Counter(px[x,60] for x in range(0,w,8)).most_common(4))
finally:
    print("rc", h.close(), flush=True)
