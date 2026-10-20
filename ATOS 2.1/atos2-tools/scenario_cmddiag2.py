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
def r32(h,a):
    b=hx(h.read_mem(a,4,settle=0.3)); return int.from_bytes(b[:4],"big") if len(b)>=4 else None
def rb(h,a):
    b=hx(h.read_mem(a,1,settle=0.25)); return b[0] if b else None
def tap(h,k):
    h.keydown(k); time.sleep(0.18); h.keyup(k); time.sleep(0.12)
h = Hatari(P, log=P+"/work/hatari-cmddiag2.log", window=False, image=IMG, binary=HB, cfg=True, extra=EXTRA)
try:
    time.sleep(28)
    h.type("command", delay=0.09); tap(h,"Return"); time.sleep(5)
    h.type("dir", delay=0.09); tap(h,"Return"); time.sleep(3)
    a=r32(h,0x0004408a); d1=rb(h,0x38942); l1=r32(h,0x40194)
    time.sleep(2)
    b=r32(h,0x0004408a); d2=rb(h,0x38942); l2=r32(h,0x40194)
    print("loop_calls: %s -> %s (delta %s)" % (a,b,(b-a) if (a is not None and b is not None) else "?"), flush=True)
    print("capture: done %s->%s len %s->%s" % (d1,d2,l1,l2), flush=True)
finally:
    print("rc", h.close(), flush=True)
