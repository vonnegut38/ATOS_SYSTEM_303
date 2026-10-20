#!/usr/bin/env python3
"""Rename a file inside an ATOS AHDI FAT16 image (8.3 name only).

Used to temporarily disable the desktop auto-start (DESKTOP.APP -> DESKTOP.APX)
when a scenario needs the text shell to own the keyboard, then restore it.

    python3 work/rename_app.py <image> <path\\FILE.EXT> <NEWNAME.EXT>
"""
import struct
import sys


def le16(b, o):
    return struct.unpack_from("<H", b, o)[0]


def be32(b, o):
    return struct.unpack_from(">I", b, o)[0]


def main(image, path, newname):
    d = open(image, "r+b")

    def rd(o, s):
        d.seek(o)
        return d.read(s)

    def wr(o, b):
        d.seek(o)
        d.write(b)

    root = rd(0, 512)
    parts = [(be32(root, 0x1C6 + n * 12 + 4), root[0x1C7 + n * 12:0x1CA + n * 12])
             for n in range(4) if root[0x1C6 + n * 12] & 1]
    part, _pid = parts[0]
    base = part * 512
    bpb = rd(base, 512)
    bps = le16(bpb, 11)
    spc = bpb[13]
    reserved = le16(bpb, 14)
    fats = bpb[16]
    roots = le16(bpb, 17)
    spf = le16(bpb, 22)
    fat0 = base + reserved * bps
    root_off = fat0 + fats * spf * bps
    root_bytes = ((roots * 32 + bps - 1) // bps) * bps
    data_off = root_off + root_bytes
    cb = spc * bps

    def coff(c):
        return data_off + (c - 2) * cb

    def fg(c):
        return le16(rd(fat0 + c * 2, 2), 0)

    def chain(f):
        c = f
        seen = set()
        while 2 <= c < 0xFFF8 and c not in seen:
            seen.add(c)
            yield c
            c = fg(c)

    comps = [p.upper() for p in path.replace("\\", "/").split("/") if p]
    cur = 0
    for comp in comps[:-1]:
        wanted = comp.ljust(11).encode()[:11]
        found = 0
        if cur == 0:
            rng = [(root_off, roots)]
        else:
            rng = [(coff(c), cb // 32) for c in chain(cur)]
        for off, count in rng:
            for i in range(count):
                o = off + i * 32
                e = rd(o, 32)
                if e[0] == 0:
                    break
                if e[0] != 0xE5 and e[:11] == wanted and e[11] & 0x10:
                    found = le16(e, 26)
                    break
            if found:
                break
        if not found:
            raise SystemExit("dir not found: " + comp)
        cur = found

    fname, _, fext = comps[-1].partition(".")
    wanted = (fname.ljust(8) + fext.ljust(3)).encode()[:11]
    newf, _, newe = newname.upper().partition(".")
    new11 = (newf.ljust(8) + newe.ljust(3)).encode()[:11]
    if len(new11) != 11:
        raise SystemExit("new name must be 8.3")
    rng = [(root_off, roots)] if cur == 0 else [(coff(c), cb // 32) for c in chain(cur)]
    for off, count in rng:
        for i in range(count):
            o = off + i * 32
            e = rd(o, 32)
            if e[0] == 0:
                break
            if e[0] != 0xE5 and e[:11] == wanted:
                wr(o, new11 + e[11:])
                d.close()
                print("renamed %s -> %s" % (path, newname))
                return
    d.close()
    raise SystemExit("file not found: " + comps[-1])


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2], sys.argv[3])
