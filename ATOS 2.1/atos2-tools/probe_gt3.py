import sys, time, struct
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
WM=0x4bc46; STRIDE=60
h = Hatari(P, log=P + "/work/hatari-gt3.log", window=True)
def wins(tag):
    raw=h.read_bytes(WM,480)
    for i in range(4):
        s=raw[i*STRIDE:(i+1)*STRIDE]
        if len(s)<20: continue
        fx,fy,fw,fh=struct.unpack(">hhhh", s[0:8])
        cx,cy,cw,ch=struct.unpack(">hhhh", s[8:16])
        pid=struct.unpack(">I", s[16:20])[0]
        fl=s[20]
        if fw or fh:
            print("%s win%d frame=(%d,%d,%d,%d) client=(%d,%d,%d,%d) pid=%d flags=%d"%(tag,i,fx,fy,fw,fh,cx,cy,cw,ch,pid,fl), flush=True)
try:
    h.wait(24)
    wins("boot")
    h.press("F7", delay=0.8); time.sleep(2.5); wins("open")
    h.press("F2", delay=0.8); time.sleep(3.0); wins("grow")
finally:
    print("rc", h.close(), flush=True)
