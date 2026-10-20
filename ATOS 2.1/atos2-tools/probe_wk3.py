import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
BASE = 0x0003f6f4
STRIDE = 834
def dump(h, tag):
    raw = h.read_bytes(BASE, STRIDE*6)
    out=[]
    for i in range(6):
        s = raw[i*STRIDE:(i+1)*STRIDE]
        if len(s) < 700: continue
        pid = int.from_bytes(s[8:12], 'big')
        state = int.from_bytes(s[132:136], 'big')
        name = s[656:668].split(b'\x00')[0].decode('latin1','replace').strip()
        out.append((i, pid, state, name))
    live = [r for r in out if r[1]]
    print(tag, "live:", live, flush=True)
    return live
h = Hatari(P, log=P + "/work/hatari-wk3.log", window=True)
try:
    h.wait(24)
    dump(h, "boot")
    h.press("F5", delay=0.7); time.sleep(4.5)
    a = dump(h, "after1")
    h.press("F5", delay=0.7); time.sleep(4.0)
    b = dump(h, "after2")
    ac_a = [r for r in a if r[3].startswith('AC')]
    ac_b = [r for r in b if r[3].startswith('AC')]
    print("AC count after1=%d after2=%d" % (len(ac_a), len(ac_b)), flush=True)
finally:
    print("rc", h.close(), flush=True)
