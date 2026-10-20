import sys, struct, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
h = Hatari(P, log=P + "/work/hatari-req.log", window=True)
ADDR = {"gfx":"0x3d0a4","total":"0x472ce","max":"0x472d2","last":"0x472d6","seen":"0x472da","frames":"0x472e2"}
def u32(a):
    b = h.read_bytes(int(a,16),4,settle=0.25)
    return struct.unpack(">I", b)[0] if b and len(b)==4 else None
def snap(tag):
    print(tag, {k:u32(v) for k,v in ADDR.items()}, flush=True)
try:
    h.wait(15)
    h.type("start desktop.app", delay=0.12); h.press("\n", delay=0.1); h.wait(8.0)
    snap("desktop")
    h.type("start windemo.app", delay=0.12); h.press("\n", delay=0.1); h.wait(6.0)
    snap("windemo")
    time.sleep(1.0)
    snap("settled")
except Exception as e:
    print("ERR", e, flush=True)
finally:
    print("rc", h.close(), flush=True)
