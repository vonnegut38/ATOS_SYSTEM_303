#!/usr/bin/env python3
r"""Generate an ATOS BIOS.CFG (v2, 128 B, CRC, big-endian/m68k)."""
import argparse, struct, pathlib

def crc(b):
    s=0
    for i in range(len(b)):
        if 8<=i<12: continue
        s=((s<<1)|(s>>31))&0xffffffff; s^=b[i]
    return s

ap=argparse.ArgumentParser()
ap.add_argument("out",type=pathlib.Path)
ap.add_argument("--boot-device",type=int,default=1)   # 1 = ACSI
ap.add_argument("--auto-tos",type=int,default=0)
ap.add_argument("--boot-delay",type=int,default=4)
ap.add_argument("--language",default="EN")
a=ap.parse_args()
buf=bytearray(128)
struct.pack_into(">I",buf,0,0x41534643)
struct.pack_into(">H",buf,4,2)
struct.pack_into(">H",buf,6,128)
buf[12]=a.boot_device; buf[14]=2; buf[21]=1; buf[22]=a.boot_delay
buf[25]=1; buf[26]=a.auto_tos
buf[17:21]=(a.language+"\0\0\0\0").encode("ascii")[:4]
struct.pack_into(">I",buf,8,crc(bytes(buf)))
a.out.write_bytes(buf)
print("wrote",a.out,"auto_tos",a.auto_tos,"crc",hex(struct.unpack_from(">I",buf,8)[0]))
