#!/usr/bin/env python3
r"""Nadpisz plik w podkatalogu obrazu AHDI FAT16 (8.3). Uzycie:
   python3 install_file.py <obraz> <SCIERZKA>\<PLIK.8.3> <plik>
Napisane, bo tools/install_files_ahdi.py obsluguje tylko katalogi w KORZENIU.
Aktualizuje OBA FAT-y, alokuje wolne klastry i wpis katalogowy."""
import struct, sys, pathlib
def main():
    if len(sys.argv) != 4:
        raise SystemExit(__doc__)
    img, target, src = pathlib.Path(sys.argv[1]), sys.argv[2].replace("/", "\\").strip("\\").upper(), pathlib.Path(sys.argv[3])
    data = src.read_bytes()
    f = open(img, "r+b")
    rd = lambda o, n: (f.seek(o), f.read(n))[1]
    be32 = lambda b, o: struct.unpack(">I", b[o:o+4])[0]
    le16 = lambda b, o: struct.unpack("<H", b[o:o+2])[0]
    le32 = lambda b, o: struct.unpack("<I", b[o:o+4])[0]
    root = rd(0, 512)
    base = be32(root, 0x1C6 + 4) * 512
    bpb = rd(base, 512)
    spc, res = bpb[13], le16(bpb, 14)
    fats, roots, spf = bpb[16], le16(bpb, 17), (le16(bpb, 22) or le32(bpb, 36))
    fat_off = base + res * 512
    root_off = base + (res + fats * spf) * 512
    data_off = root_off + roots * 32
    fat = rd(fat_off, spf * 512)
    def chain(c):
        out = []
        while 2 <= c < 0xFFF8 and len(out) < 200000:
            out.append(c); c = le16(fat, c * 2)
            if c in out: break
        return out
    def dirents(off, n):
        raw = rd(off, n); out = []
        for i in range(0, len(raw), 32):
            e = raw[i:i+32]
            if not e or e[0] == 0: break
            if e[0] == 0xE5 or e[11] & 0x0F == 0x0F: continue
            nm = (e[0:8].decode('latin1').rstrip() + "." + e[8:11].decode('latin1').rstrip()).strip(".")
            out.append((off + i, nm, le16(e, 26), le32(e, 28), e[11]))
        return out
    parts = target.split("\\")
    d = [x for x in dirents(root_off, roots * 32) if x[1].upper() == parts[0]]
    if not d: raise SystemExit("brak katalogu " + parts[0])
    cl = d[0][2]
    for name in parts[1:-1]:
        e = [x for x in dirents(data_off + (cl - 2) * spc * 512, spc * 512) if x[1].upper() == name]
        if not e: raise SystemExit("brak katalogu " + name)
        cl = e[0][2]
    dir_off = data_off + (cl - 2) * spc * 512
    # Sprawdzamy miejsce ZANIM cokolwiek zapiszemy (takze przed utworzeniem
    # wpisu katalogowego): przy braku miejsca obraz musi zostac nietkniety.
    _need = (len(data) + spc * 512 - 1) // (spc * 512)
    _fat = rd(fat_off, spf * 512)
    _free = [i for i in range(2, len(_fat) // 2) if le16(_fat, i * 2) == 0]
    if len(_free) < _need:
        raise SystemExit(f"brak miejsca: potrzeba {_need} klastrow, wolnych {len(_free)} (obraz nietkniety)")
    ent = [x for x in dirents(dir_off, spc * 512) if x[1].upper() == parts[-1]]
    if ent:
        off, name, old, oldsz, attr = ent[0]
    else:
        # Nie ma pliku: UTWORZ wpis 8.3 w wolnym slocie katalogu.  Szukamy
        # wpisu skasowanego (0xE5) albo konca katalogu (0x00).
        raw = rd(dir_off, spc * 512)
        slot = -1
        for i in range(0, len(raw), 32):
            if raw[i] == 0x00 or raw[i] == 0xE5:
                slot = i; break
        if slot < 0: raise SystemExit("brak wolnego wpisu w katalogu")
        base8 = parts[-1].split(".")[0][:8].ljust(8)
        ext3 = (parts[-1].split(".")[1] if "." in parts[-1] else "")[:3].ljust(3)
        e = bytearray(32)
        e[0:8] = base8.encode("latin1")
        e[8:11] = ext3.encode("latin1")
        e[11] = 0x20                      # archiwum
        f.seek(dir_off + slot); f.write(bytes(e))
        off, name, old, oldsz = dir_off + slot, parts[-1], 0, 0
        fat = rd(fat_off, spf * 512)      # odswiez po zapisie wpisu
        print(f"utworzono wpis {parts[-1]} w katalogu")
    # NAJPIERW sprawdzamy miejsce (stare klastry licza sie jako dostepne),
    # dopiero potem cokolwiek zmieniamy.  Poprzednia wersja zwalniala stary
    # lancuch PRZED sprawdzeniem miejsca, wiec przy braku miejsca uszkadzala
    # plik (audyt r2 odtworzyl to na sztucznym obrazie).
    freed = set(chain(old))
    fat = rd(fat_off, spf * 512)
    free = [i for i in range(2, len(fat) // 2) if le16(fat, i * 2) == 0 or i in freed]
    need = (len(data) + spc * 512 - 1) // (spc * 512)
    if len(free) < need:
        raise SystemExit(f"brak miejsca: potrzeba {need} klastrow, dostepne {len(free)}")
    pick = free[:need]
    blob = data + b"\x00" * (need * spc * 512 - len(data))
    for i, c in enumerate(pick):
        f.seek(data_off + (c - 2) * spc * 512); f.write(blob[i*spc*512:(i+1)*spc*512])
        nxt = pick[i+1] if i + 1 < len(pick) else 0xFFFF
        for k in range(fats):
            f.seek(fat_off + k * spf * 512 + c * 2); f.write(struct.pack("<H", nxt))
    # Zwolnij tylko te stare klastry, ktorych nowy plik NIE uzyl.
    for c in freed - set(pick):
        for k in range(fats):
            f.seek(fat_off + k * spf * 512 + c * 2); f.write(struct.pack("<H", 0))
    f.seek(off + 26); f.write(struct.pack("<H", pick[0]))
    f.seek(off + 28); f.write(struct.pack("<I", len(data)))
    f.flush(); f.close()
    print(f"{target}: {oldsz} -> {len(data)} B (klaster {pick[0]})")
main()
