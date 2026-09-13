#!/usr/bin/env python3
"""make_xgm_ahdi.py <out.img> <mb> [label]

Obraz AHDI, w ktorym partycja FAT16 jest opisana przez lancuch XGM:
  sektor 0      : tablica AHDI, slot 0 = "XGM" -> deskryptor w sektorze 1
  sektor 1      : deskryptor XGM: wpis logiczny "BGM" (start wzgledem 1) + brak linku
  sektor 3..    : wlasciwa partycja FAT16
"""
import struct, sys
BS = 512
DESC = 1
PART_START = 3
def be16(v): return struct.pack(">H", v & 0xFFFF)
def be32(v): return struct.pack(">I", v & 0xFFFFFFFF)
def le16(v): return struct.pack("<H", v & 0xFFFF)
def le32(v): return struct.pack("<I", v & 0xFFFFFFFF)

def main():
    out, mb = sys.argv[1], int(sys.argv[2])
    label = (sys.argv[3] if len(sys.argv) > 3 else "XGM").upper()[:11]
    total = (mb * 1024 * 1024) // BS
    part_sec = total - PART_START
    spc = 8
    reserved, nfats, roots = 1, 2, 512
    root_sec = (roots * 32 + BS - 1) // BS
    spf = 1
    while True:
        clusters = (part_sec - reserved - nfats * spf - root_sec) // spc
        need = (clusters + 2) * 2 // BS + 1
        if need <= spf: break
        spf = need
    clusters = (part_sec - reserved - nfats * spf - root_sec) // spc
    fat_start = PART_START + reserved
    root_start = fat_start + nfats * spf
    data_start = root_start + root_sec
    buf = bytearray(total * BS)

    # --- AHDI root: XGM entry in slot 0 ---
    o = 0x1C6
    buf[o] = 0x01
    buf[o+1:o+4] = b"XGM"
    buf[o+4:o+8] = be32(DESC)
    buf[o+8:o+12] = be32(part_sec + 2)
    s = 0
    for i in range(0, 510, 2): s += (buf[i] << 8) | buf[i+1]
    buf[0x1FE:0x200] = be16((0x1234 - (s & 0xFFFF)) & 0xFFFF)

    # --- XGM descriptor in sector DESC: logical BGM + no link ---
    d = DESC * BS
    buf[d+0x1C6] = 0x01
    buf[d+0x1C7:d+0x1CA] = b"BGM"
    buf[d+0x1CA:d+0x1CE] = be32(PART_START - DESC)
    buf[d+0x1CE:d+0x1D2] = be32(part_sec)
    # link entry at 0x1C6+12 left inactive on purpose

    # --- FAT16 BPB of the logical partition ---
    b = bytearray(BS)
    b[0:3] = b"\xeb\x3c\x90"
    b[3:11] = b"MSDOS5.0"
    b[11:13] = le16(BS); b[13] = spc; b[14:16] = le16(reserved); b[16] = nfats
    b[17:19] = le16(roots); b[19:21] = le16(0); b[21] = 0xF8; b[22:24] = le16(spf)
    b[24:26] = le16(32); b[26:28] = le16(2); b[28:32] = le32(PART_START)
    b[32:36] = le32(part_sec); b[36] = 0x80; b[38] = 0x29
    b[39:43] = le32(0x12345678); b[43:54] = label.ljust(11)[:11].encode()
    b[54:62] = b"FAT16   "; b[510:512] = b"\x55\xaa"
    buf[PART_START*BS:PART_START*BS+BS] = b

    fat = bytearray(spf*BS); fat[0:4] = le32(0xFFFFFFF8)
    for i in range(nfats):
        off = (fat_start + i*spf)*BS
        buf[off:off+len(fat)] = fat

    open(out, "wb").write(buf)
    print("OK %s mb=%d descriptor=%d part_start=%d part_sec=%d spf=%d clusters=%d"
          % (out, mb, DESC, PART_START, part_sec, spf, clusters))

main()
