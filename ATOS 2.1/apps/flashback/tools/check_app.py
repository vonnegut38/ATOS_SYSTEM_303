#!/usr/bin/env python3
"""Validate a built FLASHBK.APP against the ATOS loader rules (kernel_module.c
verify_app_image, ABI 1.2 limits) before it reaches a disk image."""
import struct
import sys

MAX_FILE, MAX_MEMORY, MAX_STACK = 1024 * 1024, 2048 * 1024, 64 * 1024


def main(path):
    d = open(path, "rb").read()
    if d[:8] != b"ASYSAPP0":
        raise SystemExit("check_app: bad magic")
    ver, hsize = struct.unpack_from(">HH", d, 8)
    file_end, memory, entry, stack, checksum = struct.unpack_from(">IIIII", d, 12)
    errors = []
    if ver != 1 or hsize != 32:
        errors.append("header version/size")
    if file_end != len(d):
        errors.append(f"header file size {file_end} != {len(d)}")
    if len(d) > MAX_FILE:
        errors.append(f"file {len(d)} > {MAX_FILE} (ATOS ABI 1.2 limit)")
    if memory < file_end or memory > MAX_MEMORY:
        errors.append(f"memory image {memory} out of range")
    if not 1024 <= stack <= MAX_STACK or stack & 1:
        errors.append(f"stack {stack} out of range")
    magic, exec_end, writable = struct.unpack_from(">III", d, len(d) - 12)
    if magic != 0x41505732:
        errors.append("APW2 policy missing at end of file")
    if not (32 < exec_end <= writable <= len(d) - 12) or not (32 <= entry < exec_end):
        errors.append("policy/entry layout")
    total = sum(b for i, b in enumerate(d) if i < 28 or i >= 32) & 0xFFFFFFFF
    if total != checksum:
        errors.append("checksum (run patch_boot_checksum.py)")
    if errors:
        raise SystemExit("check_app: " + "; ".join(errors))
    print(f"check_app: {path}: file {len(d)} B, memory {memory} B, stack {stack} B, "
          f"code {exec_end} B - OK for ATOS ABI 1.2")


if __name__ == "__main__":
    main(sys.argv[1])
