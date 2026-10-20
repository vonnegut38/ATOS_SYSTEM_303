#!/usr/bin/env python3
"""Read the on-disk BIOS.CFG files from an ATOS AHDI FAT16 image."""
import struct, sys

def le16(b,o): return struct.unpack_from("<H",b,o)[0]
def be32(b,o): return struct.unpack_from(">I",b,o)[0]

def crc(b):
    s=0
    for i in range(len(b)):
        if 8<=i<12: continue
        s=((s<<1)|(s>>31))&0xffffffff; s^=b[i]
    return s

def main(path):
    d=open(path,"rb")
    def rd(off,size): d.seek(off); return d.read(size)
    root=rd(0,512)
    parts=[]
    for n in range(4):
        o=0x1C6+n*12
        if root[o]&1: parts.append((be32(root,o+4),root[o+1:o+4]))
    part,pid=parts[0]
    base=part*512
    bpb=rd(base,512)
    bps=le16(bpb,11); spc=bpb[13]; reserved=le16(bpb,14); fats=bpb[16]; roots=le16(bpb,17); spf=le16(bpb,22)
    fat0=base+reserved*bps
    root_off=fat0+fats*spf*bps
    root_bytes=((roots*32+bps-1)//bps)*bps
    data_off=root_off+root_bytes
    cluster_bytes=spc*bps
    def cluster_off(c): return data_off+(c-2)*cluster_bytes
    def fat_get(c): return le16(rd(fat0+c*2,2),0)
    def read_chain(first,size):
        out=b""; c=first; seen=set()
        while 2<=c<0xFFF8 and len(out)<size:
            if c in seen: break
            seen.add(c); out+=rd(cluster_off(c),cluster_bytes); c=fat_get(c)
        return out[:size]
    def entries(off,count):
        for i in range(count):
            yield off+i*32, rd(off+i*32,32)
    print("partition", part, pid)
    bios_cluster=0
    for o,e in entries(root_off,roots):
        if e[0]==0: break
        if e[0]==0xE5: continue
        name=e[:11].decode("ascii","replace")
        if name=="BIOS       " and e[11]&0x10: bios_cluster=le16(e,26)
        if name=="BIOS    CFG":
            size=struct.unpack_from("<I",e,28)[0]
            b=read_chain(le16(e,26),size)
            report("root \\BIOS.CFG",b)
    if bios_cluster>=2:
        for o,e in entries(cluster_off(bios_cluster),cluster_bytes//32):
            if e[0]==0: break
            if e[0]==0xE5: continue
            if e[:11]==b"BIOS    CFG":
                size=struct.unpack_from("<I",e,28)[0]
                report("\\BIOS\\BIOS.CFG",read_chain(le16(e,26),size))
    else:
        print("no \\BIOS directory")

def report(label,b):
    if len(b)<28:
        print(label,"size",len(b),"TOO SHORT",b.hex()); return
    magic=struct.unpack_from(">I",b,0)[0]
    ver=struct.unpack_from(">H",b,4)[0]; size=struct.unpack_from(">H",b,6)[0]
    stored=struct.unpack_from(">I",b,8)[0]
    print(label,"len",len(b),"magic",hex(magic),"ver",ver,"hdr_size",size,
          "auto_tos",b[26] if len(b)>26 else "n/a",
          "boot_valid",b[25] if len(b)>25 else "n/a",
          "boot_id",hex(b[23]) if len(b)>23 else "n/a",
          "boot_part",b[24] if len(b)>24 else "n/a",
          "video_pending",b[27] if len(b)>27 else "n/a",
          "crc_ok",crc(b)==stored,"stored",hex(stored),"calc",hex(crc(b)))

if __name__=="__main__":
    main(sys.argv[1] if len(sys.argv)>1 else "/Users/xxx/Documents/Deepseek_test/ATOS 2.0/work/atos-v2.img")
