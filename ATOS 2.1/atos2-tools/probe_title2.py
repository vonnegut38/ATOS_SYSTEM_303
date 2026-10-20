import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
h = Hatari(P, log=P + "/work/hatari-title3.log", window=True)
try:
    h.wait(20)
    h.press("F10", delay=0.3); time.sleep(0.8)
    h.press("Down", delay=0.2); time.sleep(0.4)
    h.press("Return", delay=0.3); time.sleep(0.8)
    h.type("WINDEMO", delay=0.15); time.sleep(0.4)
    h.press("Return", delay=0.3); time.sleep(5.0)
    d=h.read_bytes(0x437c0,4000,settle=1.0)
    rows=[]
    for r in range(25):
        row="".join(chr(d[(r*80+c)*2+1]) if 32<=d[(r*80+c)*2+1]<127 else " " for c in range(80))
        rows.append(row.rstrip())
    for i,row in enumerate(rows):
        if row.strip(): print("%2d|%s"%(i,row), flush=True)
finally:
    print("rc", h.close(), flush=True)
