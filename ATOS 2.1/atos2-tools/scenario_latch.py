import re, subprocess, sys, time
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

def u32(h,addr):
    b=hx(h.read_mem(addr,4,settle=0.4)); return int.from_bytes(b[:4],"big") if len(b)>=4 else None
def kscreen(h):
    raw=hx(h.read_mem(0x000410f4, 4000, settle=0.7))
    chars="".join(chr(raw[2*i+1]) if 2*i+1<len(raw) and 32<=raw[2*i+1]<127 else " " for i in range(2000))
    return [chars[r*80:(r+1)*80].rstrip() for r in range(25) if chars[r*80:(r+1)*80].strip()]

print("-- boot 1 (card) --", flush=True)
h = Hatari(P, log=P+"/work/hatari-latch.log", window=False, image=IMG, binary=HB, cfg=True, extra=EXTRA)
try:
    time.sleep(30)
    print("nova/force:", u32(h,0x00039106+56+2), hex(u32(h,0x00039106+58) or 0), flush=True)
    for l in kscreen(h): print("  |"+l, flush=True)
finally:
    print("rc", h.close(), flush=True)
subprocess.run(["python3","tools/probe_bios_cfg.py"], cwd=P, check=True)
