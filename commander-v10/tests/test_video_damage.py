"""Changed terminal cells must dirty only the corresponding output rectangles."""
from pathlib import Path
import subprocess,tempfile,unittest
class VideoDamage(unittest.TestCase):
 def test_character_damage(self):
  root=Path(__file__).resolve().parents[1]
  source=(root/'system/kernel_graphics.c').read_text()
  fn=source[source.index('static void terminal_video_damage'):source.index('/* Foreground terminal presentation:')]
  code='''#include <assert.h>
#include <stdint.h>
#include "text_terminal.h"
static int enabled,count,x,y,w,h;
static int kvideo_is_vofa(void){return enabled;}
static void kvideo_damage(int32_t a,int32_t b,int32_t c,int32_t d){count++;x=a;y=b;w=c;h=d;}
'''+fn+'''
int main(void){term_display_t cache={0};term_cell_t cells[TERM_CELLS]={0};
 terminal_video_damage(&cache,cells);assert(!count);
 enabled=1;terminal_video_damage(&cache,cells);assert(count==1&&x==0&&y==0&&w==640&&h==400);
 cache.valid=1;count=0;terminal_video_damage(&cache,cells);assert(!count);
 cells[1999]=65;terminal_video_damage(&cache,cells);assert(count==1&&x==624&&y==384&&w==16&&h==16);
 cache.cells[1999]=65;count=0;terminal_video_damage(&cache,cells);assert(!count);
 cells[0]=256;terminal_video_damage(&cache,cells);assert(count==1&&x==0&&y==0&&w==16&&h==16);
 return 0;}
'''
  with tempfile.TemporaryDirectory() as d:
   p=Path(d);(p/'test.c').write_text(code)
   subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(root/'include'),str(p/'test.c'),'-o',str(p/'test')],check=True)
   subprocess.run([str(p/'test')],check=True)
