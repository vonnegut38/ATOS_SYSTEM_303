import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
HB = "/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536"
IMG = P + "/work/atos-v2.img"
OUT = "/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/boot"
N = []
for i in range(8): N += ["--acsi", "%d=none" % i]
E = ["--machine","megast","--cpulevel","3","--cpuclock","32","--memsize","4","--ttram","64",
     "--tf536","on","--graphics-card","vofa","--gfx-chip","et4000","--mono","--ide-master",IMG]+N
h = Hatari(P, log=P+"/work/hatari-hist.log", window=True, image=IMG, binary=HB, cfg=True, extra=E)
def key(ch,d=0.18):
    h.keydown(ch, delay=d); time.sleep(0.05); h.keyup(ch, delay=d); time.sleep(0.06)
def drain(cmd, settle=0.9):
    try:
        h._capture.flush(); start=h.capture_path.stat().st_size
    except Exception: start=0
    h.cmd(cmd); time.sleep(settle)
    try: h._capture.flush()
    except Exception: pass
    with open(h.capture_path,"r",errors="replace") as fh:
        fh.seek(start); return fh.read()
try:
    time.sleep(30)
    for ch in "path": key(ch)
    key("\r",0.2); time.sleep(5)
    print("hist:", flush=True)
    print(drain("hatari-debug history 40"), flush=True)
finally:
    print("rc", h.close(), flush=True)
