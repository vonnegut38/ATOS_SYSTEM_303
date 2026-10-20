#!/usr/bin/env python3
"""Print a file from an ATOS AHDI FAT16 image: img_cat.py <image> <\\PATH\\FILE.EXT>"""
import struct, sys
def le16(b,o): return struct.unpack_from("<H",b,o)[0]
def be32(b,o): return struct.unpack_from(">I",b,o)[0]
def main(image,path):
    d=open(image,"rb")
    def rd(o,s): d.seek(o); return d.read(s)
    root=rd(0,512)
    parts=[(be32(root,0x1C6+n*12+4),root[0x1C7+n*12:0x1CA+n*12]) for n in range(4) if root[0x1C6+n*12]&1]
    part,_=parts[0]; base=part*512; bpb=rd(base,512)
    bps=le16(bpb,11); spc=bpb[13]; res=le16(bpb,14); fats=bpb[16]; roots=le16(bpb,17); spf=le16(bpb,22)
    fat0=base+res*bps; root_off=fat0+fats*spf*bps
    root_bytes=((roots*32+bps-1)//bps)*bps; data_off=root_off+root_bytes; cb=spc*bps
    def coff(c): return data_off+(c-2)*cb
    def fg(c): return le16(rd(fat0+c*2,2),0)
    def chain(f):
        c=f; seen=set()
        while 2<=c<0xFFF8 and c not in seen:
            seen.add(c); yield c; c=fg(c)
    comps=[p.upper() for p in path.replace("\\","/").split("/") if p]
    cur=0
    for comp in comps:
        want=comp if len(comp)==11 else None
        fname,_,fext=comp.partition(".")
        w=(want.encode() if want else (fname.ljust(8)+fext.ljust(3)).encode()[:11])
        found=0; ent=None
        rng=[(root_off,roots)] if cur==0 else [(coff(c),cb//32) for c in chain(cur)]
        for off,cnt in rng:
            for i in range(cnt):
                o=off+i*32; e=rd(o,32)
                if e[0]==0: break
                if e[0]!=0xE5 and e[:11]==w:
                    ent=e; found=le16(e,26); break
            if found: break
        if not found: raise SystemExit("not found: "+comp)
        cur=found
    size=le16(ent,28)|(le16(ent,30)<<16)
    sys.stderr.write("size=%d first_cluster=%d\n"%(size,cur))
    out=bytearray()
    for c in chain(cur):
        out+=rd(coff(c),cb)
        if len(out)>=size: break
    sys.stdout.write(bytes(out[:size]).decode("latin1"))
main(sys.argv[1], sys.argv[2])
