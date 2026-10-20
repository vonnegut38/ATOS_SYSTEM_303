import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
HB = "/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536"
FLOP = "/tmp/atos-boot.st"
OUT = "/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/boot"
N = []
for i in range(8): N += ["--acsi", "%d=none" % i]
E = ["--machine","megast","--cpulevel","3","--cpuclock","32","--memsize","4","--ttram","64",
     "--tf536","on","--graphics-card","vofa","--gfx-chip","et4000","--mono","--disk-a",FLOP]+N
h = Hatari(P, log=P+"/work/hatari-hostile.log", window=True, image=P+"/work/atos-v2.img", binary=HB, cfg=True, extra=E)
def key(ch,d=0.17):
    h.keydown(ch, delay=d); time.sleep(0.05); h.keyup(ch, delay=d); time.sleep(0.06)
def typ(t,d=0.17):
    for ch in t: key(ch,d)
def line(t, tag, wait=3.5):
    typ(t); time.sleep(0.4); key("\r",0.2); time.sleep(wait)
    print(tag, h.screenshot(OUT+"/hostile-"+tag+".png", settle=0.8), flush=True)
try:
    time.sleep(205)
    line("dir C:\\SYSTEM","01-dirC",4)
    line("cd C:\\SYSTEM","02-cdC",3)
    line("pwd","03-pwd",3)
    line("path C:\\SYSTEM","04-pathC",4)
    line("path","05-path",4)
    line("run NOSUCH.APP","06-nosuch",4)
    line("run C:\\SYSTEM\\APP\\HW.APP","07-runC",6)
    line("path \\NOPE","08-nope",4)
    line("path","09-path2",4)
    line("run","10-run",3)
finally:
    print("rc", h.close(), flush=True)
