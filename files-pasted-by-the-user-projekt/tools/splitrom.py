#!/usr/bin/env python3
"""Split/join a byte-interleaved 16-bit ROM image."""
import argparse
from pathlib import Path

p=argparse.ArgumentParser()
sp=p.add_subparsers(dest="command",required=True)
s=sp.add_parser("split"); s.add_argument("full"); s.add_argument("hi"); s.add_argument("lo")
j=sp.add_parser("join"); j.add_argument("hi"); j.add_argument("lo"); j.add_argument("full")
a=p.parse_args()
if a.command=="split":
    data=Path(a.full).read_bytes()
    if len(data)%2: raise SystemExit("ROM size must be even")
    Path(a.hi).write_bytes(data[0::2]); Path(a.lo).write_bytes(data[1::2])
else:
    hi,lo=Path(a.hi).read_bytes(),Path(a.lo).read_bytes()
    if len(hi)!=len(lo): raise SystemExit("HI/LO sizes differ")
    data=bytearray(len(hi)*2); data[0::2]=hi; data[1::2]=lo
    Path(a.full).write_bytes(data)
