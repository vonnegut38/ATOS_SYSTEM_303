#!/usr/bin/env python3
"""install_path_ahdi.py <image> <dir/sub/dir> <file...>

Tworzy zagniezdzone katalogi 8.3 i kopiuje pliki. Obsluguje FAT16 i FAT32.
Pracuj na kopii obrazu.
"""
import pathlib, struct, sys

def be32(b, o): return struct.unpack_from(">I", b, o)[0]
def le16(b, o): return struct.unpack_from("<H", b, o)[0]
def le32(b, o): return struct.unpack_from("<I", b, o)[0]

def main():
    if len(sys.argv) < 4:
        raise SystemExit(__doc__)
    image = pathlib.Path(sys.argv[1])
    comps = [c.upper() for c in sys.argv[2].split("/") if c]
    files = [pathlib.Path(p) for p in sys.argv[3:]]
    disk = image.open("r+b")
    def rd(o, n):
        disk.seek(o); return disk.read(n)
    def wr(o, d):
        disk.seek(o); disk.write(d)
    root = rd(0, 512)
    part = None
    for n in range(4):
        o = 0x1C6 + n * 12
        if not (root[o] & 1):
            continue
        pid = root[o+1:o+4]
        start = be32(root, o + 4)
        if pid == b"XGM":
            desc = start
            for _ in range(16):
                ds = rd(desc * 512, 512)
                lo = 0x1C6
                if (ds[lo] & 1) and ds[lo+1:lo+4] != b"XGM":
                    part = desc + be32(ds, lo + 4)
                    break
                li = lo + 12
                if not (ds[li] & 1) or ds[li+1:li+4] != b"XGM":
                    break
                rel = be32(ds, li + 4)
                if not rel:
                    break
                desc += rel
            if part is not None:
                break
        else:
            part = start
            break
    if part is None: raise SystemExit("brak aktywnej partycji AHDI")
    base = part * 512
    bpb = rd(base, 512)
    bps = le16(bpb, 11); spc = bpb[13]; reserved = le16(bpb, 14)
    fats = bpb[16]; roots = le16(bpb, 17)
    spf16 = le16(bpb, 22); spf32 = le32(bpb, 36)
    if spf16:
        fat32 = False; spf = spf16
    elif spf32:
        fat32 = True; spf = spf32; roots = 0
    else:
        raise SystemExit("nieobslugiwany BPB")
    fat0 = base + reserved * bps
    eoc = 0x0FFFFFFF if fat32 else 0xFFFF
    ent_size = 4 if fat32 else 2
    if fat32:
        root_clus = le32(bpb, 44)
        data_off = fat0 + fats * spf * bps
        max_clusters = min((spf * bps) // 4, 0x0FFFFFF5)
    else:
        root_off = fat0 + fats * spf * bps
        root_bytes = ((roots * 32 + bps - 1) // bps) * bps
        data_off = root_off + root_bytes
        max_clusters = (spf * bps) // 2
    cluster_bytes = spc * bps

    def fat_get(c):
        o = fat0 + c * ent_size
        return le32(rd(o, 4), 0) & 0x0FFFFFFF if fat32 else le16(rd(o, 2), 0)
    def fat_set(c, v):
        for f in range(fats):
            o = fat0 + f * spf * bps + c * ent_size
            wr(o, struct.pack("<I", v & 0x0FFFFFFF) if fat32 else struct.pack("<H", v & 0xFFFF))
    def coff(c): return data_off + (c - 2) * cluster_bytes
    def alloc():
        for c in range(2, max_clusters):
            if fat_get(c) == 0:
                fat_set(c, eoc)
                return c
        raise SystemExit("brak wolnych klastrow")
    def slots_for_chain(first, fixed_off=None, fixed_bytes=0):
        if fixed_off is not None:
            for o in range(fixed_off, fixed_off + fixed_bytes, 32):
                yield o, rd(o, 32)
            return
        c = first; seen = set()
        while 2 <= c < eoc:
            if c in seen: raise SystemExit("petla FAT")
            seen.add(c)
            co = coff(c)
            for o in range(co, co + cluster_bytes, 32):
                yield o, rd(o, 32)
            c = fat_get(c)
    def dir_slots(dc):
        if dc < 2:
            if fat32:
                yield from slots_for_chain(root_clus)
            else:
                yield from slots_for_chain(0, root_off, roots * 32)
        else:
            yield from slots_for_chain(dc)
    def ent_cluster(e):
        if fat32:
            return (le16(e, 20) << 16) | le16(e, 26)
        return le16(e, 26)
    def set_ent_cluster(e, c):
        struct.pack_into("<H", e, 26, c & 0xFFFF)
        if fat32: struct.pack_into("<H", e, 20, (c >> 16) & 0xFFFF)
    def find_dir(dc, name11):
        for o, e in dir_slots(dc):
            if e[0] in (0, 0xE5): continue
            if e[:11] == name11 and (e[11] & 0x10):
                return ent_cluster(e)
        return None
    def free_chain(first):
        c = first; seen = set()
        while 2 <= c < eoc:
            if c in seen: break
            seen.add(c)
            nxt = fat_get(c); fat_set(c, 0); c = nxt
    def create_dir(dc, dirname):
        name11 = dirname.ljust(11).encode("ascii")[:11]
        existing = find_dir(dc, name11)
        if existing is not None:
            return existing
        slot = None
        for o, e in dir_slots(dc):
            if e[0] in (0, 0xE5): slot = o; break
        if slot is None: raise SystemExit("katalog pelny")
        c = alloc()
        block = bytearray(cluster_bytes)
        block[0:11] = b".          "; block[11] = 0x10
        e = bytearray(block[0:32]); set_ent_cluster(e, c); block[0:32] = e
        block[32:43] = b"..         "; block[43] = 0x10
        e2 = bytearray(block[32:64]); set_ent_cluster(e2, dc if dc >= 2 else 0); block[32:64] = e2
        wr(coff(c), block)
        entry = bytearray(32); entry[:11] = name11; entry[11] = 0x10
        set_ent_cluster(entry, c)
        wr(slot, entry)
        print("katalog", dirname, "-> klaster", c)
        return c
    def write_file(dc, name11, payload):
        slot = None
        for o, e in dir_slots(dc):
            if e[:11] == name11: slot = o; break
            if slot is None and e[0] in (0, 0xE5): slot = o
        if slot is None: raise SystemExit("katalog pelny")
        old = rd(slot, 32)
        if old[0] not in (0, 0xE5) and old[:11] == name11:
            free_chain(ent_cluster(old))
        needed = (len(payload) + cluster_bytes - 1) // cluster_bytes
        chain = [alloc() for _ in range(needed)]
        for i, c in enumerate(chain):
            fat_set(c, chain[i + 1] if i + 1 < len(chain) else eoc)
            chunk = payload[i * cluster_bytes:(i + 1) * cluster_bytes]
            wr(coff(c), chunk + b"\0" * (cluster_bytes - len(chunk)))
        e = bytearray(32); e[:11] = name11; e[11] = 0x20
        set_ent_cluster(e, chain[0])
        struct.pack_into("<I", e, 28, len(payload))
        wr(slot, e)
        print("plik", name11.decode(), len(payload), "B, klaster", chain[0])
    dc = 0
    for comp in comps:
        dc = create_dir(dc, comp)
    for src in files:
        payload = src.read_bytes()
        stem, _, ext = src.name.upper().partition(".")
        if len(stem) > 8 or len(ext) > 3:
            raise SystemExit(f"{src.name}: nazwa musi byc 8.3")
        write_file(dc, (stem.ljust(8) + ext.ljust(3))[:11].encode("ascii"), payload)
    disk.flush(); disk.close()
    print("FS:", "FAT32" if fat32 else "FAT16")
    print("gotowe")

if __name__ == "__main__":
    main()
