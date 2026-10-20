import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
from PIL import Image
from collections import Counter
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
def btn(path):
    im = Image.open(path).convert('RGB'); px = im.load()
    c = Counter(px[x,y] for x in range(84,97) for y in range(378,397))
    tot = sum(c.values())
    return c.get((255,255,255),0)*100//tot, c.get((0,0,0),0)*100//tot
h = Hatari(P, log=P + "/work/hatari-st2.log", window=True)
try:
    h.wait(24)
    h.press("F9", delay=0.6); time.sleep(1.5)
    h.cmd("hatari-debug screenshot /tmp/s2-0.png"); time.sleep(2.0)
    h.press("F5", delay=0.7); time.sleep(4.5)
    h.press("F9", delay=0.6); time.sleep(1.5)
    h.cmd("hatari-debug screenshot /tmp/s2-1.png"); time.sleep(2.0)
    h.press("F6", delay=0.7); time.sleep(3.0)
    h.press("F9", delay=0.6); time.sleep(1.5)
    h.cmd("hatari-debug screenshot /tmp/s2-2.png"); time.sleep(2.0)
    h.press("F5", delay=0.7); time.sleep(3.0)
    h.press("F9", delay=0.6); time.sleep(1.5)
    h.cmd("hatari-debug screenshot /tmp/s2-3.png"); time.sleep(2.0)
    print("done", flush=True)
finally:
    print("rc", h.close(), flush=True)
for i,tag in enumerate(["idle -> white","running -> black","minimised -> grey","clicked again -> black"]):
    w,b = btn("/tmp/s2-%d.png" % i)
    print("%-28s white=%3d%% black=%3d%%" % (tag, w, b), flush=True)
