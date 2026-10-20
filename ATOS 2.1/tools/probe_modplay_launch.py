#!/usr/bin/env python3
"""Launch MODPLAY through the desktop RUN dialog and read its stage markers.

The app is built with -DMODPLAY_TRACE, so it writes progress to fixed addresses
(0x800 entered, 0x804 gfx pid, 0x808 gui pid, 0x80C load result, 0x810 painted,
0x814 loop count).  Those addresses need no symbols, exactly like the audio
probe's scratch area.

Usage: python3 tools/probe_modplay_launch.py [--typed MODPLAY] [--seconds 90]
"""
import sys, time, shutil
from pathlib import Path

REAL = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REAL / "atos2-tools"))
from atos2_auto import Hatari

TYPED = "MODPLAY"
if "--typed" in sys.argv:
    TYPED = sys.argv[sys.argv.index("--typed") + 1]
SECONDS = 90
if "--seconds" in sys.argv:
    SECONDS = int(sys.argv[sys.argv.index("--seconds") + 1])

IMG = str(REAL / "work/atos-tree-full.img")
BIOS = str(REAL / "work/audio-baseline/BIOS-tree.bin")

extra = ["--machine", "megast", "--cpulevel", "3", "--cpuclock", "32", "--memsize", "8",
         "--ttram", "64", "--tf536", "on", "--graphics-card", "vofa", "--gfx-chip", "et4000",
         "--mono", "--fast-forward", "off", "--sound", "44100"]
for i in range(8):
    extra += ["--acsi", "%d=none" % i]
extra += ["--ide-master", IMG, "--mmu", "off", "--tos", BIOS, "--patch-tos", "false"]

work = Path("/tmp/atos-mpprobe")
shutil.rmtree(work, ignore_errors=True)
work.mkdir(parents=True)
(work / "work").mkdir(exist_ok=True)
(work / "build").mkdir(exist_ok=True)
shutil.copy(BIOS, str(work / "build/bios_mste.bin"))
shutil.copy(str(REAL / "atos2.cfg"), str(work / "atos2.cfg"))

h = Hatari(str(work), log=str(work / "work/h.log"), window=False, image=IMG,
           binary="/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536",
           cfg=True, extra=extra, render=True)


def word(addr):
    for _ in range(3):
        raw = h.read_bytes(addr, 4, settle=0.2)
        if raw and len(raw) == 4:
            return int.from_bytes(raw, "big")
        time.sleep(0.15)
    return None


def key(k, hold=0.16, gap=0.28):
    h.keydown(k, delay=hold)
    time.sleep(hold)
    h.keyup(k, delay=0.05)
    time.sleep(gap)


def typed(text):
    for ch in text:
        h.keydown(ch, delay=0.18)
        time.sleep(0.18)
        h.keyup(ch, delay=0.06)
        time.sleep(0.34)


try:
    print("boot", flush=True)
    time.sleep(45)
    h.press("Q", delay=0.1)
    time.sleep(1)
    key("F4")
    key("Down")
    key("Return", gap=0.7)
    typed(TYPED)
    time.sleep(0.6)
    h.screenshot("/tmp/mpp_dialog.png", settle=0.5)
    key("Return", gap=0.0)
    time.sleep(5)
    h.screenshot("/tmp/mpp_after.png", settle=0.5)
    for i in range(max(1, SECONDS // 10)):
        print("ticks=%-8s alloc=0x%-8X stat=%-5s size=%-8s open=%-6s loops=%s" % (
            word(0x840) or 0, word(0x844) or 0, word(0x848) or 0, word(0x84C) or 0,
            word(0x850) or 0, word(0x814) or 0), flush=True)
        time.sleep(10)
finally:
    print("rc", h.close(), flush=True)
