#!/usr/bin/env python3
"""Copy Flashback data files into an ATOS disk image (\\GAMES\\FLASHBK\\DATA).

usage: install_data.py IMAGE [--data DATA_DIR] [--cfg FLASHBK.CFG] [--levels N] [--cutscenes]

Only 8.3 names are copied.  By default (--levels 2) the files of levels 3..5
are left out - the test build plays levels 1 and 2 - and so are the polygon
cutscene files (*.CMD, *.POL) unless --cutscenes is given.  The image must not
be in use by a running Hatari."""
import argparse
import pathlib
import re
import subprocess
import sys

here = pathlib.Path(__file__).resolve().parent
installer = here.parent.parent.parent / "tools" / "install_boot_sys.py"
ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
ap.add_argument("image")
ap.add_argument("--data")
ap.add_argument("--cfg")
ap.add_argument("--levels", type=int, default=2)
ap.add_argument("--cutscenes", action="store_true")
a = ap.parse_args()
target = "GAMES/FLASHBK/DATA"


def install(path, extra=()):
    subprocess.run([sys.executable, str(installer), a.image, str(path), "--directory", target, "--data", *extra],
                   check=True)


if a.cfg and pathlib.Path(a.cfg).is_file():
    install(pathlib.Path(a.cfg), ["--if-missing"])
if not a.data:
    print("install_data: no DATA directory given - only FLASHBK.APP/CFG installed")
    raise SystemExit(0)
count = 0
for f in sorted(pathlib.Path(a.data).iterdir()):
    if not f.is_file():
        continue
    name = f.name.upper()
    if not re.fullmatch(r"[A-Z0-9_\-]{1,8}(\.[A-Z0-9_\-]{1,3})?", name):
        print(f"install_data: skipping non-8.3 name {f.name}")
        continue
    m = re.match(r"LEVEL(\d)", name)
    if m and int(m.group(1)) > a.levels:
        continue
    if not a.cutscenes and name.endswith((".CMD", ".POL")):
        continue
    if f.name != name:
        tmp = pathlib.Path("/tmp") / name
        tmp.write_bytes(f.read_bytes())
        f = tmp
    install(f)
    count += 1
print(f"install_data: {count} files -> \\{target.replace('/', chr(92))}")
