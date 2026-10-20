#!/usr/bin/env python3
"""Create a checksum-valid APP with deliberately invalid APW2 metadata."""
import pathlib
import struct
import sys

if len(sys.argv) != 3:
    raise SystemExit("usage: make_bad_policy_app.py GOOD.APP BADPOL.APP")

source = pathlib.Path(sys.argv[1])
target = pathlib.Path(sys.argv[2])
data = bytearray(source.read_bytes())
if len(data) < 44 or data[-12:-8] != b"APW2":
    raise SystemExit("source has no APW2 policy")

# Keep the APP header and checksum valid while making executable_end overlap
# the writable section. The loader, rather than the checksum, must reject it.
writable_start = struct.unpack_from(">I", data, len(data) - 4)[0]
struct.pack_into(">I", data, len(data) - 8, writable_start + 2)
struct.pack_into(">I", data, 28, 0)
checksum = sum(data[:28]) + sum(data[32:])
struct.pack_into(">I", data, 28, checksum & 0xFFFFFFFF)
target.write_bytes(data)
