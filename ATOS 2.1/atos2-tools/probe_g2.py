import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
from PIL import Image
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
h = Hatari(P, log=P + "/work/hatari-g2.log", window=True)
def sample(tag):
    import subprocess
    path="/tmp/g2-%s.png"%tag
    # screenshot is taken by the harness through the fifo
    h.cmd("hatari-debug screenshot "+path); time.sleep(1.2)
    im=Image.open(path).convert("RGB"); px=im.load()
    # newly exposed area: x 530..620, y 350..430 (inside new frame, outside old)
    c={}
    for x in range(530,624,6):
        for y in range(348,396,4):
            c[px[x,y]]=c.get(px[x,y],0)+1
    tot=sum(c.values())
    print("%s: white=%d%% black=%d%% other=%s" % (tag, c.get((255,255,255),0)*100//tot,
          c.get((0,0,0),0)*100//tot, sorted(c.items(), key=lambda kv:-kv[1])[:3]), flush=True)
try:
    h.wait(24)
    h.press("F7", delay=0.8); time.sleep(2.5)
    h.press("F2", delay=0.0)
    time.sleep(0.5); sample("0s")
    time.sleep(3.0); sample("3s")
finally:
    print("rc", h.close(), flush=True)
