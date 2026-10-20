import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
G=0x00046614; MR=0x0003ec38; VM=0x0004a708
h = Hatari(P, log=P + "/work/hatari-col6.log", window=True, extra=["--ttram","64"])
def s32(a): return int.from_bytes(h.read_bytes(a,4),"big",signed=True)
def st(tag):
    raw=h.read_bytes(G,16)
    w=int.from_bytes(raw[0:2],'big');hh=int.from_bytes(raw[2:4],'big')
    lb=int.from_bytes(raw[4:6],'big');mode=raw[7]
    print("%s w=%d h=%d line=%d mode=%d | result=%d vofa_mode=%d"%(
        tag,w,hh,lb,mode,s32(MR),int.from_bytes(h.read_bytes(VM,2),'big')),flush=True)
try:
    h.wait(26); st("boot")
    h.press("F3", delay=1.0); time.sleep(3.0); st("F3-800mono")
    h.cmd("hatari-debug screenshot /tmp/mono800.png"); time.sleep(2.5)
finally:
    print("rc", h.close(), flush=True)
