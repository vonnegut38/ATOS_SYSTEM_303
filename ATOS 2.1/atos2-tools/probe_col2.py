import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
G=0x000465e0; VM=0x0004a6d4
h = Hatari(P, log=P + "/work/hatari-col3.log", window=True, extra=["--ttram","64"])
def st(tag):
    raw=h.read_bytes(G,16)
    w=int.from_bytes(raw[0:2],'big');hh=int.from_bytes(raw[2:4],'big')
    lb=int.from_bytes(raw[4:6],'big');pl=raw[6];mode=raw[7]
    caps=int.from_bytes(raw[8:10],'big')
    vm=int.from_bytes(h.read_bytes(VM,2),'big')
    print("%s w=%d h=%d line=%d planes=%d mode=%d caps=0x%02x vofa_mode=%d"%(tag,w,hh,lb,pl,mode,caps,vm),flush=True)
try:
    h.wait(26); st("boot")
    h.press("F7", delay=1.0); time.sleep(2.5); st("F7")
    h.press("F3", delay=1.0); time.sleep(3.0); st("F3")
finally:
    print("rc", h.close(), flush=True)
