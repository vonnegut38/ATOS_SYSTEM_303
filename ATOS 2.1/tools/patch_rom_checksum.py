#!/usr/bin/env python3
"""Patch a padded ATOS ROM with a 32-bit checksum at header offset 0x14.

The value is the sum of big-endian 32-bit words over the whole ROM with bytes
0x14..0x17 treated as zero.  diag_rom_checksum() (diag/tests.c) computes the
same sum over [__rom_start, __rom_end) and compares, so an unpatched ROM fails
its POST self-test.  Run after tools/padrom.py (the padded size must be a
multiple of 4)."""
import struct
import sys
from pathlib import Path

p = Path(sys.argv[1])
data = bytearray(p.read_bytes())
if len(data) < 0x18 or len(data) % 4:
    raise SystemExit("ROM must be >= 24 bytes and a multiple of 4")
data[0x14:0x18] = b"\0\0\0\0"
total = 0
for i in range(0, len(data), 4):
    total = (total + struct.unpack_from(">I", data, i)[0]) & 0xFFFFFFFF
struct.pack_into(">I", data, 0x14, total)
p.write_bytes(data)
print("%s: ROM checksum %08x (%d bytes)" % (p.name, total, len(data)))
