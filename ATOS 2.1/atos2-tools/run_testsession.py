import sys, time, os
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
OUT = "/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/testsession"
os.makedirs(OUT, exist_ok=True)
h = Hatari(P, log=P + "/work/hatari-testsession.log", window=True)
print("Hatari pid", h.proc.pid, flush=True)
try:
    h.wait(16)
    h.type("run command.app", delay=0.12); h.wait(0.6)
    h.press("\n", delay=0.1); h.wait(7)
    print("ready", h.screenshot(OUT + "/ready.png"), flush=True)
    print("READY - okno Hatari z Commanderem otwarte; czekam az je zamkniesz", flush=True)
    while h.proc.poll() is None:
        time.sleep(2)
finally:
    print("rc", h.close())
