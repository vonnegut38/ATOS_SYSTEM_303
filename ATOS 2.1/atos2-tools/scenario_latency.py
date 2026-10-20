import sys, os, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
OUT = "/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/latency"
os.makedirs(OUT, exist_ok=True)
for f in os.listdir(OUT):
    try: os.remove(os.path.join(OUT, f))
    except OSError: pass
h = Hatari(P, log=P + "/work/hatari-latency.log", window=True)
try:
    h.wait(15)
    h.screenshot(OUT + "/base.png", settle=0.4)
    t0 = time.time()
    h.type("run command.app", delay=0.07); h.press("\n", delay=0.0)
    t0 = time.time()
    print("t0 %.3f" % t0, flush=True)
    for i in range(70):
        h.wait(0.25)
        h.screenshot(OUT + "/f%03d.png" % i, settle=0.25)
        print("%03d %.3f" % (i, time.time() - t0), flush=True)
finally:
    print("rc", h.close())
