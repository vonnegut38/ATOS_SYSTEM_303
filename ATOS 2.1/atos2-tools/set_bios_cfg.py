#!/usr/bin/env python3
r"""Read or patch the on-disk BIOS.CFG inside an ATOS AHDI FAT16 image.

The ROM keeps a failover latch (video_pending, byte 27) in \BIOS\BIOS.CFG.  A
card boot that never reaches the shell leaves it set, and the next start then
forces the Shifter.  Tests need to write that byte deterministically instead of
relying on the ROM's own write path.

Usage:
    set_bios_cfg.py IMAGE                 # report every BIOS.CFG found
    set_bios_cfg.py IMAGE --pending 0     # patch the latch in place (CRC fixed)
    set_bios_cfg.py IMAGE --pending 0 --root-only
"""
import argparse
import struct
import sys


def le16(b, o):
    return struct.unpack_from("<H", b, o)[0]


def be32(b, o):
    return struct.unpack_from(">I", b, o)[0]


def crc(b):
    s = 0
    for i in range(len(b)):
        if 8 <= i < 12:
            continue
        s = ((s << 1) | (s >> 31)) & 0xFFFFFFFF
        s ^= b[i]
    return s


class Image:
    def __init__(self, path, writable=False):
        self.f = open(path, "r+b" if writable else "rb")
        root = self.rd(0, 512)
        parts = []
        for n in range(4):
            o = 0x1C6 + n * 12
            if root[o] & 1:
                parts.append((be32(root, o + 4), root[o + 1:o + 4]))
        if not parts:
            raise SystemExit("no AHDI partition found")
        self.part, self.pid = parts[0]
        base = self.part * 512
        bpb = self.rd(base, 512)
        self.bps = le16(bpb, 11)
        self.spc = bpb[13]
        self.fats = bpb[16]
        self.spf = le16(bpb, 22)
        self.roots = le16(bpb, 17)
        self.fat0 = base + le16(bpb, 14) * self.bps
        self.root_off = self.fat0 + self.fats * self.spf * self.bps
        root_bytes = ((self.roots * 32 + self.bps - 1) // self.bps) * self.bps
        self.data_off = self.root_off + root_bytes
        self.cluster_bytes = self.spc * self.bps

    def rd(self, off, size):
        self.f.seek(off)
        return self.f.read(size)

    def wr(self, off, data):
        self.f.seek(off)
        self.f.write(data)

    def cluster_off(self, c):
        return self.data_off + (c - 2) * self.cluster_bytes

    def fat_get(self, c):
        return le16(self.rd(self.fat0 + c * 2, 2), 0)

    def read_chain(self, first, size):
        out = b""
        c = first
        seen = set()
        while 2 <= c < 0xFFF8 and len(out) < size:
            if c in seen:
                break
            seen.add(c)
            out += self.rd(self.cluster_off(c), self.cluster_bytes)
            c = self.fat_get(c)
        return out[:size]

    def entries(self, off, count):
        for i in range(count):
            yield off + i * 32, self.rd(off + i * 32, 32)

    def find(self):
        """Return [(label, dir_entry_offset, first_cluster, size)]."""
        out = []
        bios_cluster = 0
        for o, e in self.entries(self.root_off, self.roots):
            if e[0] == 0:
                break
            if e[0] == 0xE5:
                continue
            name = e[:11].decode("ascii", "replace")
            if name == "BIOS       " and e[11] & 0x10:
                bios_cluster = le16(e, 26)
            if name == "BIOS    CFG":
                out.append(("\\BIOS.CFG", o, le16(e, 26),
                            struct.unpack_from("<I", e, 28)[0]))
        if bios_cluster >= 2:
            for o, e in self.entries(self.cluster_off(bios_cluster),
                                     self.cluster_bytes // 32):
                if e[0] == 0:
                    break
                if e[0] == 0xE5:
                    continue
                if e[:11] == b"BIOS    CFG":
                    out.append(("\\BIOS\\BIOS.CFG", o, le16(e, 26),
                                struct.unpack_from("<I", e, 28)[0]))
        return out

    def dump(self):
        for label, ent, cl, size in self.find():
            b = self.read_chain(cl, size)
            print(f"{label:18s} len={len(b):3d} magic={be32(b,0):#x} "
                  f"ver={struct.unpack_from('>H',b,4)[0]} "
                  f"boot_device={b[12]} video_driver={b[13]} video_mode={b[14]} "
                  f"video_pending={b[27]} boot_valid={b[25]} auto_tos={b[26]} "
                  f"crc_ok={crc(b)==be32(b,8)}")

    def set_pending(self, value, root_only=False):
        changed = []
        for label, ent, cl, size in self.find():
            if root_only and label != "\\BIOS.CFG":
                continue
            b = bytearray(self.read_chain(cl, size))
            if len(b) < 28:
                continue
            if b[27] == value:
                continue
            b[27] = value
            struct.pack_into(">I", b, 8, crc(bytes(b)))
            # The file is one sector-sized cluster in these images; write it
            # back through the FAT chain so a multi-cluster file also works.
            c = cl
            at = 0
            while 2 <= c < 0xFFF8 and at < len(b):
                n = min(self.cluster_bytes, len(b) - at)
                self.wr(self.cluster_off(c), bytes(b[at:at + n]))
                at += n
                c = self.fat_get(c)
            changed.append(label)
        return changed


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("image")
    ap.add_argument("--pending", type=int, choices=[0, 1], default=None)
    ap.add_argument("--root-only", action="store_true")
    a = ap.parse_args()
    img = Image(a.image, writable=a.pending is not None)
    try:
        if a.pending is None:
            img.dump()
        else:
            changed = img.set_pending(a.pending, a.root_only)
            print("patched:", changed if changed else "(already set)")
            img.dump()
    finally:
        img.f.close()


if __name__ == "__main__":
    main()
