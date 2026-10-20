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
ap.add_argument("--boot-device",type=int,default=1)   # 1 = ACSI, 2 = IDE
ap.add_argument("--boot-id",type=int,default=-1)      # -1 = auto-detect; 0x80 = IDE unit 0
ap.add_argument("--boot-part",type=int,default=0)
ap.add_argument("--auto-tos",type=int,default=0)
ap.add_argument("--boot-delay",type=int,default=4)
ap.add_argument("--video-pending",type=int,default=0)  # 1 = card boot unconfirmed (failover test)
ap.add_argument("--cpu-clock",type=int,default=0)   # nominal MHz for machines with no readable clock (0 = unknown)
ap.add_argument("--language",default="EN")
ap.add_argument("--boot-order",default="")   # e.g. "0,0x80,0xfe"; 0xff fills the rest
ap.add_argument("--video-driver",type=int,default=0)  # 0 auto, 1 shifter, 2 et4000/vofa
ap.add_argument("--video-mode",type=int,default=2)    # 0 low, 1 medium, 2 high(mono), 3 vga8 800x600x256
a=ap.parse_args()
buf=bytearray(128)
struct.pack_into(">I",buf,0,0x41534643)
struct.pack_into(">H",buf,4,2)
struct.pack_into(">H",buf,6,128)
buf[12]=a.boot_device; buf[13]=a.video_driver&0xff; buf[14]=a.video_mode&0xff
buf[21]=1; buf[22]=a.boot_delay
# Only claim a remembered selection when one is actually named; otherwise the
# ROM must auto-detect (the shipped default pointed at non-existent ACSI 0).
buf[23]=(a.boot_id&0xff) if a.boot_id>=0 else 0
buf[24]=a.boot_part
buf[25]=1 if a.boot_id>=0 else 0
buf[26]=a.auto_tos; buf[27]=a.video_pending; buf[28]=a.cpu_clock&0xff
buf[17:21]=(a.language+"\0\0\0\0").encode("ascii")[:4]
order=[int(x,0)&0xff for x in a.boot_order.split(",") if x.strip()][:4]
for i,v in enumerate(order): buf[29+i]=v
buf[33]=1 if order else 0
struct.pack_into(">I",buf,8,crc(bytes(buf)))
a.out.write_bytes(buf)
print("wrote",a.out,"auto_tos",a.auto_tos,"boot_valid",buf[25],
      "boot_id",hex(buf[23]),"cpu_clock",buf[28],"crc",hex(struct.unpack_from(">I",buf,8)[0]))