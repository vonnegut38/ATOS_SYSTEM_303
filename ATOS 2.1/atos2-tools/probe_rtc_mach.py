import sys, time, datetime
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
for mach in ("ste","megaste","tt","falcon"):
    try:
        h = Hatari(P, log=P + ("/work/hatari-rtc-%s.log"%mach), window=True, extra=["--machine",mach])
        try:
            h.wait(20)
            print(mach, "host", datetime.datetime.now().strftime("%H:%M"),
                  h.screenshot("/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/rtc-%s.png"%mach, settle=0.6), flush=True)
        finally:
            h.close()
    except Exception as e:
        print(mach, "ERR", e, flush=True)
