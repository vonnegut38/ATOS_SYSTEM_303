import sys
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
HB = "/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536"
IMG = P + "/work/atos-v2.img"
OUT = "/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/boot"
EXTRA = ["--machine","st","--cpulevel","0","--cpuclock","8","--memsize","4",
         "--ttram","0","--graphics-card","none","--mono",
         "--tos", P + "/build/bios_st.bin",
         "--acsi","0=" + IMG,"--acsi","1=none","--acsi","2=none","--acsi","3=none",
         "--acsi","4=none","--acsi","5=none","--acsi","6=none","--acsi","7=none"]
h = Hatari(P, log=P + "/work/hatari-st2.log", window=True, image=IMG, binary=HB, cfg=True, extra=EXTRA)
try:
    h.wait(30)
    print("st1", h.screenshot(OUT + "/st1.png", settle=0.8), flush=True)
finally:
    print("rc", h.close())
