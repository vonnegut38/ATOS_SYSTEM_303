"""The real clock implementation: missed polls, pending IRQ and reload races."""
import pathlib, subprocess, tempfile, unittest
class HiresClock(unittest.TestCase):
    def test_irq_snapshot(self):
        root=pathlib.Path(__file__).resolve().parents[1]
        src=(root/'system/kernel_module.c').read_text()
        code=src[src.index('uint32_t kernel_user_hires_clock(void)'):src.index('\nint32_t kernel_user_yield',src.index('uint32_t kernel_user_hires_clock(void)'))]
        code=code.replace('static uint32_t last;','/* last is exposed only by the host fixture */')
        code=code.replace('*(volatile uint8_t *)(uintptr_t)MFP_TCDR','counter_read()').replace('*(volatile uint8_t *)(uintptr_t)0xfffffa0du','fake_pending')
        pre=r'''
#include <stdint.h>
#include <assert.h>
#define KERNEL_SERVICE_TIMER 1
#define KERNEL_TIMER_VERSION 1
volatile uint32_t kernel_timer_ticks;
static uint32_t last;
static uint8_t counter,fake_pending;
static int race;
typedef struct {uint32_t frequency_hz;} kernel_timer_service_t;
static kernel_timer_service_t timer={50};
static const void *kernel_find_service(int s,int v){(void)s;(void)v;return &timer;}
static uint8_t counter_read(void){
 if(race==1){race=2;return 2;}
 if(race==2){race=0;++kernel_timer_ticks;return 246;}
 return counter;
}
'''
        main=r'''
int main(void){
 kernel_timer_ticks=100;counter=240;assert(kernel_user_hires_clock()==24606);
 /* Five periods passed with no poll; every period must still count. */
 kernel_timer_ticks=105;counter=10;assert(kernel_user_hires_clock()==26066);
 kernel_timer_ticks=106;counter=220;fake_pending=0x20;
 assert(kernel_user_hires_clock()==107*246u+26u);
 /* Servicing the pending IRQ must not count that period twice. */
 kernel_timer_ticks=107;counter=219;fake_pending=0;
 assert(kernel_user_hires_clock()==107*246u+27u);
 kernel_timer_ticks=108;counter=230;race=1;
 assert(kernel_user_hires_clock()==109*246u+16u);
 /* A stale outer sample cannot undo a newer nested sample. */
 kernel_timer_ticks=108;assert(kernel_user_hires_clock()==109*246u+16u);
 /* Natural 32-bit timestamp wrap. */
 last=0xfffffff0u;kernel_timer_ticks=17459217u;counter=200;
 assert(kernel_user_hires_clock()==(uint32_t)(17459217u*246u+46u));
 return 0;
}
'''
        with tempfile.TemporaryDirectory() as d:
            p=pathlib.Path(d);(p/'t.c').write_text(pre+code+main)
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'t.c'),'-o',str(p/'t')],check=True)
            subprocess.run([str(p/'t')],check=True)
if __name__=='__main__':unittest.main()
