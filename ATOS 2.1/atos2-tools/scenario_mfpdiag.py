import re, sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
HB = "/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536"
IMG = P + "/work/atos-v2.img"
NOACSI = []
for i in range(8): NOACSI += ["--acsi", "%d=none" % i]
EXTRA = ["--machine","megast","--cpulevel","3","--cpuclock","32","--memsize","4",
         "--ttram","64","--tf536","on","--graphics-card","vofa","--gfx-chip","et4000","--mono",
         "--ide-master", IMG] + NOACSI

def hx(t):
    out=[]
    for line in t.splitlines():
        if not re.match(r"^[0-9A-Fa-f]{8}:", line): continue
        for tok in line.split(":",1)[1][:48].split():
            if len(tok)==2 and all(c in "0123456789abcdefABCDEF" for c in tok): out.append(int(tok,16))
    return bytes(out)

def readb(h,addr):
    b=hx(h.read_mem(addr,1,settle=0.25)); return b[0] if b else None
def read32(h,addr):
    b=hx(h.read_mem(addr,4,settle=0.25)); return int.from_bytes(b[:4],"big") if len(b)>=4 else None
def bios_bottom(h):
    raw=hx(h.read_mem(0x00000d70,2000,settle=0.3))
    s="".join(chr(c) if 32<=c<127 else " " for c in raw)
    lines=[s[r*80:(r+1)*80].rstrip() for r in range(25)]
    return " | ".join([l for l in lines if l.strip()][-2:])[:150]

h = Hatari(P, log=P+"/work/hatari-mfpdiag.log", window=False, image=IMG, binary=HB, cfg=True, extra=EXTRA)
try:
    prev=0.0
    for t in (1,2,3,5,8,12):
        time.sleep(t-prev); prev=t
        tcdcr=readb(h,0xfffffa1d); tc=readb(h,0xfffffa23); td=readb(h,0xfffffa25)
        loaded=readb(h,0x00000ce3); err=readb(h,0x00000ce1)
        vrs=read32(h,0x00000d6c); kt=read32(h,0x00038918)
        print("t=%-2d TCDCR=%s TC(0x23)=%s TD(0x25)=%s loaded=%s err=%s vrs=%s kticks=%s | %s" % (
            t, hex(tcdcr) if tcdcr is not None else "?", tc, td, loaded, err,
            hex(vrs) if vrs is not None else "?", kt, bios_bottom(h)), flush=True)
finally:
    print("rc", h.close(), flush=True)
