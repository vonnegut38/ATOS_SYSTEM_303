#!/usr/bin/env python3
"""Attribute ACIA overruns to main-loop sections and the Timer-C handler,
using the temporary gap counters in work/atos-v2-gap.img."""
import re, sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari

P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.0"
IMG = P + "/work/atos-v2-gap.img"
PENDING = "0x9a6ba"
OVERRUN = "0x9a6fa"
DROPPED = "0x9a73a"
CYCLES = int(sys.argv[1]) if len(sys.argv) > 1 else 16
NAMES = ["0 timer/switch", "1 input poll", "2 ksessions", "3 consumer loop",
         "4 notify", "5 dispatch ready", "6 dispatch output", "7 ksessions2",
         "8 kconsole_flush", "9 wm_frame_tick", "10 wm_applet_bar", "11 ksessions_paint",
         "12 kvideo_flush", "13", "14 Timer-C handler", "15"]

h = Hatari(P, log=P + "/work/hatari-gap2.log", image=IMG)
def stamp(m): print(m, flush=True)
def sw(): h.combo(["Shift", "Alt", "F10"], delay=0.15)

def words(text):
    data = []
    for line in text.splitlines():
        if not re.match(r"^[0-9A-Fa-f]{8}:", line):
            continue
        for tok in line.split(":", 1)[1][:48].split():
            if len(tok) == 2 and all(c in "0123456789abcdefABCDEF" for c in tok):
                data.append(int(tok, 16))
    return [int.from_bytes(bytes(data[k:k + 4]), "big") for k in range(0, len(data) - 3, 4)]

def dump(tag):
    w = words(h.read_mem(PENDING, 128, settle=0.7))
    pending, overrun = w[0:16], w[16:32]
    drop = words(h.read_mem(DROPPED, 4, settle=0.5))
    stamp(f"--- {tag}: total_overrun={sum(overrun)} total_pending={sum(pending)} irq_dropped={drop} ---")
    for i in range(16):
        if pending[i] or overrun[i]:
            stamp(f"    id {i:2d} {NAMES[i]:20s} overrun={overrun[i]:4d} pending={pending[i]:4d}")

try:
    h.wait(7)
    dump("baseline")
    h.type("tts2", delay=0.09); h.press("\n", delay=0.09); h.wait(0.8)
    h.type("run sysinfo.app", delay=0.09); h.press("\n", delay=0.09); h.wait(4.0)
    h.press("F5", delay=0.1); h.wait(3.0)
    sw(); h.wait(1.5)
    h.type("tts3", delay=0.09); h.press("\n", delay=0.09); h.wait(0.8)
    h.type("run command.app", delay=0.09); h.press("\n", delay=0.09); h.wait(3.5)
    stamp("setup done")
    for i in range(1, CYCLES + 1):
        sw(); h.wait(1.5)
        h.type("inputstat", delay=0.11); h.press("\n", delay=0.11); h.wait(0.8)
        sw(); h.wait(1.0)
        h.press("F5", delay=0.1); h.wait(2.8)
        sw(); h.wait(1.0)
        for k in ("Down", "Down", "Up", "Tab"):
            h.press(k, delay=0.12)
        h.mouse("doubleclick", delay=0.2)
        h.mouse("rightdown", delay=0.15); h.mouse("rightup", delay=0.15)
        if i % 8 == 0:
            dump(f"after cycle {i}")
    dump("final")
finally:
    stamp("closing")
    print("hatari rc=", h.close())
