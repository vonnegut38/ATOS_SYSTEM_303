import re, struct, sys, time
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
def rstr(h,a,n):
    b=hx(h.read_mem(a,n,settle=0.35)); s=b.split(b"\x00")[0]; return s.decode("latin1")
def tap(h,k):
    h.keydown(k); time.sleep(0.25); h.keyup(k); time.sleep(0.15)
h = Hatari(P, log=P+"/work/hatari-cmddiag3.log", window=False, image=IMG, binary=HB, cfg=True, extra=EXTRA)
try:
    time.sleep(28)
    h.type("command", delay=0.09); tap(h,"Return"); time.sleep(5)
    svc=r32(h,0x00044082)
    print("service_ptr=%s" % (hex(svc) if svc else None), flush=True)
    if svc:
        base=svc-0x2660
        print("base=%s version=%s size=%s" % (hex(base), rb(h,svc+4), rb(h,svc+6)), flush=True)
        print("submit=%s expect=%s" % (hex(r32(h,svc+32) or 0), hex(base+0x1786)), flush=True)
        print("run_pending=%s expect=%s" % (hex(r32(h,svc+36) or 0), hex(base+0x17a8)), flush=True)
        print("flag_before=%s pending=%r" % (rb(h,base+0x296c), rstr(h,base+0x292c,16)), flush=True)
    h.type("dir", delay=0.09); tap(h,"Return"); time.sleep(2.0)
    if svc:
        base=svc-0x2660
        print("AFTER flag=%s pending=%r capture_done=%s len=%s" % (
            rb(h,base+0x296c), rstr(h,base+0x292c,16), rb(h,0x38942), r32(h,0x40194)), flush=True)
    time.sleep(3)
    if svc:
        base=svc-0x2660
        print("LATER flag=%s pending=%r capture_done=%s len=%s" % (
            rb(h,base+0x296c), rstr(h,base+0x292c,16), rb(h,0x38942), r32(h,0x40194)), flush=True)
finally:
    print("rc", h.close(), flush=True)
