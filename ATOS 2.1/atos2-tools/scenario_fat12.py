import re, subprocess, sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
HB = "/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536"
IMG = P + "/work/atos-v2.img"
FLOP = "/tmp/atos-floppy.st"
OUT = "/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/boot"
NOACSI = []
for i in range(8): NOACSI += ["--acsi", "%d=none" % i]
EXTRA = ["--machine","megast","--cpulevel","3","--cpuclock","32","--memsize","4",
         "--ttram","64","--tf536","on","--graphics-card","vofa","--gfx-chip","et4000","--mono",
         "--ide-master", IMG, "--disk-a", FLOP] + NOACSI
def hx(t):
    out=[]
    for line in t.splitlines():
        if not re.match(r"^[0-9A-Fa-f]{8}:", line): continue
        for tok in line.split(":",1)[1][:48].split():
            if len(tok)==2 and all(c in "0123456789abcdefABCDEF" for c in tok): out.append(int(tok,16))
    return bytes(out)
def r32(h,a):
    b=hx(h.read_mem(a,4,settle=0.4)); return int.from_bytes(b[:4],"big") if len(b)>=4 else None
def tap(h,k):
    h.keydown(k); time.sleep(0.3); h.keyup(k); time.sleep(0.25)
subprocess.run(["python3","tools/mk_floppy.py","/tmp/atos-floppy.st"], cwd=P, check=True)
subprocess.run(["python3","tools/make_bios_cfg.py","work/BIOS.CFG","--auto-tos","0","--boot-delay","15"], cwd=P, check=True)
subprocess.run(["python3","tools/install_boot_sys.py","work/atos-v2.img","work/BIOS.CFG","--directory","BIOS","--data"], cwd=P, check=True)
h = Hatari(P, log=P+"/work/hatari-fat12.log", window=True, image=IMG, binary=HB, cfg=True, extra=EXTRA)
try:
    time.sleep(4.5); tap(h,"F3"); time.sleep(4.0)
    st=r32(h,0x00000e70); sig=r32(h,0x00000e6c)
    txt = bytes.fromhex("%08x"%sig).decode("latin1") if sig else None
    print("fat12_dbg_status=%s fat12_dbg_sig=%s (%r)" % (st, hex(sig) if sig is not None else None, txt), flush=True)
    print("shot", h.screenshot(OUT+"/fat12-probe.png", settle=0.8), flush=True)
finally:
    print("rc", h.close(), flush=True)
subprocess.run(["python3","tools/make_bios_cfg.py","work/BIOS.CFG","--auto-tos","0","--boot-delay","1"], cwd=P, check=True)
for extra in (["--directory","BIOS","--data"],["--root","--data"]):
    subprocess.run(["python3","tools/install_boot_sys.py","work/atos-v2.img","work/BIOS.CFG"]+extra, cwd=P, check=True)
print("DONE", flush=True)
