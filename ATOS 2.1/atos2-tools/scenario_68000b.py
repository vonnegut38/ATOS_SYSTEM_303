import re, sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
OUT = "/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/cpu"
h = Hatari(P, log=P + "/work/hatari-68000b.log", window=True, extra=["--cpulevel","0"])
def w(a):
    t=h.read_mem(a,4,settle=0.35); d=[]
    for line in t.splitlines():
        if not re.match(r"^[0-9A-Fa-f]{8}:", line): continue
        for tok in line.split(":",1)[1][:48].split():
            if len(tok)==2 and all(c in "0123456789abcdefABCDEF" for c in tok): d.append(int(tok,16))
    return int.from_bytes(bytes(d[:4]),"big") if len(d)>=4 else None
try:
    h.wait(13)
    h.type("run busflt.app", delay=0.1); h.press("\n", delay=0.1); h.wait(2.5)
    print("after busflt: vec",w("0x3980a"),"pc",w("0x39806"),"sr",w("0x39802"), flush=True)
    print("shot", h.screenshot(OUT+"/c68000-busflt.png"), flush=True)
finally:
    print("rc", h.close())
