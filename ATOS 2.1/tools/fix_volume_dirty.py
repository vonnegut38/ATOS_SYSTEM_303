#!/usr/bin/env python3
"""Naprawa znacznika czystosci FAT na obrazie ATOS (AHDI).

Jadro ATOS trzyma w FAT[1] standardowe znaczniki:
    bit 15 = "clean shutdown"  (0 = wolumin DIRTY)
    bit 14 = "no hard error"   (0 = blad krytyczny)
Przed kazda transakcja zapisu jadro czysci bit 15, a po udanym zapisie go
ustawia.  Jesli maszyna zostanie zresetowana (albo emulator ubity) w trakcie
zapisu, bit 15 zostaje wyczyszczony i jadro ODMAWIA WSZYSTKICH ZAPISOW
(-FAT_WRITE_READ_ONLY) do czasu naprawy.  W systemie nie ma fsck-a, wiec ten
skrypt robi minimum: sprawdza, czy obie kopie FAT sa identyczne i czy pierwsze
wpisy sa sensowne, a potem ustawia bity 14+15 ("clean, no errors").

Uzycie:
    fix_volume_dirty.py <obraz.img>            # tylko raport
    fix_volume_dirty.py <obraz.img> --clear    # napraw (zapisuje na obrazie)
"""
import struct, sys, pathlib

SECTOR = 512

def active_ahdi_partition(root: bytes):
    for n in range(4):
        o = 0x1C6 + n * 12
        if root[o] & 1:
            return struct.unpack_from(">I", root, o + 4)[0], root[o + 1:o + 4]
    return None, None

def main():
    if len(sys.argv) < 2:
        print(__doc__); return 2
    img = pathlib.Path(sys.argv[1])
    clear = "--clear" in sys.argv
    dirty = "--dirty" in sys.argv
    d = img.open("r+b" if (clear or dirty) else "rb")
    root = d.read(SECTOR)
    part, pid = active_ahdi_partition(root)
    if part is None:
        print("brak aktywnej partycji AHDI"); return 1
    base = part * SECTOR
    d.seek(base); bpb = d.read(SECTOR)
    bps = struct.unpack_from("<H", bpb, 11)[0]
    reserved = struct.unpack_from("<H", bpb, 14)[0]
    fats = bpb[16]
    spf = struct.unpack_from("<H", bpb, 22)[0]
    fat_start = base + reserved * bps                      # pierwszy sektor FAT
    scale = bps // SECTOR
    def fat_copy(n):
        d.seek(fat_start + n * spf * scale * SECTOR)
        return d.read(min(spf * bps, 64 * 1024))
    f0 = fat_copy(0)
    f1 = fat_copy(1) if fats > 1 else f0
    flags = struct.unpack_from("<H", f0, 2)[0]             # FAT[1]
    clean = bool(flags & 0x8000); noerr = bool(flags & 0x4000)
    print(f"partycja {pid.decode('ascii','replace')} @ LBA {part}, FAT start {fat_start}, "
          f"kopie FAT: {fats}, bps={bps}")
    print(f"FAT[1] = 0x{flags:04x}  clean(bit15)={int(clean)}  no-error(bit14)={int(noerr)}")
    same = (f0 == f1)
    print(f"obie kopie FAT identyczne: {'tak' if same else 'NIE (uwaga!)'}")
    if dirty:
        new = flags & ~0x8000
        for n in range(fats):
            off = fat_start + n * spf * scale * SECTOR
            d.seek(off); cur = d.read(4)
            d.seek(off); d.write(struct.pack("<H", struct.unpack_from("<H", cur, 0)[0]) + struct.pack("<H", new))
        d.flush()
        print(f"USTAWIONE NA DIRTY (test): FAT[1] = 0x{new:04x}")
        return 0
    if clean and noerr:
        print("wolumin jest CZYSTY - zapis powinien dzialac")
        return 0
    print("wolumin jest DIRTY/BLAD - jadro odmowi zapisu do czasu naprawy")
    if not clear:
        print("(uzyj --clear, aby ustawic bity 14+15)")
        return 0
    if not same:
        print("ODMOWA: kopie FAT sie roznia - najpierw trzeba ustalic, ktora jest wlasciwa")
        return 1
    new = flags | 0x8000 | 0x4000
    for n in range(fats):
        off = fat_start + n * spf * scale * SECTOR
        d.seek(off); cur = d.read(4)
        d.seek(off); d.write(struct.pack("<H", struct.unpack_from("<H", cur, 0)[0]) + struct.pack("<H", new))
    d.flush()
    print(f"NAPRAWIONE: FAT[1] = 0x{new:04x} (clean + no-error), zapisane w {fats} kopii")
    return 0

if __name__ == "__main__":
    sys.exit(main())
