import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
HB = "/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536"
IMG = P + "/work/atos-v2.img"
OUT = "/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/boot"
NOACSI = []
for i in range(8): NOACSI += ["--acsi", "%d=none" % i]
def run(tag, machine, tos, cpl, clk):
    extra = ["--machine",machine,"--cpulevel",cpl,"--cpuclock",clk,"--memsize","4",
             "--ttram","64","--tf536","on","--graphics-card","vofa","--gfx-chip","et4000","--mono",
             "--ide-master", IMG, "--tos", tos] + NOACSI
    h = Hatari(P, log=P+"/work/hatari-bank-"+tag+".log", window=True, image=IMG, binary=HB, cfg=True, extra=extra)
    try:
        time.sleep(35)
        print(tag, h.screenshot(OUT+"/bank-"+tag+".png", settle=0.8), flush=True)
    finally:
        print(tag+"-rc", h.close(), flush=True)
run("st",    "st",     P+"/build/bios_st.bin",   "3", "32")
time.sleep(1)
run("mste",  "megaste",P+"/build/bios_mste.bin", "3", "32")
print("DONE", flush=True)
