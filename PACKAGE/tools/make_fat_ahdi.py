#!/usr/bin/env python3
"""make_fat_ahdi.py <out.img> <mb> [label]

Tworzy obraz AHDI z jedna partycja FAT16 gotowa do wgrania plikow przez
install_files_ahdi.py. Tablica AHDI = big-endian, BPB FAT = little-endian.
"""
import struct, sys

BS = 512
PART_START = 1

def be16(v): return struct.pack(">H", v & 0xFFFF)
def be32(v): return struct.pack(">I", v & 0xFFFFFFFF)
def le16(v): return struct.pack("<H", v & 0xFFFF)
def le32(v): return struct.pack("<I", v & 0xFFFFFFFF)

def main():
    out, mb = sys.argv[1], int(sys.argv[2])
    label = (sys.argv[3] if len(sys.argv) > 3 else "ASF").upper()[:11]
    total = (mb * 1024 * 1024) // BS
    part_sec = total - PART_START

    spc = 8                       # 4 KB klastry (jak w referencji)
    reserved, nfats, roots = 1, 2, 512
    root_sec = (roots * 32 + BS - 1) // BS
    spf = 1
    while True:
        clusters = (part_sec - reserved - nfats * spf - root_sec) // spc
        need = (clusters + 2) * 2 // BS + 1
        if need <= spf:
            break
        spf = need
    clusters = (part_sec - reserved - nfats * spf - root_sec) // spc
    fat_start = PART_START + reserved
    root_start = fat_start + nfats * spf
    data_start = root_start + root_sec

    buf = bytearray(total * BS)
    # --- AHDI root sector (big-endian) ---
    o = 0x1C6
    buf[o] = 0x01
    buf[o+1:o+4] = b"BGM"
    buf[o+4:o+8] = be32(PART_START)
    buf[o+8:o+12] = be32(part_sec)
    s = 0
    for i in range(0, 510, 2):
        s += (buf[i] << 8) | buf[i+1]
    buf[0x1FE:0x200] = be16((0x1234 - (s & 0xFFFF)) & 0xFFFF)

    # --- BPB (little-endian) ---
    b = bytearray(BS)
    b[0:3] = b"\xeb\x3c\x90"
    b[3:11] = b"MSDOS5.0"
    b[11:13] = le16(BS)
    b[13] = spc
    b[14:16] = le16(reserved)
    b[16] = nfats
    b[17:19] = le16(roots)
    b[19:21] = le16(0)                     # total16 = 0
    b[21] = 0xF8
    b[22:24] = le16(spf)
    b[24:26] = le16(32)
    b[26:28] = le16(2)
    b[28:32] = le32(PART_START)
    b[32:36] = le32(part_sec)             # total32
    b[36] = 0x80
    b[38] = 0x29
    b[39:43] = le32(0x12345678)
    b[43:54] = label.ljust(11)[:11].encode()
    b[54:62] = b"FAT16   "
    b[510:512] = b"\x55\xaa"
    buf[PART_START * BS:PART_START * BS + BS] = b

    # --- tablice FAT ---
    fat = bytearray(spf * BS)
    fat[0:4] = le32(0xFFFFFFF8)
    for i in range(nfats):
        off = (fat_start + i * spf) * BS
        buf[off:off + len(fat)] = fat

    with open(out, "wb") as f:
        f.write(buf)
    print("OK %s mb=%d part_start=%d part_sec=%d spc=%d spf=%d root_sec=%d data_start=%d clusters=%d"
          % (out, mb, PART_START, part_sec, spc, spf, root_sec, data_start, clusters))

main()
