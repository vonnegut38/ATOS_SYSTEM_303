import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
extra = ["--machine","megast","--cpuclock","32","--ttram","64","--tf536","on",
         "--graphics-card","vofa","--gfx-chip","et4000"]
ADDR = {"total":0x4b956,"cursor":0x4b95a,"deco":0x4b95e,"recomp":0x4b962,
        "composes":0x4b96e,"frames":0x4a5aa}
h = Hatari(P, log=P+"/work/hatari-perf.log", window=False, extra=extra)
def rd(a): return int.from_bytes(h.read_bytes(a,4),"big")
def snap(): return {k: rd(v) for k,v in ADDR.items()}
def diff(a,b): return {k: b[k]-a[k] for k in ADDR}
try:
    h.wait(32)
    a0=snap(); time.sleep(4.0); a1=snap()
    print("640x400 idle 4s:", diff(a0,a1), flush=True)
    # switch to 1024x768
    h.press("F10", delay=0.7); h.press("Return", delay=1.2)
    h.press("Down", delay=0.6); h.press("Down", delay=0.6)
    t=time.time(); h.press("Return", delay=0.5); time.sleep(6.0)
    print("mode switch wall", round(time.time()-t,2), flush=True)
    b0=snap(); time.sleep(4.0); b1=snap()
    print("1024x768 idle 4s:", diff(b0,b1), flush=True)
    d=diff(b0,b1)
    if d["composes"]: print("cycles/compose 1024 =", d["recomp"]//d["composes"], flush=True)
    da=diff(a0,a1)
    if da["composes"]: print("cycles/compose 640 =", da["recomp"]//da["composes"], flush=True)
finally:
    print("rc", h.close(), flush=True)
