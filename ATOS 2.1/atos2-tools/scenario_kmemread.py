import re, sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
h = Hatari(P, log=P + "/work/hatari-kmemread.log", window=True)
def words(text):
    data=[]
    for line in text.splitlines():
        if not re.match(r"^[0-9A-Fa-f]{8}:", line): continue
        for tok in line.split(":",1)[1][:48].split():
            if len(tok)==2 and all(c in "0123456789abcdefABCDEF" for c in tok): data.append(int(tok,16))
    return [int.from_bytes(bytes(data[k:k+4]),"big") for k in range(0,len(data)-3,4)]
try:
    h.wait(9)
    w = words(h.read_mem("0x984e6", 324, settle=0.9))
    n = w[0]
    print("owners:", n, flush=True)
    for i in range(n):
        print(f"  owner {w[1+3*i]:5d}  {w[2+3*i]:8d} B  x{w[3+3*i]}", flush=True)
    b=1+3*24
    print(f"ST total={w[b]} free={w[b+1]} reserved={w[b+2]} largest={w[b+3]}", flush=True)
    print(f"TT total={w[b+4]} free={w[b+5]} reserved={w[b+6]} largest={w[b+7]}", flush=True)
finally:
    print("rc", h.close())
