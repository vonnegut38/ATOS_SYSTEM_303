#!/usr/bin/env python3
"""Extract a file from an ATOS AHDI FAT16 image (host-side, 8.3 path)."""
import hashlib, struct, sys, pathlib

def le16(b,o): return struct.unpack_from("<H",b,o)[0]
def be32(b,o): return struct.unpack_from(">I",b,o)[0]

def main(image, path, out):
    d=open(image,"rb")
    def rd(off,size): d.seek(off); return d.read(size)
    root=rd(0,512)
    parts=[]
    for n in range(4):
        o=0x1C6+n*12
        if root[o]&1: parts.append((be32(root,o+4),root[o+1:o+4]))
    part,pid=parts[0]; base=part*512
    bpb=rd(base,512)
    bps=le16(bpb,11); spc=bpb[13]; reserved=le16(bpb,14); fats=bpb[16]; roots=le16(bpb,17); spf=le16(bpb,22)
    fat0=base+reserved*bps
    root_off=fat0+fats*spf*bps
    root_bytes=((roots*32+bps-1)//bps)*bps
    data_off=root_off+root_bytes
    cluster_bytes=spc*bps
    def cluster_off(c): return data_off+(c-2)*cluster_bytes
    def fat_get(c): return le16(rd(fat0+c*2,2),0)
    def chain(first):
        c=first; seen=set()
        while 2<=c<0xFFF8 and c not in seen:
            seen.add(c); yield c; c=fat_get(c)
    def read_file(first,size):
        out=b""
        for c in chain(first): out+=rd(cluster_off(c),cluster_bytes)
        return out[:size]
    comps=[p.upper() for p in path.replace("\\","/").split("/") if p]
    name=comps[-1]; parts2=name.split(".")
    if len(parts2)!=2: raise SystemExit("need 8.3 filename")
    wanted=(parts2[0].ljust(8)+parts2[1].ljust(3)).encode("ascii")
    cur_dir=0
    # resolve directories
    for comp in comps[:-1]:
        wname=comp.ljust(11).encode("ascii")[:11]
        def scan(first):
            if first==0: off,count=root_off,roots
            else: off,count=cluster_off(first),cluster_bytes//32
            # for chained dirs, iterate
            return off,count
        found=0
        if cur_dir==0:
            for o in range(root_off,root_off+root_bytes,32):
                e=rd(o,32)
                if e[0]==0: break
                if e[0]!=0xE5 and e[:11]==wname and e[11]&0x10: found=le16(e,26); break
        else:
            for c in chain(cur_dir):
                co=cluster_off(c)
                for o in range(co,co+cluster_bytes,32):
                    e=rd(o,32)
                    if e[0]==0: break
                    if e[0]!=0xE5 and e[:11]==wname and e[11]&0x10: found=le16(e,26); break
                if found: break
        if not found: raise SystemExit("dir not found: "+comp)
        cur_dir=found
    # find file entry
    found=0; size=0
    if cur_dir==0:
        for o in range(root_off,root_off+root_bytes,32):
            e=rd(o,32)
            if e[0]==0: break
            if e[0]!=0xE5 and e[:11]==wanted: found=le16(e,26); size=struct.unpack_from("<I",e,28)[0]; break
    else:
        for c in chain(cur_dir):
            co=cluster_off(c)
            for o in range(co,co+cluster_bytes,32):
                e=rd(o,32)
                if e[0]==0: break
                if e[0]!=0xE5 and e[:11]==wanted: found=le16(e,26); size=struct.unpack_from("<I",e,28)[0]; break
            if found: break
    if not found: raise SystemExit("file not found: "+path)
    data=read_file(found,size)
    out=pathlib.Path(out); out.write_bytes(data)
    print("extracted",path,"size",size,"md5",hashlib.md5(data).hexdigest())

if __name__=="__main__":
    main(sys.argv[1],sys.argv[2],sys.argv[3])
