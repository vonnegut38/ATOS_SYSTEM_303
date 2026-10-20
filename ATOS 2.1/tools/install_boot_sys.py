#!/usr/bin/env python3
"""Install BOOT.SYS into SYSTEM on the first active AHDI FAT16 partition."""
import argparse
import pathlib
import struct

def be32(b, o): return struct.unpack_from(">I", b, o)[0]
def le16(b, o): return struct.unpack_from("<H", b, o)[0]

ap = argparse.ArgumentParser()
ap.add_argument("image", type=pathlib.Path)
ap.add_argument("boot_sys", type=pathlib.Path)
ap.add_argument("--root", action="store_true",
                help="install an ordinary 8.3 file in the partition root")
ap.add_argument("--directory", default="SYSTEM",
                help="destination directory below the root (default: SYSTEM)")
ap.add_argument("--data", action="store_true", help="install an ordinary 8.3 data file in the selected directory")
ap.add_argument("--if-missing", action="store_true", help="preserve an existing file (user settings)")
args = ap.parse_args()
disk = args.image.open("r+b")
def read_at(offset, size): disk.seek(offset); return disk.read(size)
def write_at(offset, data): disk.seek(offset); disk.write(data)
payload = args.boot_sys.read_bytes()
if payload[:8] == b"ASYSBOOT": target_name=b"BOOT    SYS"
elif payload[:8] == b"ASYSKERN": target_name=b"KERNEL  SYS"
elif payload[:8] in (b"ASYSMOD0",b"ASYSAPP0"):
    filename=args.boot_sys.name.upper()
    parts=filename.split(".")
    if len(parts)==1 and parts[0] and len(parts[0])<=8:
        parts.append("")
    if len(parts)!=2 or not parts[0] or len(parts[0])>8 or len(parts[1])>3:
        raise SystemExit("module filename must use an 8.3 name")
    try: target_name=(parts[0].ljust(8)+parts[1].ljust(3)).encode("ascii")
    except UnicodeEncodeError: raise SystemExit("module filename must be ASCII")
elif args.root or args.data or args.directory != "SYSTEM":
    # An ordinary data file (e.g. a TOS .PRG) is also installed when an explicit
    # --directory is given; previously this fell through to the error below.
    filename=args.boot_sys.name.upper(); parts=filename.split(".")
    if len(parts)!=2 or not parts[0] or len(parts[0])>8 or len(parts[1])>3:
        raise SystemExit("root filename must use an 8.3 name")
    try: target_name=(parts[0].ljust(8)+parts[1].ljust(3)).encode("ascii")
    except UnicodeEncodeError: raise SystemExit("root filename must be ASCII")
else: raise SystemExit("input is not a recognized Atari System image")

root = read_at(0,512)
parts = []
for n in range(4):
    o = 0x1C6 + n * 12
    if root[o] & 1: parts.append((be32(root, o + 4), root[o+1:o+4]))
if not parts: raise SystemExit("no active AHDI partition")
part, pid = parts[0]
base = part * 512
bpb=read_at(base,512)
bps = le16(bpb,11); spc = bpb[13]; reserved = le16(bpb,14)
fats = bpb[16]; roots = le16(bpb,17); spf = le16(bpb,22)
if bps < 512 or bps > 8192 or bps & (bps-1) or not all((spc,reserved,fats,roots,spf)):
    raise SystemExit("unsupported FAT16 BPB")
