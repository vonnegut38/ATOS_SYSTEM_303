import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
from PIL import Image
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
A_DRAWS=0x4bbd8; A_OPENS=0x4bbd0; A_CLOSES=0x4bbd4; A_OPEN=0x4bbee; A_DRAWN=0x4bbdc
A_COMP=0x4bc78; A_FB=0x4be72
h = Hatari(P, log=P + "/work/hatari-dg2.log", window=True)
def u32(a): return int.from_bytes(h.read_bytes(a,4),"big")
def u8(a): return h.read_bytes(a,1)[0]
def state(tag):
    print("%s draws=%d opens=%d closes=%d list_open=%d drawn=%d composes=%d" %
          (tag,u32(A_DRAWS),u32(A_OPENS),u32(A_CLOSES),u8(A_OPEN),u8(A_DRAWN),u32(A_COMP)), flush=True)
def shot_strip(tag):
    h.cmd("hatari-debug screenshot /tmp/dg-%s.png"%tag); time.sleep(1.5)
    im=Image.open("/tmp/dg-%s.png"%tag).convert("L"); px=im.load()
    w=sum(1 for x in range(534,640) for y in range(0,16) if px[x,y]>128)
    print("   %s screen strip white px=%d (fewer = icons present)" % (tag,w), flush=True)
def fb_strip(tag):
    fb=u32(A_FB)
    data=h.read_bytes(fb+66, 80*16)
    bits=sum(bin(data[y*80+b]).count("1") for y in range(16) for b in range(14))
    print("   %s framebuffer strip white bits=%d" % (tag,bits), flush=True)
try:
    h.wait(24)
    h.press("F7", delay=0.8); time.sleep(2.5); state("after F7")
    h.press("F1", delay=0.8); time.sleep(1.5); state("list open"); fb_strip("open"); shot_strip("open")
    h.press("F1", delay=0.8); time.sleep(2.0); state("list closed"); fb_strip("closed"); shot_strip("closed")
    print("done", flush=True)
finally:
    print("rc", h.close(), flush=True)
