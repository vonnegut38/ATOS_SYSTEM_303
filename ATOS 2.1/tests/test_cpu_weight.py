"""Verify the per-console CPU weighting (docs/terminal-plan.md).

Timer-C preempts the running user every tick and kernel_dispatch_ready_user()
picks the next READY process.  The weight lives in user_task_quantum() and is
applied by user_pick_ready(): a process keeps the CPU while it still owns
quantum time, so the session on screen gets 16 ticks for every 1 tick a
background session gets (a single background process earns ~5.9%).

The test extracts the two functions from kernel_exception.c and simulates the
preempt loop, so it is deterministic and does not depend on emulator timing.

Run:  python3 -m unittest discover -s tests -p "test_cpu_weight.py" -v
"""
import pathlib
import subprocess
import tempfile
import unittest

PRE = '''
#include <stdint.h>
#include <assert.h>
#define USER_PROCESS_SLOTS 3
#define USER_PROCESS_READY 1
#define USER_PROCESS_RUNNING 2
typedef struct {uint32_t terminal,state,saved_pc,saved_usp;uint16_t saved_sr;uint8_t quantum_left;} user_process_t;
static user_process_t process_table[3],*current_process;
static uint8_t next_process_slot;
static unsigned ksessions_current(void){return 0;}
'''

MAIN = '''
int main(void){
    unsigned ticks,on=0,bg=0;user_process_t *p;
    process_table[0].terminal=0;process_table[0].state=USER_PROCESS_READY;
    process_table[1].terminal=1;process_table[1].state=USER_PROCESS_READY;
    for(ticks=0;ticks<1700u;++ticks){
        if(current_process)current_process->state=USER_PROCESS_READY; /* Timer-C preempt */
        p=user_pick_ready();assert(p);
        p->state=USER_PROCESS_RUNNING;current_process=p;
        if(p==&process_table[0])++on;else ++bg;
    }
    assert(on==1600u&&bg==100u);   /* 16:1 over 100 slices */
    assert(bg>=1700u/20u);          /* background still makes progress */
    return 0;}
'''


class CpuWeight(unittest.TestCase):
    def test_on_screen_gets_16_ticks_per_background_tick(self):
        root = pathlib.Path(__file__).resolve().parents[1]
        src = (root / 'system/kernel_exception.c').read_text()
        quantum = src[src.index('#define USER_QUANTUM_ON_SCREEN'):
                      src.index('static user_process_t *user_next_ready(')]
        pick = src[src.index('static user_process_t *user_pick_ready('):
                   src.index('uint32_t kernel_user_take_ready(')]
        with tempfile.TemporaryDirectory() as d:
            p = pathlib.Path(d)
            (p / 't.c').write_text(PRE + quantum + pick + MAIN)
            subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                            str(p / 't.c'), '-o', str(p / 't')], check=True)
            subprocess.run([str(p / 't')], check=True)


if __name__ == '__main__':
    unittest.main()
