#!/usr/bin/env python3
r"""Build a 720 KB boot floppy with a real 12-bit FAT12 (714 clusters).  \SYSTEM\
holds BOOT.SYS, KERNEL.SYS, the modules and the startup apps, plus \SYSTEM\APP
with the tools the kernel PATH looks for.  The BIOS loader, the kernel FAT layer
and the ROM's hw/fat12.c reader all pick FAT12 from the cluster count, so this is
also readable by the "fdd" command."""
import pathlib, struct, sys

FILES = ["BOOT.SYS","KERNEL.SYS","ACSI.SYS","CORE.SYS","VFS.SYS","PROC.SYS",
         "TIMER.SYS","EVENT.SYS","INPUT.SYS","SHELL.SYS","VOFA.SYS",
         "INIT.APP","GFX.APP","GUI.APP"]
APPS = ["COMMAND.APP","SYSINFO.APP","HW.APP"]
SECTORS=1440; BPS=512; SPC=2; RESERVED=1; FATS=1; ROOT_ENTRIES=112; SPF=3; SPT=9; HEADS=2

def name83(n):
    base,_,ext=n.upper().partition(".")
    return (base[:8].ljust(8)+ext[:3].ljust(3)).encode("ascii")

def build(src,out):
    data=bytearray(SECTORS*BPS)
    fat_off=RESERVED*BPS
    root_off=(RESERVED+FATS*SPF)*BPS
    root_sectors=(ROOT_ENTRIES*32+BPS-1)//BPS
    data_off=root_off+root_sectors*BPS
    cluster_bytes=SPC*BPS
    def coff(c): return data_off+(c-2)*cluster_bytes
    def fat_set(c,v):
        # 12-bit FAT: 1.5 bytes/entry, little-endian nibble packing.
        off=fat_off+c+(c>>1)
        if c&1:
            data[off]=(data[off]&0x0F)|((v<<4)&0xF0)
            data[off+1]=(v>>4)&0xFF
        else:
            data[off]=v&0xFF
            data[off+1]=(data[off+1]&0xF0)|((v>>8)&0x0F)
    state={"next":3}
    def alloc(payload):
        if not payload: return 0
        n=(len(payload)+cluster_bytes-1)//cluster_bytes
        first=state["next"]
        for i in range(n):
            c=state["next"]; state["next"]+=1
            fat_set(c, 0xFFF if i==n-1 else c+1)
            data[coff(c):coff(c)+len(payload[i*cluster_bytes:(i+1)*cluster_bytes])]=payload[i*cluster_bytes:(i+1)*cluster_bytes]
        return first
    def entry(off,nm,attr,cluster,size):
        e=bytearray(32); e[0:11]=nm; e[11]=attr
        struct.pack_into("<H",e,26,cluster); struct.pack_into("<I",e,28,size)
        data[off:off+32]=e
    fat_set(0,0xFF8); fat_set(1,0xFFF); fat_set(2,0xFFF)
    entry(root_off, name83("SYSTEM"), 0x10, 2, 0)
    # reserve one cluster for the \SYSTEM\APP directory, then fill it after the
    # payloads so alloc() cannot overwrite its entry table
    appc=state["next"]; state["next"]+=1; fat_set(appc,0xFFFF)
    system_items=[(b".          ",0x10,2,0),(b"..         ",0x10,0,0),
                  (name83("APP"),0x10,appc,0)]
    for n in FILES:
        p=pathlib.Path(src)/n
        if not p.exists(): raise SystemExit("missing "+str(p))
        payload=p.read_bytes()
        system_items.append((name83(n),0x20,alloc(payload),len(payload)))
    app_items=[(b".          ",0x10,appc,0),(b"..         ",0x10,2,0)]
    for n in APPS:
        p=pathlib.Path(src)/n
        if not p.exists(): raise SystemExit("missing "+str(p))
        payload=p.read_bytes()
        app_items.append((name83(n),0x20,alloc(payload),len(payload)))
    for i,(nm,attr,c,size) in enumerate(system_items):
        entry(coff(2)+i*32, nm, attr, c, size)
    for i,(nm,attr,c,size) in enumerate(app_items):
        entry(coff(appc)+i*32, nm, attr, c, size)
    bs=bytearray(512)
    bs[0:3]=b"\xEB\x3C\x90"; bs[3:11]=b"ATOSBOOT"
    struct.pack_into("<H",bs,11,BPS); bs[13]=SPC
    struct.pack_into("<H",bs,14,RESERVED); bs[16]=FATS
    struct.pack_into("<H",bs,17,ROOT_ENTRIES)
    struct.pack_into("<H",bs,19,SECTORS); bs[21]=0xF8
    struct.pack_into("<H",bs,22,SPF); struct.pack_into("<H",bs,24,SPT); struct.pack_into("<H",bs,26,HEADS)
    bs[38]=0x29; bs[43:54]=b"ATOS BOOT  "; bs[54:62]=b"FAT12   "
    bs[510]=0x55; bs[511]=0xAA
    data[0:512]=bs
    pathlib.Path(out).write_bytes(data)
    print("wrote",out,"clusters used",state["next"]-2,"of ~714")

if __name__=="__main__":
    build(sys.argv[1] if len(sys.argv)>1 else "build/system",
          sys.argv[2] if len(sys.argv)>2 else "/tmp/atos-boot.st")