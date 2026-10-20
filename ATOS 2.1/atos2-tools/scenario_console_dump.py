import sys
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
h = Hatari(P, log=P + "/work/hatari-console-dump.log", window=True)
def u32(a):
    b = h.read_bytes(a, 4, settle=0.3)
    return int.from_bytes(b[:4], "big") if b and len(b) >= 4 else None
try:
    h.wait(15)
    h.type("run desktop.app", delay=0.12); h.press("\n", delay=0.1); h.wait(8.0)
    h.type("help", delay=0.15); h.press("\n", delay=0.15); h.wait(3.0)
    print("hidden", h.read_bytes(0x4624c,1,settle=0.2), "registered", h.read_bytes(0x46260,1,settle=0.2), flush=True)
    cs = u32(0x46248); ws0 = u32(0x46274)
    print("console_surface", hex(cs) if cs else None, "wm_surface[0]", hex(ws0) if ws0 else None, flush=True)
    for addr,name in [(cs,"console_surface"),(ws0,"wm_surface0")]:
        if addr:
            b = h.read_bytes(addr, 32000, settle=3.0)
            open(P + "/work/%s.bin" % name, "wb").write(b)
            print(name, "nonzero bytes", sum(1 for x in b if x))
finally:
    print("rc", h.close(), flush=True)
