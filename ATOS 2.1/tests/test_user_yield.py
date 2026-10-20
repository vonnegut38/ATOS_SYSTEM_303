"""A zero-duration sleep must route supervisor input without a timer wait."""
import pathlib
import subprocess
import tempfile
import unittest

class UserYield(unittest.TestCase):
    def test_yield_and_sleep_context(self):
        root = pathlib.Path(__file__).resolve().parents[1]
        src = (root / 'system/kernel_exception.c').read_text()
        begin = src.index('uint32_t kernel_user_sleep_context(')
        end = src.index('\nuint32_t ', begin + 10)
        code = src[begin:end]
        pre = r'''
#include <stdint.h>
#include <assert.h>
#include <string.h>
#define USER_PROCESS_READY 1
#define USER_PROCESS_SLEEPING 3
static struct {
 uint32_t saved_registers[15],saved_usp,saved_pc,wake_tick,state;
 uint16_t saved_sr;uint8_t registers_valid;
} process,*current_process=&process;
uint32_t kdbg_sleep_calls,kdbg_sleep_zero,kdbg_sleep_park;
static int heap_ok=1,stack_ok=1,failed;
static int32_t now=50;
static int user_heaps_valid_current(void){return heap_ok;}
static int user_stack_valid(const void *p,uint32_t usp){(void)p;return stack_ok&&usp==0x2000;}
static void user_heap_fail_current(void){failed=1;}
static void user_stack_fail_current(void){failed=2;}
static uint32_t user_switch_frame(uint32_t *r,int save,uint32_t *pc,uint16_t *sr,int a,int b)
{(void)r;(void)save;(void)pc;(void)sr;(void)a;(void)b;return 99;}
static int32_t kernel_user_yield(void){return now;}
'''
        main = r'''
int main(void){
 uint32_t regs[15],pc=0x1000;uint16_t sr=0x500;unsigned i;
 for(i=0;i<15;++i)regs[i]=100+i;
 assert(kernel_user_sleep_context(regs,0x2000,&pc,&sr,0)==UINT32_MAX);
 assert(process.state==USER_PROCESS_READY&&process.saved_registers[0]==50);
 assert(process.saved_pc==pc&&process.saved_sr==sr&&process.saved_usp==0x2000);
 assert(process.registers_valid&&kdbg_sleep_park==1);
 for(i=1;i<15;++i)assert(process.saved_registers[i]==regs[i]);
 assert(kernel_user_sleep_context(regs,0x2000,&pc,&sr,3)==UINT32_MAX);
 assert(process.state==USER_PROCESS_SLEEPING&&process.wake_tick==53);
 assert(kernel_user_sleep_context(regs,0x2000,&pc,&sr,3001)==0);
 assert(kernel_user_sleep_context(0,0x2000,&pc,&sr,0)==0);
 assert(kernel_user_sleep_context(regs,0x2000,0,&sr,0)==0);
 heap_ok=0;assert(kernel_user_sleep_context(regs,0x2000,&pc,&sr,0)==99&&failed==1);
 heap_ok=1;stack_ok=0;assert(kernel_user_sleep_context(regs,0x2000,&pc,&sr,0)==99&&failed==2);
 stack_ok=1;now=-1;assert(kernel_user_sleep_context(regs,0x2000,&pc,&sr,0)==0);
 return 0;
}
'''
        with tempfile.TemporaryDirectory() as d:
            p = pathlib.Path(d)
            (p/'test.c').write_text(pre+code+main)
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
            subprocess.run([str(p/'test')],check=True)
