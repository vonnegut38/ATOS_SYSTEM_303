"""Exhaustive RGB444 equivalence, stable ties and cache invalidation, with sanitizers."""
from pathlib import Path
import subprocess,tempfile,unittest
class QuantizerTest(unittest.TestCase):
 def test_exact_palette_mapping(self):
  s=Path('system/png_icon.c').read_text();s=s[s.index('int png_icon_convert('):]
  c='#include <stdint.h>\n#include <assert.h>\n#include <string.h>\n'+s+r'''
static uint8_t rgba[4096*4],pixels[4096],mask[512];
static uint16_t palette[255];
int main(void){
 unsigned pass,k,i;
 for(k=0;k<4096;k++){
  rgba[k*4]=(uint8_t)((k>>8)*17);rgba[k*4+1]=(uint8_t)(((k>>4)&15)*17);
  rgba[k*4+2]=(uint8_t)((k&15)*17);rgba[k*4+3]=255;
 }
 for(pass=0;pass<6;pass++){
  unsigned count=pass<3?216:(pass==3?4:255);
  for(i=0;i<count;i++)palette[i]=(uint16_t)((i*19+(pass==0?0:53))&4095);
  if(pass==5)for(i=0;i<count;i++)palette[i]=0x777; /* first tie must win */
  for(unsigned repeat=0;repeat<2;repeat++){
   assert(!png_icon_convert(rgba,64,64,palette,(uint8_t)count,pixels,mask,64));
   for(k=0;k<4096;k++){
    unsigned best=0,dist=1000;
    for(i=0;i<count;i++){
     int dr=(int)(k>>8)-(int)(palette[i]>>8);
     int dg=(int)((k>>4)&15)-(int)((palette[i]>>4)&15);
     int db=(int)(k&15)-(int)(palette[i]&15);
     unsigned d=(unsigned)(dr*dr+dg*dg+db*db);
     if(d<dist){dist=d;best=i;}
    }
    assert(pixels[k]==best);assert(mask[k>>3]&(128>>(k&7)));
   }
  }
 }
 return 0;
}
'''
  with tempfile.TemporaryDirectory() as d:
   p=Path(d);(p/'test.c').write_text(c)
   subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
   subprocess.run([str(p/'test')],check=True)
if __name__=='__main__':unittest.main()
