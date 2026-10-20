"""Keyboard reader must never steal unparsed IKBD bytes from INPUT.SYS."""
from pathlib import Path
import subprocess,tempfile,unittest
class KeyboardFraming(unittest.TestCase):
 def test_mouse_coordinates_are_not_keys(self):
  s=Path('system/kernel_module.c').read_text();s=s[s.index('int kernel_user_keyboard_event('):s.index('int kernel_raw_mouse_event(')]
  code=r'''
#include <stdint.h>
#include <assert.h>
#define KERNEL_SERVICE_INPUT 1
#define KERNEL_INPUT_VERSION 1
#define KERNEL_SERVICE_EVENT 2
#define KERNEL_EVENT_VERSION 1
#define KERNEL_INPUT_KEY_DOWN 1
#define KERNEL_INPUT_KEY_UP 2
static uint8_t user_shift_state,user_key_plain[64],user_key_shifted[64];
typedef struct {int (*poll)(uint32_t);} kernel_input_service_t;
typedef struct {int (*pending)(uint32_t);int (*receive)(uint32_t,uint32_t *,uint32_t *);} kernel_event_service_t;
unsigned ikbd_raw_head,ikbd_raw_tail,remaining,queued,raw_reads,available=1;
uint8_t raw_bytes[256];
void kernel_ikbd_irq_capture(void){}
int32_t ikbd_raw_pop(void){raw_reads++;return raw_bytes[ikbd_raw_tail++];}
static int poll(uint32_t t){assert(t==0);if(ikbd_raw_tail==ikbd_raw_head)return 0;
 unsigned b=raw_bytes[ikbd_raw_tail++];
 if(remaining){remaining--;return 0;}
 if(b>=0xf8&&b<=0xfb){remaining=2;return 0;}
 queued=b;return 1;
}
static int pending(uint32_t t){assert(t==0);return queued!=0;}
static int receive(uint32_t t,uint32_t *type,uint32_t *value){assert(t==0&&queued);*type=(queued&128)?2:1;*value=queued;queued=0;return 1;}
static const void *kernel_find_service(unsigned id,unsigned v){static const kernel_input_service_t i={poll};static const kernel_event_service_t e={pending,receive};(void)v;if(!available)return 0;return id==1?(const void *)&i:(const void *)&e;}
'''+s+r'''
int main(void){uint32_t type,scan,mods,ch;
 /* Escape/F10, modifier codes and packet-header values are all legal deltas. */
 const uint8_t deltas[]={1,0x44,0x2a,0x38,0x81,0xc4,0xf8,0xfb};
 for(unsigned i=0;i<sizeof deltas;i++){
  ikbd_raw_tail=0;ikbd_raw_head=3;remaining=queued=0;
  raw_bytes[0]=0xf8;raw_bytes[1]=deltas[i];raw_bytes[2]=deltas[sizeof deltas-1-i];
  for(unsigned j=0;j<3;j++)assert(!kernel_user_keyboard_event(&type,&scan,&mods,&ch));
  assert(ikbd_raw_tail==3&&remaining==0&&raw_reads==0&&user_shift_state==0);
 }
 raw_bytes[0]=0x44;raw_bytes[1]=0xc4;ikbd_raw_head=2;ikbd_raw_tail=0;
 assert(kernel_user_keyboard_event(&type,&scan,&mods,&ch)&&scan==0x44&&type==1);
 assert(kernel_user_keyboard_event(&type,&scan,&mods,&ch)&&scan==0x44&&type==2);
 assert(!kernel_user_keyboard_event(&type,&scan,&mods,&ch));
 available=0;ikbd_raw_tail=0;
 assert(!kernel_user_keyboard_event(&type,&scan,&mods,&ch)&&ikbd_raw_tail==0&&raw_reads==0);
 return 0;}
'''
  with tempfile.TemporaryDirectory() as d:
   p=Path(d);(p/'test.c').write_text(code)
   subprocess.run(['cc','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
   subprocess.run([str(p/'test')],check=True)
