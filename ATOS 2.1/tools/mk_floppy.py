#!/usr/bin/env python3
"""Create a 720 KB FAT12 floppy image with TEST.TXT (for the floppy reader)."""
import pathlib, struct, sys

PAYLOAD = b"HELLO FROM FLOPPY\n"

def main(out):
    data = bytearray(80*2*9*512)
    bps,spc,reserved,fats,root_entries,total,media,spf,spt,heads = 512,1,1,1,224,1440,0xF0,9,9,2
    bs = bytearray(512)
    bs[0:3] = b"\xEB\x3C\x90"
    bs[3:11] = b"ATOSFLPY"
    struct.pack_into("<H",bs,11,bps)
    bs[13] = spc
    struct.pack_into("<H",bs,14,reserved)
    bs[16] = fats
    struct.pack_into("<H",bs,17,root_entries)
    struct.pack_into("<H",bs,19,total)
    bs[21] = media
    struct.pack_into("<H",bs,22,spf)
    struct.pack_into("<H",bs,24,spt)
    struct.pack_into("<H",bs,26,heads)
    bs[38] = 0x29
    bs[43:54] = b"ATOS FLOPPY"
    bs[54:62] = b"FAT12   "
    bs[510] = 0x55; bs[511] = 0xAA
    data[0:512] = bs
    fat_off = reserved*bps
    data[fat_off] = media; data[fat_off+1] = 0xFF; data[fat_off+2] = 0xFF
    data[fat_off+3] = 0xFF; data[fat_off+4] = 0x0F   # cluster 2 -> 0xFFF
    root_off = (reserved+fats*spf)*bps
    e = bytearray(32); e[0:11] = b"TEST    TXT"; e[11] = 0x20
    struct.pack_into("<H",e,26,2); struct.pack_into("<I",e,28,len(PAYLOAD))
    data[root_off:root_off+32] = e
    data_off = (reserved+fats*spf+(root_entries*32+bps-1)//bps)*bps
    data[data_off:data_off+len(PAYLOAD)] = PAYLOAD
    pathlib.Path(out).write_bytes(data)
    print("wrote",out,len(data),"TEST.TXT",len(PAYLOAD),"bytes")

if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv)>1 else "/tmp/atos-floppy.st")
