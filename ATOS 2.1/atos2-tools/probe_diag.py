import sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
h = Hatari(P, log=P + "/work/hatari-diag.log", window=True)
def screen(): return "\n".join(h.console_lines(rows=25, settle=0.5) or [])
try:
    h.wait(18)
    print("=== before ===", flush=True)
    for ln in (h.console_lines(rows=8, settle=0.5) or []): print("|"+ln, flush=True)
    h.type("start desktop.app", delay=0.12); h.press("\n", delay=0.1); h.wait(4.0)
    print("=== after 4s ===", flush=True)
    for ln in (h.console_lines(rows=12, settle=0.5) or []): print("|"+ln, flush=True)
    h.wait(5.0)
    print("=== after 9s ===", flush=True)
    for ln in (h.console_lines(rows=12, settle=0.5) or []): print("|"+ln, flush=True)
    print("shot:", h.screenshot("/Users/xxx/Documents/Deepseek_test/atos2-tools/shots/diag-clock.png", settle=0.6), flush=True)
except Exception as e:
    print("ERR", e, flush=True)
finally:
    print("rc", h.close(), flush=True)
