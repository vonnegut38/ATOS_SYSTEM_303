import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
HB = "/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536"
IMG = P + "/work/atos-v2.img"
OUT = "/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/boot"
EXTRA = ["--machine","st","--cpulevel","0","--cpuclock","8","--memsize","4",
         "--tos", P+"/build/bios_st.bin"]
h = Hatari(P, log=P+"/work/hatari-bank-st2.log", window=True, image=IMG, binary=HB, cfg=True, extra=EXTRA)
try:
    time.sleep(18)
    print("st2", h.screenshot(OUT+"/bank-st2.png", settle=0.8), flush=True)
finally:
    print("rc", h.close(), flush=True)
