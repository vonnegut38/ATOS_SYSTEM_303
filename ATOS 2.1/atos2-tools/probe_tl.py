import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
from PIL import Image
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
h = Hatari(P, log=P + "/work/hatari-tl.log", window=True)
def shot(tag):
    h.cmd("hatari-debug screenshot /tmp/tl-%s.png"%tag); time.sleep(1.5)
def strip(tag):
    im=Image.open("/tmp/tl-%s.png"%tag).convert("RGB"); px=im.load()
    c={}
    for x in range(520,640,2):
        for y in range(0,16,2): c[px[x,y]]=c.get(px[x,y],0)+1
    tot=sum(c.values())
    print("%s strip: white=%d%% black=%d%%"%(tag,c.get((255,255,255),0)*100//tot,c.get((0,0,0),0)*100//tot),flush=True)
    # list area
    c2={}
    for x in range(430,630,4):
        for y in range(18,60,3): c2[px[x,y]]=c2.get(px[x,y],0)+1
    tot2=sum(c2.values())
    print("    list area: white=%d%% black=%d%%"%(c2.get((255,255,255),0)*100//tot2,c2.get((0,0,0),0)*100//tot2),flush=True)
try:
    h.wait(24)
    h.press("F7", delay=0.8); time.sleep(2.5)      # DISK C window
    h.press("F1", delay=0.8); time.sleep(1.5)      # open task list
    shot("open"); strip("open")
    h.press("F1", delay=0.8); time.sleep(2.0)      # close task list
    shot("closed"); strip("closed")
    print("done", flush=True)
finally:
    print("rc", h.close(), flush=True)