fat0 = base + reserved*bps
root_off = fat0 + fats*spf*bps
root_bytes = ((roots*32+bps-1)//bps)*bps
data_off = root_off + root_bytes
cluster_bytes = spc*bps
max_clusters=(spf*bps)//2

def fat_get(c): return le16(read_at(fat0+c*2,2),0)
def fat_set(c, value):
    value_bytes=struct.pack("<H",value)
    for f in range(fats): write_at(fat0+f*spf*bps+c*2,value_bytes)
def cluster_off(c): return data_off+(c-2)*cluster_bytes
def chain(first):
    seen=set(); c=first
    while 2 <= c < 0xFFF8:
        if c in seen: raise SystemExit("FAT loop detected")
        seen.add(c); yield c; c=fat_get(c)

def allocate_directory(parent, name):
    try: new_cluster=next(c for c in range(2,max_clusters) if fat_get(c)==0)
    except StopIteration: raise SystemExit("not enough free FAT clusters")
    fat_set(new_cluster,0xFFFF)
    block=bytearray(cluster_bytes)
    block[0:11]=b".          "; block[11]=0x10
    struct.pack_into("<H",block,26,new_cluster)
    block[32:43]=b"..         "; block[43]=0x10
    struct.pack_into("<H",block,58,parent)
    write_at(cluster_off(new_cluster),block)
    wanted=name.ljust(11).encode("ascii")
    for c in chain(parent):
        co=cluster_off(c)
        for o in range(co,co+cluster_bytes,32):
            e=read_at(o,32)
            if e[0] in (0,0xE5):
                entry=bytearray(32);entry[:11]=wanted;entry[11]=0x10
                struct.pack_into("<H",entry,26,new_cluster);write_at(o,entry)
                return new_cluster
    raise SystemExit(f"directory {name} has no free parent entry")

def find_subdirectory(parent, name):
    wanted=name.ljust(11).encode("ascii")
    for c in chain(parent):
        co=cluster_off(c)
        for o in range(co,co+cluster_bytes,32):
            e=read_at(o,32)
            if e[0] == 0: return 0
            if e[0] != 0xE5 and e[:11] == wanted and e[11]&0x10: return le16(e,26)
    return 0

system_cluster = 0
root_slot = None
for o in range(root_off, root_off+roots*32, 32):
    e=read_at(o,32)
    if e[0] in (0,0xE5):
        if root_slot is None: root_slot=o
        continue
    if e[:11] == b"SYSTEM     " and e[11] & 0x10: system_cluster=le16(e,26); break
if system_cluster < 2:
    if root_slot is None: raise SystemExit("root directory has no free entry")
    try: system_cluster=next(c for c in range(2,max_clusters) if fat_get(c)==0)
    except StopIteration: raise SystemExit("not enough free FAT clusters")
    fat_set(system_cluster,0xFFFF)
    write_at(cluster_off(system_cluster),b"\0"*cluster_bytes)
    entry=bytearray(32);entry[:11]=b"SYSTEM     ";entry[11]=0x10
    struct.pack_into("<H",entry,26,system_cluster);write_at(root_slot,entry)

def find_root_subdirectory(name):
    wanted=name.ljust(11).encode("ascii")
    for o in range(root_off, root_off+root_bytes, 32):
        e=read_at(o,32)
        if e[0]==0: return 0
        if e[0]!=0xE5 and e[:11]==wanted and e[11]&0x10: return le16(e,26)
    return 0

def allocate_root_subdirectory(name):
    try: new_cluster=next(c for c in range(2,max_clusters) if fat_get(c)==0)
    except StopIteration: raise SystemExit("not enough free FAT clusters")
    fat_set(new_cluster,0xFFFF)
    block=bytearray(cluster_bytes)
    block[0:11]=b".          "; block[11]=0x10
    struct.pack_into("<H",block,26,new_cluster)
    block[32:43]=b"..         "; block[43]=0x10
    struct.pack_into("<H",block,58,0)
    write_at(cluster_off(new_cluster),block)
    wanted=name.ljust(11).encode("ascii")
    for o in range(root_off, root_off+root_bytes, 32):
        e=read_at(o,32)
        if e[0] in (0,0xE5):
            entry=bytearray(32);entry[:11]=wanted;entry[11]=0x10
            struct.pack_into("<H",entry,26,new_cluster);write_at(o,entry)
            return new_cluster
    raise SystemExit("root directory has no free entry")

target_directory=0
directory_label=""
if not args.root:
    components=[part.upper() for part in args.directory.replace("\\","/").split("/") if part]
    if not components:
        raise SystemExit("empty destination directory")
    if components[0]=="SYSTEM":
        target_directory=system_cluster;directory_label="SYSTEM"
    else:
        target_directory=find_root_subdirectory(components[0])
        if target_directory<2: target_directory=allocate_root_subdirectory(components[0])
        directory_label=components[0]
    for component in components[1:]:
        if len(component)>8 or not component.isascii():
            raise SystemExit("directory components must use ASCII 8.3 names")
        child=find_subdirectory(target_directory,component)
        if child<2: child=allocate_directory(target_directory,component)
        target_directory=child;directory_label += "/"+component

slot = None; old_first = 0
if args.root:
    # Root: fixed size, no clustering.  Scan stops at the first 0x00 (anything
    # beyond it is not part of the directory for a FAT reader, so a file written
    # there is invisible even though the script reports success).
    free_slot = None; terminator = None
    for o in range(root_off,root_off+roots*32,32):
        e=read_at(o,32)
        if e[:11] == target_name: slot=o; old_first=le16(e,26); break
        if e[0] == 0xE5 and free_slot is None: free_slot=o
        if e[0] == 0x00: terminator=o; break
    if slot is None: slot = free_slot if free_slot is not None else terminator
    if slot is None: raise SystemExit("root directory has no free entry")
    if terminator is not None and slot == terminator and slot+32 < root_off+root_bytes:
        write_at(slot+32, b"\0"*32)
else:
    free_slot = None; terminator = None
    for c in chain(target_directory):
        co=cluster_off(c)
        for o in range(co,co+cluster_bytes,32):
            e=read_at(o,32)
            if e[:11] == target_name: slot=o; old_first=le16(e,26); break
            if e[0] == 0xE5 and free_slot is None: free_slot=o
            if e[0] == 0x00: terminator=(o,co,c); break
        if old_first or terminator: break
    if slot is None: slot = free_slot          # reuse a deleted slot first
    if slot is None and terminator is not None:
        o, co, c = terminator
        slot = o                               # the entry goes INTO the end slot
        if o + 32 < co + cluster_bytes:
            write_at(o+32, b"\0"*32)          # terminator follows in place
        else:
            # Last slot of this cluster: the directory continues in the NEXT
            # cluster.  Reuse the existing link when there is one, otherwise add
            # one - and put the new terminator at the start of that cluster.
            # (Writing the file into a fresh cluster while the old slot stayed
            # 0x00 is exactly the "installed but invisible" bug.)
            nxt = fat_get(c)
            if not (2 <= nxt < 0xFFF8):
                try: nxt=next(cc for cc in range(2,max_clusters) if fat_get(cc)==0)
                except StopIteration: raise SystemExit("not enough free FAT clusters")
                fat_set(c,nxt)
            fat_set(nxt,0xFFFF)
            write_at(cluster_off(nxt), b"\0"*cluster_bytes)
    if slot is None:
        # Directory chain completely full (no 0x00 anywhere): link one more
        # cluster and put the entry at its start, with a terminator after it.
        last=None
        for c in chain(target_directory): last=c
        try: newc=next(cc for cc in range(2,max_clusters) if fat_get(cc)==0)
        except StopIteration: raise SystemExit("not enough free FAT clusters")
        fat_set(last,newc); fat_set(newc,0xFFFF)
        write_at(cluster_off(newc), b"\0"*cluster_bytes)
        slot=cluster_off(newc)
        write_at(slot+32, b"\0"*32)
    if slot is None: raise SystemExit("directory has no free entry")
if args.if_missing and read_at(slot,11)==target_name:
    disk.close()
    print(f"preserved existing {args.boot_sys.name}")
    raise SystemExit(0)
if old_first:
    for c in list(chain(old_first)): fat_set(c,0)

needed=(len(payload)+cluster_bytes-1)//cluster_bytes
free=[c for c in range(2,max_clusters) if fat_get(c)==0][:needed]
if len(free)!=needed: raise SystemExit("not enough free FAT clusters")
for n,c in enumerate(free):
    fat_set(c, free[n+1] if n+1<len(free) else 0xFFFF)
    chunk=payload[n*cluster_bytes:(n+1)*cluster_bytes]
    write_at(cluster_off(c),chunk+b"\0"*(cluster_bytes-len(chunk)))
entry=bytearray(32);entry[:11]=target_name;entry[11]=0x20
struct.pack_into("<H",entry,26,free[0]);struct.pack_into("<I",entry,28,len(payload))
write_at(slot,entry)
disk.flush();disk.close()
location=args.boot_sys.name if args.root else f"{directory_label}/{args.boot_sys.name}"
print(f"installed {len(payload)} bytes as {location} on {pid.decode('ascii',errors='replace')} partition at sector {part}")
