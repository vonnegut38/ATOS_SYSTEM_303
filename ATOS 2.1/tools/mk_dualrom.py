#!/usr/bin/env python3
"""Build a banked EPROM image: each input ROM becomes one bank (bank1 first).

The TF536 ST Edition EPROM socket carries two complete ROM images and a jumper
selects bank1/bank2 (a high address line, usually A18 on a 512 KiB part).
Every input is padded with 0xff to --bank-size; a too-large input is an error.
"""
import argparse
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument("--bank-size", type=lambda s: int(s, 0), default=262144)
p.add_argument("output")
p.add_argument("images", nargs="+")
a = p.parse_args()
out = bytearray()
for name in a.images:
    data = Path(name).read_bytes()
    if len(data) > a.bank_size:
        raise SystemExit("%s: %d bytes > bank size %d" % (name, len(data), a.bank_size))
    out += data + b"\xff" * (a.bank_size - len(data))
Path(a.output).write_bytes(out)
print("%s: %d bytes = %d bank(s) of %d" % (a.output, len(out), len(a.images), a.bank_size))