import sys, struct
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
h = Hatari(P, log=P + "/work/hatari-wm-state.log", window=True)
def win(a):
    b = h.read_bytes(a, 28, settle=0.3)
    if not b or len(b) < 28: return None
    fx,fy,fw,fh,cx,cy,cw,ch = struct.unpack(">hhhhhhhh", b[0:16])
    pid = int.from_bytes(b[16:20], "big")
    flags = b[20]
    sx,sy = struct.unpack(">hh", b[22:26])
    return dict(frame=(fx,fy,fw,fh), client=(cx,cy,cw,ch), pid=hex(pid), flags=flags, src=(sx,sy))
try:
    h.wait(15)
    h.type("run wmdemo.app", delay=0.12); h.press("\n", delay=0.1); h.wait(9.0)
    base = 0x457a2
    for i in range(4):
        print("win[%d]" % i, win(base + i*28), flush=True)
    print("count", h.read_bytes(base+224, 4, settle=0.3), flush=True)
    print("clip_count", h.read_bytes(base+740, 8, settle=0.3), flush=True)
    print("screen_w/h", h.read_bytes(base+840, 4, settle=0.3), flush=True)
    print("surf[0..3]", [h.read_bytes(0x45788+i*4,4,settle=0.2).hex() for i in range(4)], flush=True)
finally:
    print("rc", h.close(), flush=True)
