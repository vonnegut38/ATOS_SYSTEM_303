import re, subprocess, sys, time
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
def bios_text(h):
    raw=hx(h.read_mem(0x00000d70,2000,settle=0.4))
    return "".join(chr(c) if 32<=c<127 else " " for c in raw)
def tap(h,k):
    h.keydown(k); time.sleep(0.28); h.keyup(k); time.sleep(0.2)

print("=== TEST 1: cd moves the active panel ===", flush=True)
h = Hatari(P, log=P+"/work/hatari-cd.log", window=True, image=IMG, binary=HB, cfg=True, extra=EXTRA)
try:
    time.sleep(28)
    h.type("command", delay=0.1); tap(h,"Return"); time.sleep(5)
    h.type("cd \\SYSTEM", delay=0.1); tap(h,"Return"); time.sleep(3)
    tap(h,"F3"); time.sleep(1.5)
    print("cd-shot", h.screenshot(OUT+"/cd-panel.png", settle=0.8), flush=True)
finally:
    print("rc1", h.close(), flush=True)

print("=== TEST 2: remembered device gone -> fallback scan boots ===", flush=True)
subprocess.run(["python3","tools/make_bios_cfg.py","work/BIOS.CFG","--auto-tos","0",
                "--boot-delay","3","--boot-id","0"], cwd=P, check=True)
subprocess.run(["python3","tools/install_boot_sys.py","work/atos-v2.img","work/BIOS.CFG",
                "--directory","BIOS","--data"], cwd=P, check=True)
h = Hatari(P, log=P+"/work/hatari-bootfb.log", window=True, image=IMG, binary=HB, cfg=True, extra=EXTRA)
try:
    snaps=[];pos=0.0
    for t in (1.3,1.8,2.4):
        time.sleep(max(0.0,t-pos));pos=t
        snaps.append(bios_text(h))
    txt="\n".join(snaps)
    print("BIOS has REMEMBERED:", "REMEMBERED BOOT DEVICE UNAVAILABLE" in txt, flush=True)
    time.sleep(34)
    print("boot-shot", h.screenshot(OUT+"/bootfb.png", settle=0.8), flush=True)
finally:
    print("rc2", h.close(), flush=True)

print("=== restore clean config ===", flush=True)
subprocess.run(["python3","tools/make_bios_cfg.py","work/BIOS.CFG","--auto-tos","0","--boot-delay","1"], cwd=P, check=True)
for extra in (["--directory","BIOS","--data"],["--root","--data"]):
    subprocess.run(["python3","tools/install_boot_sys.py","work/atos-v2.img","work/BIOS.CFG"]+extra, cwd=P, check=True)
print("DONE", flush=True)
