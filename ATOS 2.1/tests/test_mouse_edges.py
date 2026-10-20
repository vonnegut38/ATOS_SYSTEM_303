"""Exercise ordered mouse packets: clicks must retain their original positions."""
from pathlib import Path
import subprocess,tempfile,unittest
class MouseEdgeTest(unittest.TestCase):
 def test_edges(self):
  s=Path('system/kernel_module.c').read_text();a=s.index('int kernel_raw_mouse_event(');b=s.index('int kernel_user_mouse_event(',a)
  code='''#include <stdint.h>
#include <assert.h>
#define KERNEL_SERVICE_INPUT 1
#define KERNEL_INPUT_VERSION 1
#define KERNEL_SERVICE_EVENT 2
#define KERNEL_EVENT_VERSION 1
#define KERNEL_INPUT_MOUSE_PACKET 3
static uint8_t user_mouse_buttons;
typedef struct {int (*poll)(uint32_t);} kernel_input_service_t;
typedef struct {int (*pending)(uint32_t);int (*receive)(uint32_t,uint32_t *,uint32_t *);} kernel_event_service_t;
static uint32_t packets[4];static unsigned n,pos;
static int poll(uint32_t task){(void)task;return 0;}
static int pending(uint32_t task){assert(task==1);return pos<n;}
static int receive(uint32_t task,uint32_t *type,uint32_t *v){assert(task==1&&pos<n);*type=3;*v=packets[pos++];return 1;}
static const void *kernel_find_service(unsigned id,unsigned version){static const kernel_input_service_t i={poll};static const kernel_event_service_t e={pending,receive};(void)version;return id==1?(const void *)&i:(const void *)&e;}
'''+s[a:b]+'''
int main(void){uint32_t buttons;int32_t x,y;unsigned k;
 /* The IKBD status byte has bit0 = right, bit1 = left; the kernel swaps them
    so ATOS' primary button is bit0.  Expect the swapped bits here. */
 for(k=1;k<=3;k++){unsigned s=((k&1u)<<1)|((k>>1)&1u);pos=0;n=3;user_mouse_buttons=0;
 packets[0]=(k<<16)|0xfe03;packets[1]=(k<<16)|0x4444;packets[2]=0x01ff;
 assert(kernel_raw_mouse_event(&buttons,&x,&y));assert(buttons==(s|4u|(s<<3)));assert(x==-2&&y==3);
 assert(kernel_raw_mouse_event(&buttons,&x,&y));assert(buttons==s&&x==68&&y==68);
 assert(kernel_raw_mouse_event(&buttons,&x,&y));assert(buttons==0&&x==1&&y==-1);
 assert(!kernel_raw_mouse_event(&buttons,&x,&y));}
 pos=0;n=1;packets[0]=0x020001;assert(kernel_raw_mouse_event(&buttons,&x,&y));assert(buttons==13&&x==0&&y==1);
 pos=0;n=1;packets[0]=0;assert(kernel_raw_mouse_event(&buttons,&x,&y));assert(buttons==0);
 return 0;}
'''
  with tempfile.TemporaryDirectory() as d:
   c=Path(d)/'test.c';exe=Path(d)/'test';c.write_text(code)
   subprocess.run(['cc','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(c),'-o',str(exe)],check=True)
   subprocess.run([str(exe)],check=True)
