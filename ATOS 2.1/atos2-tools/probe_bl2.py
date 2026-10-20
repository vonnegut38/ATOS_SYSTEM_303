import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
G=0x00046740
h = Hatari(P, log=P + "/work/hatari-bl.log", window=False)
try:
    h.wait(24)
    raw=h.read_bytes(G, 16)
    w=int.from_bytes(raw[0:2],'big'); hh=int.from_bytes(raw[2:4],'big')
    planes=raw[6]; mode=raw[7]; caps=int.from_bytes(raw[8:10],'big')
    print("graphics w=%d h=%d planes=%d mode=%d caps=0x%04x" % (w,hh,planes,mode,caps), flush=True)
    print("  BLITTER_PRESENT=%d BLITTER_ENABLED=%d COPY=%d" % (bool(caps&0x4), bool(caps&0x8), bool(caps&0x40)), flush=True)
finally:
    print("rc", h.close(), flush=True)
