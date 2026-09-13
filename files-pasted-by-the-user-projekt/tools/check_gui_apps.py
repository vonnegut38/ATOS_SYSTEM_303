"""Check the shipped native GUI image headers and relocation-free code."""
from pathlib import Path
import struct
import subprocess

for name,elf in [('MONODESK','monodesk'),('COUNT','mono_count'),('NOTE','mono_note')]:
    p=Path('build/system')/(name+'.APP');b=p.read_bytes()
    file_size,memory,entry,stack,checksum=struct.unpack_from('>5I',b,12)
    assert b[:8]==b'ASYSAPP0' and file_size==len(b),name
    assert memory>=file_size and 32<=entry<file_size and stack>=1024,name
    assert sum(b[:28]+b[32:])&0xffffffff==checksum,name+' checksum'
    assert b[-12:-8]==b'APW2',name+' policy'
    disasm=subprocess.check_output(['m68k-elf-objdump','-d',str(p.with_name(elf+'.elf'))],text=True)
    for line in disasm.splitlines():
        if '\tjsr ' in line or '\tjmp ' in line:
            target=line.split('\t')[-1].split()[1]
            assert target.startswith('%'),name+' absolute call: '+line
    print(f'{name}: {file_size} bytes, memory {memory}, stack {stack}; checksum/policy/PIC calls OK')
