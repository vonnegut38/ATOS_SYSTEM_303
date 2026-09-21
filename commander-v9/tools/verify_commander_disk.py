"""Read-only FAT16 consistency and unchanged-file audit of the Hatari test disk."""
import hashlib, struct, sys
from pathlib import Path
u16=lambda b,o:struct.unpack_from('<H',b,o)[0]
u32=lambda b,o:struct.unpack_from('<I',b,o)[0]
def inspect(path):
 f=open(path,'rb')
 def read(o,n):
  f.seek(o);b=f.read(n);assert len(b)==n;return b
 root=read(0,512);start=struct.unpack_from('>I',root,0x1ca)[0]*512;b=read(start,512)
 bps=u16(b,11);spc=b[13];spf=u16(b,22);nf=b[16];roots=u16(b,17);total=u16(b,19) or u32(b,32)
 fo=start+u16(b,14)*bps;fat=read(fo,spf*bps);ro=fo+nf*spf*bps;rs=(roots*32+bps-1)//bps;data=ro+rs*bps;cb=spc*bps
 count=(total-u16(b,14)-nf*spf-rs)//spc
 for i in range(1,nf):assert read(fo+i*spf*bps,spf*bps)==fat,'FAT mirrors differ'
 assert u16(fat,2)&0xc000==0xc000,'dirty or hard-error FAT'
 owners={};files={}
 def chain(c,name):
  ids=[]
  if not c:return ids
  while True:
   assert 2<=c<count+2 and c<0xfff0,(name,'invalid cluster',c)
   assert c not in owners,(name,'crosslink/cycle',c,owners.get(c))
   owners[c]=name;ids.append(c);c=u16(fat,c*2)
   if c>=0xfff8:return ids
 def walk(raw,prefix):
  for o in range(0,len(raw),32):
   e=raw[o:o+32]
   if not e[0]:break
   if e[0] in (229,46) or e[11]&15==15 or e[11]&8:continue
   name=e[:8].decode('ascii').rstrip();ext=e[8:11].decode('ascii').rstrip();name+=('.'+ext) if ext else '';name=prefix+'/'+name
   ids=chain(u16(e,26),name);payload=b''.join(read(data+(c-2)*cb,cb) for c in ids)
   if e[11]&16:walk(payload,name)
   else:
    size=u32(e,28);assert len(payload)>=size,(name,'short chain');files[name]=hashlib.sha256(payload[:size]).hexdigest()
 walk(read(ro,roots*32),'')
 allocated={c for c in range(2,min(count+2,0xfff0)) if u16(fat,c*2)!=0}
 assert set(owners)<=allocated
 lost=allocated-set(owners)
 f.close();return files,lost
current,lost=inspect(sys.argv[1]);before,oldlost=inspect(sys.argv[2])
allowed={'/SYSTEM/KERNEL.SYS',*sys.argv[3:]}
for name,digest in before.items():
 if name not in allowed:assert current.get(name)==digest,('changed original file',name)
assert lost==oldlost,('new lost clusters',lost-oldlost)
assert '/MCPROBE/COPY.SYS' not in current
assert '/TMP/DESKTOP.APP' not in current
print(f'Disk audit PASS: {len(current)} files, mirrored clean FAT, no new lost clusters or crosslinks; original files unchanged except {sorted(allowed)}.')
