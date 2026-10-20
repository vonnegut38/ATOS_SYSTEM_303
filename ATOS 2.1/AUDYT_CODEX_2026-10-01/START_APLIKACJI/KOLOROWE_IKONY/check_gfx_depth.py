#!/usr/bin/env python3
"""Check desktop/broker depth-query agreement and the built GFX.APP (read-only).
Usage: python3 check_gfx_depth.py /path/to/ATOS
"""
from pathlib import Path
import re
import struct
import sys
root = Path(sys.argv[1])
desktop = (root / 'system/desktop_wm.c').read_text()
broker = (root / 'system/gfx_service_app.S').read_text()
op = int(re.search(r'#define GFX_VIDEO_INFO\s+(\d+)u', desktop)[1])
limit = int(re.search(r'cmpi\.l #(\d+),%d2[^\n]*\n\s*bhi command_invalid', broker)[1])
assert op <= limit, f'Desktop opcode {op} is rejected by broker limit {limit}'
image = (root / 'build/system/GFX.APP').read_bytes()
assert image[:8] == b'ASYSAPP0'
assert len(image) == struct.unpack_from('>I', image, 12)[0]
assert sum(image[:28] + image[32:]) & 0xffffffff == struct.unpack_from('>I', image, 28)[0]
assert b'\x0c\x82' + struct.pack('>I', limit) in image, 'Rebuild stale GFX.APP'
print(f'PASS: opcode {op}, broker limit {limit}, built image size/checksum/instruction')
