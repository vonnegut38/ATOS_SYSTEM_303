#!/usr/bin/env python3
"""Short check: what IPL does the main loop run at before/after user preemption?"""
import re, sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari

P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
IMG = P + "/work/atos-v2-gap.img"
h = Hatari(P, log=P + "/work/hatari-ipl.log", image=IMG)

def words(text):
    data = []
    for line in text.splitlines():
        if not re.match(r"^[0-9A-Fa-f]{8}:", line):
            continue
        for tok in line.split(":", 1)[1][:48].split():
            if len(tok) == 2 and all(c in "0123456789abcdefABCDEF" for c in tok):
                data.append(int(tok, 16))
    return [int.from_bytes(bytes(data[k:k+4]), "big") for k in range(0, len(data) - 3, 4)]

def dump(tag):
    ipl = words(h.read_mem("0x9a69a", 32, settle=0.6))
    pend = words(h.read_mem("0x9a6ba", 128, settle=0.6))
    over = words(h.read_mem("0x9a6fa", 128, settle=0.6))
    print(f"--- {tag} ---")
    print("  ipl histogram (index=IPL):", ipl)
    print("  total pending:", sum(pend[0:16]), " total overrun:", sum(over[0:16]))
    nz = [(i, over[i], pend[i]) for i in range(16) if over[i] or pend[i]]
    print("  per-section:", nz)

try:
    h.wait(7)
    dump("shell only (before any app/quantum)")
    h.type("tts2", delay=0.09); h.press("\n", delay=0.09); h.wait(0.8)
    h.type("run sysinfo.app", delay=0.09); h.press("\n", delay=0.09); h.wait(5.0)
    h.press("F5", delay=0.1); h.wait(4.0)
    dump("after SYSINFO + F5 (preemption active)")
finally:
    print("hatari rc=", h.close())
