import sys, struct, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
h = Hatari(P, log=P + "/work/hatari-probe2.log", window=True)
def u32(a):
    b = h.read_bytes(a,4,settle=0.25)
    return struct.unpack(">I", b)[0] if b and len(b)==4 else None
def u8(a):
    b = h.read_bytes(a,1,settle=0.25)
    return b[0] if b else None
def dump(tag):
    print(tag, "desktop_is_active", u8(0x3d055), "wm_ready", u8(0x472e4),
          "console_reg", u8(0x47258), "used", u8(0x472b2), u8(0x472b3), flush=True)
try:
    h.wait(15)
    h.type("run desktop.app", delay=0.12); h.press("\n", delay=0.1); h.wait(8.0)
    dump("after desktop")
    h.type("start windemo.app", delay=0.12); h.press("\n", delay=0.1); h.wait(6.0)
    dump("after windemo")
    s0 = u32(0x472bc); s1 = u32(0x472c0)
    print("surf0", hex(s0) if s0 else None, "surf1", hex(s1) if s1 else None, flush=True)
    for name,s in (("s0",s0),("s1",s1)):
        if s and s > 0x10000:
            data = h.read_bytes(s, 32000, settle=1.0)
            print(name,"nonzero", sum(1 for x in data if x), data[:48].hex(), flush=True)
    for ln in (h.console_lines(rows=25, settle=0.5) or []): print("|"+ln, flush=True)
except Exception as e:
    print("ERR", e, flush=True)
finally:
    print("rc", h.close(), flush=True)

