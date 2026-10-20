#!/usr/bin/env python3
"""Write a host file into an ATOS AHDI FAT16 image, overwriting an existing file.

Config files such as \\SYSTEM\\LAUNCH.CFG cannot go through
install_boot_sys.py (that tool only accepts Atari executables), so this writes
plain data.  The chain of the existing file is reused; the file must therefore
already be allocated long enough for the new content.
"""
import struct, sys

def le16(b, o): return struct.unpack_from("<H", b, o)[0]
def le32(b, o): return struct.unpack_from("<I", b, o)[0]
def be32(b, o): return struct.unpack_from(">I", b, o)[0]

class Vol:
    def __init__(self, image):
        self.d = open(image, "r+b")
        root = self.rd(0, 512)
        parts = [(be32(root, 0x1C6 + n * 12 + 4), root[0x1C7 + n * 12:0x1CA + n * 12])
                 for n in range(4) if root[0x1C6 + n * 12] & 1]
        if not parts:
            raise SystemExit("no partition")
        part, _ = parts[0]
        self.base = part * 512
        bpb = self.rd(self.base, 512)
        self.bps = le16(bpb, 11)
        self.spc = bpb[13]
        reserved = le16(bpb, 14)
        fats = bpb[16]
        roots = le16(bpb, 17)
        spf = le16(bpb, 22)
        self.fat0 = self.base + reserved * self.bps
        self.root_off = self.fat0 + fats * spf * self.bps
        self.root_bytes = ((roots * 32 + self.bps - 1) // self.bps) * self.bps
        self.data_off = self.root_off + self.root_bytes
        self.cb = self.spc * self.bps

    def rd(self, o, s):
        self.d.seek(o); return self.d.read(s)

    def wr(self, o, b):
        self.d.seek(o); self.d.write(b)

    def coff(self, c): return self.data_off + (c - 2) * self.cb

    def fat_next(self, c):
        return le16(self.rd(self.fat0 + c * 2, 2), 0)

    def find(self, path):
        """Return (dir_entry_offset, first_cluster, size) for an existing file."""
        parts = [p for p in path.replace("/", "\\").split("\\") if p]
        name = parts.pop()
        if parts and parts[0].upper().rstrip(":") in ("A", "C", "D"):
            parts.pop(0)
        doff, dbytes = self.root_off, self.root_bytes
        for seg in parts:
            entry = self._scan(doff, dbytes, seg)
            if not entry:
                raise SystemExit("directory not found: " + seg)
            cl = le16(self.rd(entry + 26, 2), 0)
            doff, dbytes = self.coff(cl), self.cb
        e = self._scan(doff, dbytes, name)
        if not e:
            raise SystemExit("file not found: " + path)
        return e, le16(self.rd(e + 26, 2), 0), le32(self.rd(e + 28, 4), 0)

    def _scan(self, off, size, name):
        want = (name.upper().split(".") + [""])[:2]
        want = want[0][:8].ljust(8) + want[1][:3].ljust(3)
        for i in range(0, size, 32):
            e = self.rd(off + i, 32)
            if not e or e[0] == 0:
                return None
            if e[0] == 0xE5 or (e[11] & 0x0F) == 0x0F:
                continue
            if e[0:11].decode("latin-1").upper() == want:
                return off + i
        return None

    def write_file(self, path, data):
        e, cl, old = self.find(path)
        need = max(1, (len(data) + self.cb - 1) // self.cb)
        chain, c = [], cl
        while c and c < 0xFFF8 and len(chain) < need:
            chain.append(c)
            c = self.fat_next(c)
        if len(chain) < need:
            raise SystemExit("file too small on disk: needs %d clusters, has %d"
                             % (need, len(chain)))
        for i, c in enumerate(chain):
            chunk = data[i * self.cb:(i + 1) * self.cb]
            self.wr(self.coff(c), chunk + b"\0" * (self.cb - len(chunk)))
        self.wr(e + 28, struct.pack("<I", len(data)))
        self.d.flush()
        return len(data)


def main():
    if len(sys.argv) != 4:
        raise SystemExit("usage: img_put.py <image> <atari-path> <host-file>")
    image, apath, host = sys.argv[1], sys.argv[2], sys.argv[3]
    data = open(host, "rb").read()
    v = Vol(image)
    n = v.write_file(apath, data)
    print("%s: wrote %d bytes" % (apath, n))


if __name__ == "__main__":
    main()
