from pathlib import Path
import struct,hashlib
f=open('outputs/atari_system_desktop_768.img','rb')
def read(o,n):f.seek(o);return f.read(n)
def u16(b,o):return struct.unpack_from('<H',b,o)[0]
def u32(b,o):return struct.unpack_from('<I',b,o)[0]
r=read(0,512);part=struct.unpack_from('>I',r,0x1ca)[0];base=part*512;b=read(base,512)
bps=u16(b,11);spc=b[13];fat=base+u16(b,14)*bps;root=fat+b[16]*u16(b,22)*bps;roots=u16(b,17);data=root+((roots*32+bps-1)//bps)*bps;size=bps*spc

def chain(c):
 while 2<=c<0xfff8:
  yield c;c=u16(read(fat+c*2,2),0)
def offset(c):return data+(c-2)*size
def entries(c=0):
 blocks=[read(root,roots*32)] if not c else [read(offset(k),size) for k in chain(c)]
 for block in blocks:
  for o in range(0,len(block),32):
   e=block[o:o+32]
   if e[0] not in (0,229):yield e
sys=next(e for e in entries() if e[:11]==b'SYSTEM     ')
app=next(e for e in entries(u16(sys,26)) if e[:11]==b'APP        ')
for name in ['MONODESK','COUNT','NOTE']:
 e=next(e for e in entries(u16(app,26)) if e[:11]==name.ljust(8).encode()+b'APP')
 binary=b''.join(read(offset(k),size) for k in chain(u16(e,26)))[:u32(e,28)]
 source=Path('build/system/'+name+'.APP').read_bytes()
 print(name,len(binary),'match',binary==source,'sum',sum(binary[:28]+binary[32:]),struct.unpack_from('>I',binary,28)[0], 'policy',binary[-12:].hex())
