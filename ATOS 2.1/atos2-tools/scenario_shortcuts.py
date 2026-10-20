import re, sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
CUR = "0x432bc"
h = Hatari(P, log=P + "/work/hatari-shortcuts.log", image=P + "/work/atos-v2.img")
def cur(): return h.read_u32(CUR, settle=0.3)
def raw_combo(gap):
    h.cmd("hatari-event keydown 42"); h.cmd("hatari-event keydown 56")
    h.cmd("hatari-event keypress 68")
    h.cmd("hatari-event keyup 56"); h.cmd("hatari-event keyup 42")
    if gap: time.sleep(gap)
def goto0():
    for _ in range(4):
        if cur()==0: return
        raw_combo(0.25); time.sleep(0.8)
try:
    h.wait(8)
    h.type("tts2", delay=0.09); h.press("\n", delay=0.09); h.wait(0.8)
    h.type("tts3", delay=0.09); h.press("\n", delay=0.09); h.wait(0.8)
    print("start", cur(), flush=True)
    for n in (1,2,3):
        goto0(); start=cur()
        for _ in range(n): raw_combo(0.35)
        time.sleep(1.0); print(f"baseline n={n}: {start} -> {cur()}", flush=True)
    for n in (2,3,6):
        goto0(); start=cur()
        for _ in range(n): raw_combo(0)
        time.sleep(1.5); print(f"fast n={n}: {start} -> {cur()}", flush=True)
    for n in (2,3,6):
        goto0(); start=cur()
        for _ in range(n): raw_combo(0.08)
        time.sleep(1.5); print(f"mid n={n}: {start} -> {cur()}", flush=True)
finally:
    print("rc", h.close())
