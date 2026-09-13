#!/usr/bin/env python3
"""Wgraj pliki do katalogu w korzeniu pierwszej aktywnej partycji AHDI FAT16.

Uzycie:
    python3 install_files_ahdi.py <obraz> <katalog> <plik1> [plik2 ...]

Wgrywa pliki (8.3) do <katalog> w korzeniu partycji, tworzac go, jesli nie istnieje.
Do uruchamiania sterownikow przy starcie uzyj katalogu AUTO - TOS wykonuje
wlasnie AUTO; katalog o innej nazwie nie zostanie uruchomiony.

UWAGA: pracuj na KOPII obrazu. Skrypt modyfikuje tablice FAT.
"""
import pathlib
import struct
import sys


def be32(b, o):
    return struct.unpack_from(">I", b, o)[0]


def le16(b, o):
    return struct.unpack_from("<H", b, o)[0]


def main():
    if len(sys.argv) < 4:
        raise SystemExit(__doc__)

    image = pathlib.Path(sys.argv[1])
    dirname = sys.argv[2].upper()
    files = [pathlib.Path(p) for p in sys.argv[3:]]

    if len(dirname) > 8:
        raise SystemExit("nazwa katalogu musi byc nazwa 8.3")

    disk = image.open("r+b")

    def read_at(offset, size):
        disk.seek(offset)
        return disk.read(size)

    def write_at(offset, data):
        disk.seek(offset)
        disk.write(data)

    # --- tablica partycji AHDI ---
    root = read_at(0, 512)
    parts = []
    for n in range(4):
        o = 0x1C6 + n * 12
        if root[o] & 1:
            parts.append((be32(root, o + 4), root[o + 1:o + 4]))
    if not parts:
        raise SystemExit("brak aktywnej partycji AHDI")
    part, _pid = parts[0]
    base = part * 512

    bpb = read_at(base, 512)
    bps = le16(bpb, 11)
    spc = bpb[13]
    reserved = le16(bpb, 14)
    fats = bpb[16]
    roots = le16(bpb, 17)
    spf = le16(bpb, 22)
    if bps < 512 or bps > 8192 or bps & (bps - 1) or not all((spc, reserved, fats, roots, spf)):
        raise SystemExit("nieobslugiwany BPB FAT16")

    fat0 = base + reserved * bps
    root_off = fat0 + fats * spf * bps
    root_bytes = ((roots * 32 + bps - 1) // bps) * bps
    data_off = root_off + root_bytes
    cluster_bytes = spc * bps
    max_clusters = (spf * bps) // 2

    def fat_get(c):
        return le16(read_at(fat0 + c * 2, 2), 0)

    def fat_set(c, value):
        v = struct.pack("<H", value)
        for f in range(fats):
            write_at(fat0 + f * spf * bps + c * 2, v)

    def cluster_off(c):
        return data_off + (c - 2) * cluster_bytes

    def chain(first):
        seen = set()
        c = first
        while 2 <= c < 0xFFF8:
            if c in seen:
                raise SystemExit("petla w FAT")
            seen.add(c)
            yield c
            c = fat_get(c)

    def alloc_cluster():
        for c in range(2, max_clusters):
            if fat_get(c) == 0:
                return c
        raise SystemExit("brak wolnych klastrow")

    # --- znajdz lub utworz katalog w korzeniu ---
    want_dir = dirname.ljust(11).encode("ascii")
    want_attr = 0x10
    dir_cluster = 0
    dir_slot = None
    free_root = None
    for o in range(root_off, root_off + roots * 32, 32):
        e = read_at(o, 32)
        if e[0] in (0, 0xE5):
            if free_root is None:
                free_root = o
            continue
        if e[:11] == want_dir and e[11] & 0x10:
            dir_cluster = le16(e, 26)
            break

    if dir_cluster < 2:
        if free_root is None:
            raise SystemExit("korzen nie ma wolnego wpisu")
        dir_cluster = alloc_cluster()
        fat_set(dir_cluster, 0xFFFF)
        block = bytearray(cluster_bytes)
        block[0:11] = b".          "
        block[11] = 0x10
        struct.pack_into("<H", block, 26, dir_cluster)
        block[32:43] = b"..         "
        block[43] = 0x10
        struct.pack_into("<H", block, 58, 0)  # korzen = 0
        write_at(cluster_off(dir_cluster), block)
        entry = bytearray(32)
        entry[:11] = want_dir
        entry[11] = want_attr
        struct.pack_into("<H", entry, 26, dir_cluster)
        write_at(free_root, entry)
        print(f"utworzono katalog {dirname} (klaster {dir_cluster})")
    else:
        print(f"katalog {dirname} istnieje (klaster {dir_cluster})")

    # --- wgraj pliki ---
    for src in files:
        payload = src.read_bytes()
        name = src.name.upper()
        stem, _, ext = name.partition(".")
        if not stem or len(stem) > 8 or len(ext) > 3:
            raise SystemExit(f"{name}: nazwa musi byc 8.3")
        target = (stem.ljust(8) + ext.ljust(3)).encode("ascii")

        slot = None
        old_first = 0
        for c in chain(dir_cluster):
            co = cluster_off(c)
            for o in range(co, co + cluster_bytes, 32):
                e = read_at(o, 32)
                if e[:11] == target:
                    slot = o
                    old_first = le16(e, 26)
                    break
                if slot is None and e[0] in (0, 0xE5):
                    slot = o
            if old_first:
                break
        if slot is None:
            raise SystemExit(f"{dirname}: brak wolnego wpisu")

        if old_first:
            for c in list(chain(old_first)):
                fat_set(c, 0)

        needed = (len(payload) + cluster_bytes - 1) // cluster_bytes
        free = []
        c = 2
        while len(free) < needed and c < max_clusters:
            if fat_get(c) == 0:
                free.append(c)
            c += 1
        if len(free) != needed:
            raise SystemExit("brak miejsca na plik")

        for n, cc in enumerate(free):
            fat_set(cc, free[n + 1] if n + 1 < len(free) else 0xFFFF)
            chunk = payload[n * cluster_bytes:(n + 1) * cluster_bytes]
            write_at(cluster_off(cc), chunk + b"\0" * (cluster_bytes - len(chunk)))

        entry = bytearray(32)
        entry[:11] = target
        entry[11] = 0x20
        struct.pack_into("<H", entry, 26, free[0])
        struct.pack_into("<I", entry, 28, len(payload))
        write_at(slot, entry)
        print(f"wgrany {name} ({len(payload)} B) jako {dirname}\\{name}")

    disk.flush()
    disk.close()
    print("gotowe")


if __name__ == "__main__":
    main()
