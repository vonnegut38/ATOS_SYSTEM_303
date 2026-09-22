#!/usr/bin/env python3
import hashlib,zlib,sys
from pathlib import Path
for name in sys.argv[1:]:
    data=Path(name).read_bytes()
    print(f"{Path(name).name}  CRC32 {zlib.crc32(data):08x}  SHA256 {hashlib.sha256(data).hexdigest()}")
