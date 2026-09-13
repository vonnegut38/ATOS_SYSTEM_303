#!/usr/bin/env python3
"""make_fat32_ahdi.py <out.img> <mb> [label]

Obraz AHDI z jedna partycja FAT32 (ID "F32").
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
    spc, reserved, nfats = 8, 32, 2
    spf = 1
    while True:
        clusters = (part_sec - reserved - nfats * spf) // spc
        need = (clusters + 2) * 4 // BS + 1
        if need <= spf: break
        spf = need
    clusters = (part_sec - reserved - nfats * spf) // spc
    fat_start = PART_START + reserved
    data_start = fat_start + nfats * spf
    buf = bytearray(total * BS)

    o = 0x1C6
    buf[o] = 1
    buf[o+1:o+4] = b"F32"
    buf[o+4:o+8] = be32(PART_START)
    buf[o+8:o+12] = be32(part_sec)
    s = 0
    for i in range(0, 510, 2): s += (buf[i] << 8) | buf[i+1]
    buf[0x1FE:0x200] = be16((0x1234 - (s & 0xFFFF)) & 0xFFFF)

    b = bytearray(BS)
    b[0:3] = b"\xeb\x58\x90"
    b[3:11] = b"MSWIN4.1"
    b[11:13] = le16(BS)
    b[13] = spc
    b[14:16] = le16(reserved)
    b[16] = nfats
    b[17:19] = le16(0)          # root entries = 0
    b[19:21] = le16(0)          # total16 = 0
    b[21] = 0xF8
    b[22:24] = le16(0)          # spf16 = 0 -> FAT32
    b[24:26] = le16(63)
    b[26:28] = le16(255)
    b[28:32] = le32(PART_START)
    b[32:36] = le32(part_sec)
    b[36:40] = le32(spf)
    b[40:42] = le16(0)          # ext flags
    b[42:44] = le16(0)          # fs version
    b[44:48] = le32(2)          # root cluster
    b[48:50] = le16(1)          # FSInfo sector
    b[50:52] = le16(6)          # backup boot sector
    b[64] = 0x80
    b[66] = 0x29
    b[67:71] = le32(0x12345678)
    b[71:82] = label.ljust(11)[:11].encode()
    b[82:90] = b"FAT32   "
    b[510:512] = b"\x55\xaa"
    def put(sec, data): buf[sec*BS:sec*BS+len(data)] = data
    put(PART_START, b)
    put(PART_START+6, b)

    fi = bytearray(BS)
    fi[0:4] = le32(0x41615252)
    fi[484:488] = le32(0x61417272)
    fi[488:492] = le32(0xFFFFFFFF)
    fi[492:496] = le32(0xFFFFFFFF)
    fi[508:512] = b"\x00\x00\x55\xaa"
    put(PART_START+1, fi)
    put(PART_START+7, fi)

    fat = bytearray(spf * BS)
    fat[0:4] = le32(0x0FFFFFF8)
    fat[4:8] = le32(0x0FFFFFFF)
    fat[8:12] = le32(0x0FFFFFFF)   # root directory cluster
    for i in range(nfats):
        off = (fat_start + i * spf) * BS
        buf[off:off+len(fat)] = fat

    open(out, "wb").write(buf)
    print("OK %s mb=%d part_start=%d part_sec=%d spc=%d spf=%d data_start=%d clusters=%d"
          % (out, mb, PART_START, part_sec, spc, spf, data_start, clusters))

main()
