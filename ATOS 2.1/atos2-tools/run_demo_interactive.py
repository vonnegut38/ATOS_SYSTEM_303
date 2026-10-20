#!/usr/bin/env python3
"""Launch ATOS 2.1 in a VISIBLE Hatari window and auto-run the TOS demo.

Unlike the automated scenarios (which run headless and only save screenshots),
this opens a real SDL window so a human can watch and interact: the script only
types the RUN command, then leaves the shell in the window for further typing.

Usage:
    python3 atos2-tools/run_demo_interactive.py [program]

Default program: \\SYSTEM\\TEST\\TOSDEMO.PRG
"""
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from atos2_auto import Hatari  # noqa: E402

PROJECT = Path(__file__).resolve().parent.parent
BINARY = "/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536"
IMAGE = PROJECT / "work" / "atos-tos1.img"

EXTRA = [
    "--machine", "megast",
    "--cpulevel", "3", "--cpuclock", "32",
    "--memsize", "4", "--ttram", "64",
    "--tf536", "on",
    "--graphics-card", "vofa", "--gfx-chip", "et4000", "--mono",
    "--fast-forward", "off",
    "--ide-master", str(IMAGE),
]
for _i in range(8):
    EXTRA += ["--acsi", "%d=none" % _i]


def main():
    # "-" means: just boot and leave the desktop on screen, type nothing.
    program = sys.argv[1] if len(sys.argv) > 1 else "\\SYSTEM\\TEST\\TOSDEMO.PRG"
    h = Hatari(str(PROJECT), log=str(PROJECT / "work" / "hatari-interactive.log"),
               window=True, image=str(IMAGE), binary=BINARY, cfg=True, extra=EXTRA)

    print("Hatari window launched, waiting for the ATOS shell ...", flush=True)
    ready = False
    for _ in range(60):
        time.sleep(2)
        try:
            text = "\n".join(h.console_lines(rows=25) or [])
        except Exception:
            text = ""
        if "ATOS 2.0" in text:
            ready = True
            break
    print("shell ready:", ready, flush=True)
    time.sleep(20)

    if program == "-":
        print("boot-only mode: desktop left on screen", flush=True)
    else:
        print("typing: run %s" % program, flush=True)
        h.type("run " + program)
        time.sleep(0.4)
        h.press("Return")

    # Leave the window open for the user: the demo runs ~15 s (3 stages) and then
    # returns to the shell, where further commands can be typed by hand.
    print("demo started - window stays open; Ctrl-C here to close it", flush=True)
    try:
        while True:
            time.sleep(5)
    except KeyboardInterrupt:
        pass
    finally:
        print("closing Hatari, rc", h.close())


if __name__ == "__main__":
    main()
