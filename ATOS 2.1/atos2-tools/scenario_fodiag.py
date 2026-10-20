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

h = Hatari(P, log=P+"/work/hatari-fodiag.log", window=False, image=IMG, binary=HB, cfg=True, extra=EXTRA)
def dump(tag):
    g = hx(h.read_mem(0x00000ce1, 40, settle=0.6))
    gs = hx(h.read_mem(0x00001546, 76, settle=0.6))
    vrs = hx(h.read_mem(0x00000d6c, 4, settle=0.5))
    rh = hx(h.read_mem(0x00039106, 64, settle=0.5))
    print("---", tag, flush=True)
    print("  g_config_error=%d saved=%d loaded=%d" % (g[0], g[1], g[2]), flush=True)
    cfg = g[3:3+32]
    print("  g_config magic=%s ver=%d size=%d vdrv=%d vpend=%d bootdev=%d crc=%s" % (
        cfg[0:4].hex(), int.from_bytes(cfg[4:6],"big"), int.from_bytes(cfg[6:8],"big"),
        cfg[13], cfg[27], cfg[12], cfg[8:12].hex()), flush=True)
    print("  g_system nova_present=%d adapter=%d chipset=%d nova_init=%d vpref=%d vact=%d vforce=%d ram=%d ttram=%d" % (
        gs[14], gs[15], gs[16], gs[17], gs[18], gs[19], gs[20],
        int.from_bytes(gs[64:68],"big"), int.from_bytes(gs[68:72],"big")), flush=True)
    print("  vofa_rom_status=0x%08x" % int.from_bytes(vrs,"big"), flush=True)
    print("  runtime_handoff v58=%d v59=%d (bytes %s)" % (rh[58], rh[59], rh[54:64].hex()), flush=True)

try:
    h.wait(2.0); dump("t=2")
    time.sleep(10); dump("t=12")
    time.sleep(18); dump("t=30")
finally:
    print("rc", h.close(), flush=True)
