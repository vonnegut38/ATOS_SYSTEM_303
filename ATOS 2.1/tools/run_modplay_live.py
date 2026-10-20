#!/usr/bin/env python3
"""Launch the emulator with MODPLAY on the tree image and keep it open.

Kept in the project tree on purpose: scripts in /tmp disappear between sessions
and a missing script looks exactly like a failed launch.

Usage: python3 tools/run_modplay_live.py [--headless] [--seconds N]
"""
import sys, time, shutil, subprocess
from pathlib import Path

REAL = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REAL / "atos2-tools"))
from atos2_auto import Hatari

HEADLESS = "--headless" in sys.argv
SECONDS = 120
if "--seconds" in sys.argv:
    SECONDS = int(sys.argv[sys.argv.index("--seconds") + 1])

IMG = str(REAL / "work/atos-tree-full.img")
BIOS = str(REAL / "work/audio-baseline/BIOS-tree.bin")

symbols = {}
nm = subprocess.run(["m68k-elf-nm", str(REAL / "build/system/kernel.elf")],
                    capture_output=True, text=True).stdout
for line in nm.splitlines():
    p = line.split()
    if len(p) == 3:
        symbols[p[2]] = int(p[0], 16)

extra = ["--machine", "megast", "--cpulevel", "3", "--cpuclock", "32", "--memsize", "8",
         "--ttram", "64", "--tf536", "on", "--graphics-card", "vofa", "--gfx-chip", "et4000",
         "--mono", "--fast-forward", "off", "--sound", "44100"]
if not HEADLESS:
    extra += ["--window", "--statusbar", "false", "--borders", "false", "--resizable", "false",
              "--zoom", "1"]
for i in range(8):
    extra += ["--acsi", "%d=none" % i]
extra += ["--ide-master", IMG, "--mmu", "off", "--tos", BIOS, "--patch-tos", "false"]

work = Path("/tmp/atos-mplive")
shutil.rmtree(work, ignore_errors=True)
work.mkdir(parents=True)
(work / "work").mkdir(exist_ok=True)
(work / "build").mkdir(exist_ok=True)
shutil.copy(BIOS, str(work / "build/bios_mste.bin"))
shutil.copy(str(REAL / "atos2.cfg"), str(work / "atos2.cfg"))

h = Hatari(str(work), log=str(work / "work/h.log"), window=not HEADLESS, image=IMG,
           binary="/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536",
           cfg=True, extra=extra, render=True)


def word(addr):
    for _ in range(3):
        raw = h.read_bytes(addr, 4, settle=0.25)
        if raw and len(raw) == 4:
            return int.from_bytes(raw, "big")
        time.sleep(0.2)
    return None


def sym(name):
    return word(symbols.get(name))


print("boot: czekam na pulpit", flush=True)
time.sleep(45)
if not HEADLESS:
    h.screenshot("/tmp/modplay_live_0.png", settle=0.5)

end = time.time() + SECONDS
while time.time() < end:
    print("owner=%-3s kolejka=%-3s zagrane=%-6s isr_max=%s" % (
        sym("kaudio_state_owner"), sym("kaudio_state_queued"), sym("kaudio_state_events"),
        sym("kaudio_dbg_isr_max")), flush=True)
    time.sleep(10)

if not HEADLESS:
    h.screenshot("/tmp/modplay_live_1.png", settle=0.5)
if HEADLESS:
    print("rc", h.close(), flush=True)
else:
    print("okno zostaje otwarte - kliknijcie RUN i wpiszcie MODPLAY", flush=True)
    while True:
        time.sleep(30)
