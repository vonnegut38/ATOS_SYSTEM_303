"""The BIOS configuration allocator must update both FAT copies."""
from pathlib import Path
import subprocess,tempfile,unittest
class BiosFatMirrors(unittest.TestCase):
 def test_config_allocation_updates_both_copies(self):
  s=Path('hw/config.c').read_text();fn=s[s.index('static int cfg_fat_put('):s.index('static uint16_t cfg_alloc(')]
  code=r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
static uint8_t cfg_id,cfg_sector[512],cfg_fat_copies=2,media[8][512];
static uint16_t cfg_spf=3;
static uint32_t cfg_fat_start=1;
static int fail_read=-1,fail_write=-1;
static int acsi_read_sector(uint8_t id,uint32_t lba,void *out){assert(id==0&&lba<8);if((int)lba==fail_read)return 0;memcpy(out,media[lba],512);return 1;}
static int acsi_write_sector(uint8_t id,uint32_t lba,const void *in){assert(id==0&&lba<8);if((int)lba==fail_write)return 0;memcpy(media[lba],in,512);return 1;}
'''+fn+r'''
int main(void){
 memset(media,0x35,sizeof media);assert(cfg_fat_put(353,0xffff));
 assert(media[2][194]==255&&media[2][195]==255&&media[5][194]==255&&media[5][195]==255);
 assert(media[2][193]==0x35&&media[5][196]==0x35);
 fail_read=5;assert(!cfg_fat_put(353,0x1234));fail_read=-1;
 fail_write=5;assert(!cfg_fat_put(353,0x4321));fail_write=-1;
 cfg_fat_copies=1;assert(cfg_fat_put(256,0x1234));assert(media[2][0]==0x34&&media[2][1]==0x12);return 0;}
'''
  with tempfile.TemporaryDirectory() as d:
   c=Path(d)/'test.c';c.write_text(code);exe=Path(d)/'test'
   subprocess.run(['cc','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(c),'-o',str(exe)],check=True)
   subprocess.run([str(exe)],check=True)
