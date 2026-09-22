#!/usr/bin/env python3
"""A2: turn a linked m68k ELF into a relocated TOS .PRG.

The ASF T1 loader implements the 0x601A relocation table (it adds the load
address to every listed longword).  This tool links a freestanding C program
with a base of 0x100 (the basepage's text offset) and emits:

    0x601A | tlen | dlen | blen | slen=0 | 10 reserved bytes
    text | data | relocation table (offsets from the text start, 0-terminated)

Relocation entries are the R_68K_32 (absolute longword) sites reported by
readelf; PC-relative relocations are already position independent.
"""
import struct
import subprocess
import sys

BASE = 0x100  # matches the loader: text lives at basepage + 0x100


def run(cmd):
    return subprocess.run(cmd, capture_output=True, text=True, check=True).stdout


def section_bytes(elf, section):
    out = "/tmp/mk_tosprg_%s.bin" % section.strip(".")
    subprocess.run(["m68k-elf-objcopy", "-O", "binary", "-j", section, elf, out], check=True)
    with open(out, "rb") as handle:
        return handle.read()


def section_size(elf, section):
    for line in run(["m68k-elf-size", "-A", elf]).splitlines():
        parts = line.split()
        if len(parts) >= 2 and parts[0] == section:
            return int(parts[1], 0)
    return 0


def text_base(elf):
    """Address the linker placed the text at (the loader's load base + 0x100)."""
    for line in run(["m68k-elf-nm", elf]).splitlines():
        parts = line.split()
        if len(parts) == 3 and parts[2] == "_tos_start":
            return int(parts[0], 16)
    return BASE


def relocations(elf, base):
    """Absolute-longword sites, as offsets from the start of the text."""
    offsets = []
    for line in run(["m68k-elf-readelf", "-r", elf]).splitlines():
        parts = line.split()
        if len(parts) >= 3 and parts[2].startswith("R_68K_32"):
            offsets.append(int(parts[0], 16) - base)
    bad = [o for o in offsets if o < 0]
    if bad:
        raise SystemExit("relocation outside the text (base=0x%x): %s" % (base, bad[:8]))
    return sorted(set(offsets))


def main():
    if len(sys.argv) != 3:
        print("usage: mk_tosprg.py <input.elf> <output.prg>", file=sys.stderr)
        return 1
    elf, out = sys.argv[1], sys.argv[2]
    # Build the whole image in memory first: a failure must never leave a
    # truncated .PRG behind (that is exactly how MC.PRG got corrupted).
    text = section_bytes(elf, ".text")
    data = section_bytes(elf, ".data")
    bss = section_size(elf, ".bss")
    relocs = relocations(elf, text_base(elf))
    image = bytearray()
    with open(out, "wb") as handle:
        image += struct.pack(">HIIII", 0x601A, len(text), len(data), bss, 0)
        image += b"\0" * 10
        image += text
        image += data
        for offset in relocs:
            image += struct.pack(">I", offset)
        image += b"\0\0\0\0"
        handle.write(image)
    print("%s: text=%d data=%d bss=%d relocs=%d" % (out, len(text), len(data), bss, len(relocs)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
