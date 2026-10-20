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
def r32(h,a):
    b=hx(h.read_mem(a,4,settle=0.3)); return int.from_bytes(b[:4],"big") if len(b)>=4 else None
def kscreen(h):
    raw=hx(h.read_mem(0x0004124c,4000,settle=0.6))
    s="".join(chr(raw[2*i+1]) if 2*i+1<len(raw) and 32<=raw[2*i+1]<127 else " " for i in range(2000))
    return [s[r*80:(r+1)*80].rstrip() for r in range(25) if s[r*80:(r+1)*80].strip()]
def tap(h,k):
    h.keydown(k); time.sleep(0.28); h.keyup(k); time.sleep(0.2)
def cmd(h,t):
    h.type(t,delay=0.09); tap(h,"Return")

h = Hatari(P, log=P+"/work/hatari-hang.log", window=False, image=IMG, binary=HB, cfg=True, extra=EXTRA)
try:
    time.sleep(28)
    for label,text in [("run NOPE.APP","run NOPE.APP"),("run NOPE","run NOPE"),("start NOPE.APP","start NOPE.APP"),("memory","memory")]:
        a=r32(h,0x00038a58); time.sleep(0.1)
        cmd(h,text); time.sleep(3)
        b=r32(h,0x00038a58); time.sleep(1); c=r32(h,0x00038a58)
        print("%-16s kticks %s->%s->%s | %s" % (label,a,b,c, kscreen(h)[-1] if kscreen(h) else ""), flush=True)
    print("shot", h.screenshot(OUT+"/hang.png", settle=0.8), flush=True)
    print("--- last lines ---", flush=True)
    for l in kscreen(h)[-10:]: print("  |"+l, flush=True)
finally:
    print("rc", h.close(), flush=True)
