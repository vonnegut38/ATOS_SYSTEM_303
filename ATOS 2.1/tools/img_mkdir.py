#!/usr/bin/env python3
"""Create one 8.3 directory in the root of an ATOS AHDI FAT16 image."""
import struct, sys
def le16(b,o): return struct.unpack_from("<H",b,o)[0]
def be32(b,o): return struct.unpack_from(">I",b,o)[0]
def main(image,name):
    f=open(image,"r+b")
    rd=lambda o,n:(f.seek(o),f.read(n))[1]
    wr=lambda o,b:(f.seek(o),f.write(b))
    root=rd(0,512)
    base=be32(root,0x1C6+4)*512
    bpb=rd(base,512)
    bps=le16(bpb,11);spc=bpb[13];res=le16(bpb,14);fats=bpb[16];roots=le16(bpb,17);spf=le16(bpb,22) or struct.unpack_from("<I",bpb,36)[0]
    fat_off=base+res*bps;root_off=base+(res+fats*spf)*bps
    root_bytes=((roots*32+bps-1)//bps)*bps
    data_off=root_off+root_bytes;cb=spc*bps
    fat=rd(fat_off,spf*bps)
    nent=len(fat)//2
    free=0
    for i in range(2,nent):
        if le16(fat,i*2)==0: free=i;break
    if not free: raise SystemExit("no free cluster")
    # free root slot
    raw=rd(root_off,root_bytes);slot=-1
    for i in range(0,len(raw),32):
        if raw[i]==0 or raw[i]==0xE5: slot=i;break
    if slot<0: raise SystemExit("no root slot")
    # FAT: EOC in every copy
    eoc=struct.pack("<H",0xffff)
    for k in range(fats):
        fo=fat_off+k*spf*bps
        wr(fo+free*2,eoc)
    # zero cluster
    wr(data_off+(free-2)*cb,b"\x00"*cb)
    nm=name.upper().ljust(8)[:8].encode("latin1")
    def entry(n11,attr,clus):
        e=bytearray(32);e[0:11]=n11;e[11]=attr
        struct.pack_into("<H",e,26,clus);return bytes(e)
    dot=entry(b".          ",0x10,free)
    dotdot=entry(b"..         ",0x10,0)
    wr(data_off+(free-2)*cb,dot+dotdot)
    rootent=entry(nm+b"   ",0x10,free)
    wr(root_off+slot,rootent)
    f.flush();f.close()
    print("mkdir",name,"cluster",free)
if __name__=="__main__": main(sys.argv[1],sys.argv[2])
