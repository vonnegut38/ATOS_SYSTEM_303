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
elif args.root:
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

target_directory=0
directory_label=""
if not args.root:
    components=[part.upper() for part in args.directory.replace("\\","/").split("/") if part]
    if not components or components[0]!="SYSTEM":
        raise SystemExit("destination directory must be SYSTEM or a subdirectory of SYSTEM")
    target_directory=system_cluster;directory_label="SYSTEM"
    for component in components[1:]:
        if len(component)>8 or not component.isascii():
            raise SystemExit("directory components must use ASCII 8.3 names")
        child=find_subdirectory(target_directory,component)
        if child<2: child=allocate_directory(target_directory,component)
        target_directory=child;directory_label += "/"+component

slot = None; old_first = 0
if args.root:
    for o in range(root_off,root_off+roots*32,32):
        e=read_at(o,32)
        if e[:11] == target_name: slot=o; old_first=le16(e,26); break
        if slot is None and e[0] in (0,0xE5): slot=o
    if slot is None: raise SystemExit("root directory has no free entry")
else:
    for c in chain(target_directory):
        co=cluster_off(c)
        for o in range(co,co+cluster_bytes,32):
            e=read_at(o,32)
            if e[:11] == target_name: slot=o; old_first=le16(e,26); break
            if slot is None and e[0] in (0,0xE5): slot=o
        if old_first: break
    if slot is None: raise SystemExit("SYSTEM directory has no free entry")
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
