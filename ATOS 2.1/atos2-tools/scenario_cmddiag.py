import re, sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
HB = "/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536"
IMG = P + "/work/atos-v2.img"
OUT = "/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/boot"
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
def readb(h,a):
    b=hx(h.read_mem(a,1,settle=0.3)); return b[0] if b else None
def read32(h,a):
    b=hx(h.read_mem(a,4,settle=0.3)); return int.from_bytes(b[:4],"big") if len(b)>=4 else None
def kscreen(h,sess):
    raw=hx(h.read_mem(0x00041158+sess*4000,4000,settle=0.6))
    chars="".join(chr(raw[2*i+1]) if 2*i+1<len(raw) and 32<=raw[2*i+1]<127 else " " for i in range(2000))
    return [chars[r*80:(r+1)*80].rstrip() for r in range(25) if chars[r*80:(r+1)*80].strip()]
def tap(h,k):
    h.keydown(k); time.sleep(0.18); h.keyup(k); time.sleep(0.12)

h = Hatari(P, log=P+"/work/hatari-cmddiag.log", window=False, image=IMG, binary=HB, cfg=True, extra=EXTRA)
try:
    time.sleep(28)
    h.type("command", delay=0.09); tap(h,"Return"); time.sleep(5)
    print("before: done=%s len=%s buf=%s kticks=%s" % (readb(h,0x38942), read32(h,0x40194), hex(read32(h,0x4019c) or 0), read32(h,0x3896c)), flush=True)
    h.type("dir", delay=0.09); tap(h,"Return"); time.sleep(4)
    print("after : done=%s len=%s buf=%s kticks=%s" % (readb(h,0x38942), read32(h,0x40194), hex(read32(h,0x4019c) or 0), read32(h,0x3896c)), flush=True)
    print("--- kernel console sess0 ---", flush=True)
    for l in kscreen(h,0)[-8:]: print("  |"+l, flush=True)
    print("shot", h.screenshot(OUT+"/cmddiag.png", settle=0.8), flush=True)
finally:
    print("rc", h.close(), flush=True)
