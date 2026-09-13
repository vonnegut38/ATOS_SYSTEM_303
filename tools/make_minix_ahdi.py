#!/usr/bin/env python3
"""make_minix_ahdi.py <out.img> <mb> <srcdir>

Tworzy obraz AHDI z jedna partycja Minix v1 (30-znakowe nazwy, magic 0x138F)
i wypelnia ja zawartoscia <srcdir> (staje sie korzeniem 'u:' dla MiNT-a).
Wszystkie struktury w kolejnosci BIG-ENDIAN (Atari).
"""
import os, struct, sys

BS = 512
BLK = 1024
MAGIC = 0x138F          # Minix v1, 30-char names
INO_SIZE = 32
DIRENT = 32
DIRECT = 7
PART_START = 2          # sektor startu partycji (AHDI)
FIRST_INO = 1

def u16(v): return struct.pack(">H", v & 0xFFFF)
def u32(v): return struct.pack(">I", v & 0xFFFFFFFF)

def collect(src):
    """Zwraca (dirs, files): dirs=[relpath], files=[(relpath, bytes)]."""
    dirs, files = [], []
    for root, dnames, fnames in os.walk(src):
        dnames.sort(); fnames.sort()
        rel = os.path.relpath(root, src)
        rel = "" if rel == "." else rel.replace(os.sep, "/")
        if rel:
            dirs.append(rel)
        for fn in fnames:
            p = os.path.join(root, fn)
            r = (rel + "/" + fn) if rel else fn
            try:
                with open(p, "rb") as f:
                    files.append((r, f.read()))
            except OSError:
                pass
    return dirs, files

def main():
    out, mb, src = sys.argv[1], int(sys.argv[2]), sys.argv[3]
    total_bytes = mb * 1024 * 1024
    total_sectors = total_bytes // BS
    part_sectors = total_sectors - PART_START
    total_zones = part_sectors // (BLK // BS)

    dirs, files = collect(src)
    # przydzial inodow: 1 = root, potem katalogi, potem pliki
    entries = [("", "dir")] + [(d, "dir") for d in dirs] + [(f, "file") for f, _ in files]
    n_inodes = len(entries) + 1
    imap_blocks = max(1, (n_inodes + 8 * BLK - 1) // (8 * BLK))
    zmap_blocks = max(1, (total_zones + 8 * BLK - 1) // (8 * BLK))
    inode_blocks = (n_inodes * INO_SIZE + BLK - 1) // BLK
    first_data_zone = 2 + imap_blocks + zmap_blocks + inode_blocks

    buf = bytearray(total_bytes)
    # --- AHDI root sector ---
    for n in range(4):
        o = 0x1C6 + n * 12
        if n == 0:
            buf[o] = 0x01
            buf[o+1:o+4] = b"MIX"
            buf[o+4:o+8] = u32(PART_START)
            buf[o+8:o+12] = u32(part_sectors)
    s = 0
    for i in range(0, 510, 2):
        s += (buf[i] << 8) | buf[i+1]
    ck = (0x1234 - (s & 0xFFFF)) & 0xFFFF
    buf[0x1FE:0x200] = u16(ck)

    base = PART_START * BS
    # --- superblock ---
    sb = bytearray(1024)
    sb[0:2] = u16(n_inodes)
    sb[2:4] = u16(total_zones - first_data_zone + 1)
    sb[4:6] = u16(imap_blocks)
    sb[6:8] = u16(zmap_blocks)
    sb[8:10] = u16(first_data_zone)
    sb[10:12] = u16(0)
    sb[12:16] = u32(0x10081C00)
    sb[16:18] = u16(MAGIC)
    buf[base + BLK:base + 2 * BLK] = sb

    imap = bytearray(imap_blocks * BLK)
    for i in range(n_inodes + 1):
        imap[i // 8] |= 0x80 >> (i % 8)
    zmap = bytearray(zmap_blocks * BLK)
    for z in range(first_data_zone):
        zmap[z // 8] |= 0x80 >> (z % 8)
    inodes = bytearray(inode_blocks * BLK)

    nz = total_zones - first_data_zone + 1
    zmap[0] |= 0x80
    zmap[1] |= 0x40
    next_zone = first_data_zone

    def put_inode(idx, mode, size, nlinks, zones):
        o = (idx - 1) * INO_SIZE
        inodes[o:o+2] = u16(mode)
        inodes[o+2:o+4] = u16(0)
        inodes[o+4:o+8] = u32(size)
        inodes[o+8:o+12] = u32(0x5A000000)
        inodes[o+12] = 0
        inodes[o+13] = nlinks
        for i in range(9):
            inodes[o+14+i*2:o+16+i*2] = u16(zones[i] if i < len(zones) else 0)

    # katalogi: policz dzieci
    children = {}
    for e, kind in entries:
        parent = "" if "/" not in e else e.rsplit("/", 1)[0]
        children.setdefault(parent, []).append(e)
    ino_of = {e: i + 1 for i, (e, k) in enumerate(entries)}
    kind_of = {e: k for e, k in entries}

    for e, kind in entries:
        idx = ino_of[e]
        base_name = "" if e == "" else e.rsplit("/", 1)[-1]
        entries_bytes = bytearray()
        if e == "":
            entries_bytes += u16(idx) + b"." + b"\0" * 29
            entries_bytes += u16(idx) + b".." + b"\0" * 28
        else:
            entries_bytes += u16(idx) + b"." + b"\0" * 29
            parent = "" if "/" not in e else e.rsplit("/", 1)[0]
            entries_bytes += u16(ino_of[parent]) + b".." + b"\0" * 28
        if kind == "dir":
            for c in sorted(children.get(e, [])):
                nm = c.rsplit("/", 1)[-1].encode()[:30]
                entries_bytes += u16(ino_of[c]) + nm + b"\0" * (30 - len(nm))
            data = bytes(entries_bytes)
            mode = 0x41FF
            nlinks = 2 + len(children.get(e, []))
        else:
            data = dict(files)[e]
            mode = 0x81FF
            nlinks = 1
        nzones = (len(data) + BLK - 1) // BLK
        zone_list = []
        for i in range(nzones):
            z = next_zone + i
            zone_list.append(z)
        next_zone += nzones
        for z in zone_list:
            zmap[z // 8] |= 0x80 >> (z % 8)
        put_inode(idx, mode, len(data), nlinks, zone_list)
        for i, z in enumerate(zone_list):
            off = base + z * BLK
            buf[off:off + BLK] = data[i * BLK:(i + 1) * BLK].ljust(BLK, b"\0")

    buf[base + 2 * BLK:base + (2 + imap_blocks) * BLK] = imap
    buf[base + (2 + imap_blocks) * BLK:base + (2 + imap_blocks + zmap_blocks) * BLK] = zmap
    buf[base + (2 + imap_blocks + zmap_blocks) * BLK:
        base + (2 + imap_blocks + zmap_blocks + inode_blocks) * BLK] = inodes

    with open(out, "wb") as f:
        f.write(buf)
    print("OK image=%s mb=%d inodes=%d imap=%d zmap=%d first_data_zone=%d used_zones=%d"
          % (out, mb, n_inodes, imap_blocks, zmap_blocks, first_data_zone, next_zone - first_data_zone))
    print("dirs=%d files=%d" % (len(dirs), len(files)))

main()
