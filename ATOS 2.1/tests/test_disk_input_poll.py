"""Real disk wait and raw-ring capture preserve bytes while supervisor IRQs wait."""
from pathlib import Path
import subprocess,tempfile,unittest
class DiskInputPoll(unittest.TestCase):
 def test_capture_during_wait_and_timeout(self):
  module=Path('system/kernel_module.c').read_text();raw=module[module.index('static volatile uint8_t ikbd_raw_data'):module.index('void kernel_user_terminal_reset_modifiers')]
  block=Path('system/kernel_block.c').read_text();wait=block[block.index('static int disk_irq('):block.index('static int disk_write(',block.index('static int disk_irq('))]
  code=r'''
#include <stdint.h>
#include <assert.h>
static unsigned pos,total,busy,timeout_mode;
static uint8_t bytes[64];
static unsigned reg8(uint32_t a){
 if(a==0xfffffc00u)return pos<total?1:0;
 if(a==0xfffffc02u){assert(pos<total);return bytes[pos++];}
 assert(a==0xfffffa01u);busy++;return timeout_mode||pos<total?32:0;
}
#define IKBD_ACIA_CTRL 0xfffffc00u
#define IKBD_ACIA_DATA 0xfffffc02u
#define REG8(a) reg8(a)
#define DISK_REG8(a) reg8(a)
'''+raw+'\nstatic void kernel_ikbd_poll_capture(void){kernel_ikbd_irq_capture();}\n'+wait+r'''
int main(void){for(unsigned round=0;round<8;round++){
 kernel_ikbd_irq_reset();pos=busy=0;total=64;
 for(unsigned i=0;i<total;i++)bytes[i]=(uint8_t)(i+round*32);
 assert(disk_irq(128)&&pos==total&&busy==64&&!ikbd_raw_dropped());
 for(unsigned i=0;i<total;i++)assert(ikbd_raw_pop()==(0x100|bytes[i]));
 assert(ikbd_raw_pop()==-1);
 }
 kernel_ikbd_irq_reset();pos=0;total=4;timeout_mode=1;
 assert(!disk_irq(10)&&pos==4&&!ikbd_raw_dropped());
 for(unsigned i=0;i<4;i++)assert(ikbd_raw_pop()==(0x100|bytes[i]));
 assert(ikbd_raw_pop()==-1);return 0;}
'''
  with tempfile.TemporaryDirectory() as d:
   p=Path(d);(p/'t.c').write_text(code)
   subprocess.run(['cc','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'t.c'),'-o',str(p/'t')],check=True)
   subprocess.run([str(p/'t')],check=True)
