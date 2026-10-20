import re, sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
h = Hatari(P, log=P + "/work/hatari-68000-vec.log", window=True, extra=["--cpulevel","0"])
def words(a,n):
    t=h.read_mem(a,n,settle=0.5); d=[]
    for line in t.splitlines():
        if not re.match(r"^[0-9A-Fa-f]{8}:", line): continue
        for tok in line.split(":",1)[1][:48].split():
            if len(tok)==2 and all(c in "0123456789abcdefABCDEF" for c in tok): d.append(int(tok,16))
    return [int.from_bytes(bytes(d[k:k+4]),"big") for k in range(0,len(d)-3,4)]
try:
    h.wait(13)
    print("vec2 ", [hex(x) for x in words("0x8",4)], flush=True)
    print("vec3 ", [hex(x) for x in words("0xc",4)], flush=True)
    print("vec5 ", [hex(x) for x in words("0x14",4)], flush=True)
    print("stack", [hex(x) for x in words("0x300000",8)], flush=True)
finally:
    print("rc", h.close())
