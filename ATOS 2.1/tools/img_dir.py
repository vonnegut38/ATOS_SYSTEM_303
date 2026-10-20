#!/usr/bin/env python3
"""List an ATOS AHDI FAT16 directory (default root)."""
import struct, sys
def le16(b,o): return struct.unpack_from("<H",b,o)[0]
def be32(b,o): return struct.unpack_from(">I",b,o)[0]
def main(image, path="\\"):
    d=open(image,"rb")
    def rd(o,s): d.seek(o); return d.read(s)
    root=rd(0,512)
    parts=[(be32(root,0x1C6+n*12+4), root[0x1C7+n*12:0x1CA+n*12]) for n in range(4) if root[0x1C6+n*12]&1]
    part,pid=parts[0]; base=part*512
    bpb=rd(base,512)
    bps=le16(bpb,11); spc=bpb[13]; reserved=le16(bpb,14); fats=bpb[16]; roots=le16(bpb,17); spf=le16(bpb,22)
    fat0=base+reserved*bps
    root_off=fat0+fats*spf*bps
    root_bytes=((roots*32+bps-1)//bps)*bps
    data_off=root_off+root_bytes
    cb=spc*bps
    def coff(c): return data_off+(c-2)*cb
    def fg(c): return le16(rd(fat0+c*2,2),0)
    def chain(f):
        c=f; seen=set()
        while 2<=c<0xFFF8 and c not in seen:
            seen.add(c); yield c; c=fg(c)
    comps=[p.upper() for p in path.replace("\\","/").split("/") if p]
    cur=0
    for comp in comps:
        wanted=comp.ljust(11).encode()[:11]; found=0
        if cur==0:
            for o in range(root_off,root_off+root_bytes,32):
                e=rd(o,32)
                if e[0]==0: break
                if e[0]!=0xE5 and e[:11]==wanted and e[11]&0x10: found=le16(e,26); break
        else:
            for c in chain(cur):
                for o in range(coff(c),coff(c)+cb,32):
                    e=rd(o,32)
                    if e[0]==0: break
                    if e[0]!=0xE5 and e[:11]==wanted and e[11]&0x10: found=le16(e,26); break
                if found: break
        if not found: raise SystemExit("not found: "+comp)
        cur=found
    names=[]
    def scan(off,count):
        for i in range(count):
            e=rd(off+i*32,32)
            if e[0]==0: return
            if e[0]==0xE5: continue
            nm="".join(chr(c) for c in e[:8]).strip()
            ex="".join(chr(c) for c in e[8:11]).strip()
            names.append(nm+("."+ex if ex else ""))
    if cur==0: scan(root_off,roots)
    else:
        for c in chain(cur): scan(coff(c),cb//32)
    print("\n".join(names))
if __name__=="__main__":
    main(sys.argv[1], sys.argv[2] if len(sys.argv)>2 else "\\")
