#!/usr/bin/env python3
import pathlib
import struct
import sys

p = pathlib.Path(sys.argv[1])
data = bytearray(p.read_bytes())
if len(data) < 32 or data[:8] not in (b"ASYSBOOT",b"ASYSKERN",b"ASYSMOD0",b"ASYSAPP0"):
    raise SystemExit("invalid system image header")
declared = struct.unpack_from(">I", data, 12)[0]
if declared != len(data):
    raise SystemExit(f"header size {declared} differs from file size {len(data)}")
data[28:32] = b"\0\0\0\0"
struct.pack_into(">I", data, 28, sum(data) & 0xFFFFFFFF)
p.write_bytes(data)
