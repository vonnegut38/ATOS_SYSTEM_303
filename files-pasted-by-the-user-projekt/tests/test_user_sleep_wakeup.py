import pathlib
import subprocess
import tempfile
import unittest

class UserSleepWakeup(unittest.TestCase):
    def test_idle_dispatch_wakes_sleepers(self):
        root=pathlib.Path(__file__).resolve().parents[1]
        src=(root/'system/kernel_exception.c').read_text()
        wake=src[src.index('static void user_wake_sleepers('):src.index('static user_process_t *user_next_ready(')]
        dispatch=src[src.index('uint32_t kernel_user_take_ready('):src.index('static uint32_t user_switch_frame(')]
        pre='''
#include <stdint.h>
#include <assert.h>
#define USER_PROCESS_SLOTS 3
#define USER_PROCESS_READY 1
#define USER_PROCESS_RUNNING 2
#define USER_PROCESS_SLEEPING 3
#define USER_PROCESS_MESSAGE 8
typedef struct {uint32_t state,wake_tick,saved_registers[15],saved_pc,saved_usp;uint16_t saved_sr;} user_process_t;
static user_process_t process_table[3],*current_process;
static uint8_t next_process_slot;
static uint32_t now;
static uint32_t kernel_user_clock(void){return now;}
'''
        main='''
int main(void){uint32_t regs[15],pc;uint16_t sr;
 process_table[0].state=USER_PROCESS_SLEEPING;process_table[0].wake_tick=50;
 process_table[0].saved_pc=0x1000;process_table[0].saved_usp=0x2000;process_table[0].saved_sr=0x500;
 process_table[1].state=USER_PROCESS_MESSAGE;
 process_table[2].state=USER_PROCESS_SLEEPING;process_table[2].wake_tick=60;
 now=49;assert(kernel_user_take_ready(regs,&pc,&sr)==0);
 now=50;assert(kernel_user_take_ready(regs,&pc,&sr)==0x2000);
 assert(pc==0x1000&&sr==0x500&&regs[0]==50);
 assert(process_table[1].state==USER_PROCESS_MESSAGE&&process_table[2].state==USER_PROCESS_SLEEPING);
 process_table[0].state=USER_PROCESS_MESSAGE;process_table[2].wake_tick=2;
 process_table[2].saved_usp=0x3000;now=0xfffffffe;
 assert(kernel_user_take_ready(regs,&pc,&sr)==0);
 now=2;assert(kernel_user_take_ready(regs,&pc,&sr)==0x3000);
 return 0;}
'''
        with tempfile.TemporaryDirectory() as d:
            p=pathlib.Path(d);(p/'test.c').write_text(pre+wake+dispatch+main)
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
            subprocess.run([str(p/'test')],check=True)
