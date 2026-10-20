#!/usr/bin/env python3
"""A2: turn a linked m68k ELF into a relocated TOS .PRG.

The ASF T1 loader implements the 0x601A relocation table: it adds the TEXT
base (basepage+0x100) to every listed longword.  The text is therefore linked
from address 0, so each absolute longword is a plain offset from the start of
text.  The emitted image is:

    0x601A | tlen | dlen | blen | slen=0 | 10 reserved bytes
    text | data | relocation table (32-bit first offset + byte deltas, 0-end)

Relocation entries are the R_68K_32 (absolute longword) sites reported by
readelf; PC-relative relocations are already position independent.
"""
import struct
import subprocess
import sys

from toolchain import tool

BASE = 0  # text is linked from address 0 (standard GEMDOS PRG)

def run(cmd):
    return subprocess.run(cmd, capture_output=True, text=True, check=True).stdout


def section_bytes(elf, section):
    out = "/tmp/mk_tosprg_%s.bin" % section.strip(".")
    subprocess.run([tool("objcopy"), "-O", "binary", "-j", section, elf, out], check=True)
    with open(out, "rb") as handle:
        return handle.read()


def section_size(elf, section):
    for line in run([tool("size"), "-A", elf]).splitlines():
        parts = line.split()
        if len(parts) >= 2 and parts[0] == section:
            return int(parts[1], 0)
    return 0


def text_base(elf):
    """Address the linker placed the text at (the loader's load base + 0x100)."""
    for line in run([tool("nm"), elf]).splitlines():
        parts = line.split()
        if len(parts) == 3 and parts[2] == "_tos_start":
            return int(parts[0], 16)
    return BASE


def section_layout(elf):
    """Section sizes AND addresses.  The previous version glued the raw .text and
    .data bytes together, silently dropping the ELF alignment gap between them:
    the image was 2 bytes shorter than the addresses the relocations and symbols
    were computed for, so every relocated pointer was read from the wrong
    half-word.  The classic symptom was a Bus Error inside the very first VDI
    call of a correctly linked program (a fault also reports -2, which looked
    like "file not found")."""
    layout = {}
    for line in run([tool("objdump"), "-h", elf]).splitlines():
        parts = line.split()
        if len(parts) >= 4 and parts[1].startswith(".") and parts[0].isdigit():
            try:
                size = int(parts[2], 16)
                vma = int(parts[3], 16)
            except ValueError:
                continue
            layout[parts[1]] = (vma, size)
    return layout


def relocations(elf, base):
    """Absolute-longword sites, as offsets from the start of the text."""
    offsets = []
    for line in run([tool("readelf"), "-r", elf]).splitlines():
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
    layout = section_layout(elf)
    text = section_bytes(elf, ".text")
    try:
        data = section_bytes(elf, ".data")
    except subprocess.CalledProcessError:
        data = b""
    t_vma = layout.get(".text", (0, len(text)))[0]
    d_vma, d_size = layout.get(".data", (t_vma + len(text), len(data)))
    b_vma, b_size = layout.get(".bss", (d_vma + d_size, 0))
    # PRG segment boundaries come from the ELF ADDRESSES, not from the raw byte
    # lengths: .text ends where .data begins (any .rodata and the linker's
    # alignment padding belong to the text segment), and the data segment runs
    # up to the start of .bss.  Gluing the two section dumps together instead
    # dropped that padding, so every relocated pointer was read two bytes off -
    # a Bus Error inside the first VDI call of an otherwise correct program.
    tlen = d_vma - t_vma if d_size else len(text)
    dlen = (b_vma - d_vma) if b_size else len(data)
    if tlen < len(text):
        raise SystemExit("section layout inconsistent: tlen=%d < text=%d" % (tlen, len(text)))
    if dlen < len(data):
        raise SystemExit("section layout inconsistent: dlen=%d < data=%d" % (dlen, len(data)))
    image_bytes = bytearray(tlen + dlen)
    image_bytes[0:len(text)] = text
    if data:
        image_bytes[tlen:tlen + len(data)] = data
    bss = b_size
    relocs = relocations(elf, text_base(elf))
    image = bytearray()
    with open(out, "wb") as handle:
        image += struct.pack(">HIIII", 0x601A, tlen, dlen, bss, 0)
        image += b"\0" * 10
        image += image_bytes
        # GEMDOS byte-delta relocation table, exactly what the loader reads:
        #   32-bit offset of the FIRST relocation, then one byte per jump
        #   (0x00 ends the table, 0x01 means +0xFE, 2..255 mean +2..+255).
        #   Relocation sites are word-aligned, so every real delta is even.
        if relocs:
            image += struct.pack(">I", relocs[0])
            prev = relocs[0]
            for offset in relocs[1:]:
                delta = offset - prev
                if delta <= 0 or delta & 1:
                    raise SystemExit("bad relocation delta %d" % delta)
                # Byte 1 = "+254, no fixup" (chains gaps longer than 255);
                # any other byte b (2..255) = "+b, then fixup".  Every delta
                # must end with the fixup, so stop at >254, not >=254.
                while delta > 254:
                    image += b"\x01"
                    delta -= 254
                image += bytes([delta])
                prev = offset
            image += b"\x00"
        else:
            # No relocations: a 32-bit zero first offset marks "nothing to fix".
            image += b"\x00\x00\x00\x00"
        handle.write(image)
    print("%s: text=%d data=%d bss=%d relocs=%d (image=%d)" %
          (out, tlen, dlen, bss, len(relocs), len(image)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
