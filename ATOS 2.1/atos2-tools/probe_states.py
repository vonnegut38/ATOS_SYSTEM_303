import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
from PIL import Image
from collections import Counter
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"

def btn(path):
    im = Image.open(path).convert('RGB')
    # COMMANDER button interior: x 84..96, y 378..396 (right of the label)
    px = im.load()
    c = Counter(px[x,y] for x in range(84,97) for y in range(378,397))
    tot = sum(c.values())
    white = c.get((255,255,255),0); black = c.get((0,0,0),0)
    return white*100//tot, black*100//tot

h = Hatari(P, log=P + "/work/hatari-st.log", window=True)
try:
    h.wait(24)
    h.cmd("hatari-debug screenshot /tmp/st-0.png"); time.sleep(2.0)
    h.press("F5", delay=0.7); time.sleep(4.5)
    h.cmd("hatari-debug screenshot /tmp/st-1.png"); time.sleep(2.0)
    h.press("F6", delay=0.7); time.sleep(3.0)      # minimize AC via its gadget
    h.cmd("hatari-debug screenshot /tmp/st-2.png"); time.sleep(2.0)
    h.press("F5", delay=0.7); time.sleep(3.0)      # launcher click -> should SHOW
    h.cmd("hatari-debug screenshot /tmp/st-3.png"); time.sleep(2.0)
    print("done", flush=True)
finally:
    print("rc", h.close(), flush=True)
for i,tag in enumerate(["idle (white expected)","after launch (black expected)","after minimize (grey expected)","after click again (black expected)"]):
    w,b = btn("/tmp/st-%d.png" % i)
    print("st-%d %-38s white=%d%% black=%d%%" % (i, tag, w, b), flush=True)
