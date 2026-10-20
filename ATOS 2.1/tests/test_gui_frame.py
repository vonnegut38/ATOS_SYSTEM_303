"""Exercise the real GUI_FRAME syscall branch with isolated renderer/WM stubs."""
import pathlib, subprocess, tempfile, unittest

class GuiFrame(unittest.TestCase):
    def test_validated_atomic_submission(self):
        root=pathlib.Path(__file__).resolve().parents[1]
        src=(root/'system/kernel_exception.c').read_text()
        code=src[src.index('    if(call==81u){'):src.index('    if(call==36u){',src.index('    if(call==81u){'))]
        pre=r'''
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include "gui_draw.h"
static struct {unsigned terminal;uint32_t pid;} process={0,7},*current_process=&process;
static int32_t user_syscall_result;
static uint32_t kdbg_gui_frame_stats[8];
static gui_draw_t draws[4];static uint16_t groups[2];
static uint8_t surface[4],screen[4],*target=screen;
static unsigned session,drawn,composed,allocated,dirty,clockvalue;
static int enabled=1,desktop=1,game=0;
static int user_range(const void *p,uint32_t n){
 uintptr_t a=(uintptr_t)p,b=(uintptr_t)draws,c=(uintptr_t)groups;
 return (a>=b&&a-b<=sizeof(draws)&&n<=sizeof(draws)-(a-b))||
        (a>=c&&a-c<=sizeof(groups)&&n<=sizeof(groups)-(a-c));
}
static unsigned ksessions_current(void){return session;}
static int kernel_wm_enabled(void){return enabled;}
static int kgfx_desktop_active(void){return desktop;}
static int kgame_active(void){return game;}
static uint8_t *kernel_wm_app_surface(uint32_t pid,unsigned handle){assert(pid==7&&handle==0);allocated++;return surface;}
static void kgfx_overlay_hide(void){}
static void kgfx_cursor_hide(void){}
static uint32_t kernel_user_hires_clock(void){return ++clockvalue;}
static uint8_t *kgfx_set_target(uint8_t *p){uint8_t *old=target;target=p;return old;}
static void kgfx_draw_batch(const gui_draw_t *p,uint32_t n){assert(target==surface);assert(p==draws+drawn);drawn+=n;}
static int kgfx_batch_bounds(int32_t *l,int32_t *t,int32_t *r,int32_t *b){*l=dirty*100;*r=*l+8;*t=0;*b=8;return 1;}
static void kernel_wm_app_dirty(uint8_t *s,int32_t l,int32_t t,int32_t r,int32_t b){assert(s==surface&&r-l==8&&b-t==8);dirty++;}
static void kernel_wm_compose_now(void){assert(target==screen&&drawn==4&&dirty==2);composed++;}
static int invoke(uint32_t argument0,uint32_t argument1,void *pointer0,void *pointer1){
 unsigned call=81;user_syscall_result=0;
'''
        main=r'''
 return 0;}
int main(void){
 groups[0]=2;groups[1]=2;
 invoke(4,2,draws,groups);assert(user_syscall_result==0&&drawn==4&&dirty==2&&composed==1&&allocated==1);
 assert(kdbg_gui_frame_stats[0]==1&&kdbg_gui_frame_stats[4]==4);
 drawn=dirty=composed=allocated=0;
 groups[1]=1;invoke(4,2,draws,groups);assert(user_syscall_result<0&&!allocated);
 groups[1]=81;invoke(4,2,draws,groups);assert(user_syscall_result<0&&!allocated);
 groups[1]=2;
 invoke(0,2,draws,groups);assert(user_syscall_result<0);
 invoke(UINT32_MAX,2,draws,groups);assert(user_syscall_result<0);
 invoke(4,33,draws,groups);assert(user_syscall_result<0);
 invoke(4,2,(char*)draws+1,groups);assert(user_syscall_result<0);
 invoke(4,2,draws,(char*)groups+1);assert(user_syscall_result<0);
 invoke(4,2,screen,groups);assert(user_syscall_result<0);
 invoke(4,2,draws+1,groups);assert(user_syscall_result<0);
 invoke(4,2,draws,groups+1);assert(user_syscall_result<0);
 session=1;invoke(4,2,draws,groups);assert(user_syscall_result==1&&!allocated);
 session=0;game=1;invoke(4,2,draws,groups);assert(user_syscall_result<0&&!allocated);
 game=0;enabled=0;invoke(4,2,draws,groups);assert(user_syscall_result<0&&!allocated);
 assert(!drawn&&!dirty&&!composed);
 return 0;
}
'''
        with tempfile.TemporaryDirectory() as d:
            p=pathlib.Path(d);(p/'t.c').write_text(pre+code+main)
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(root/'include'),str(p/'t.c'),'-o',str(p/'t')],check=True)
            subprocess.run([str(p/'t')],check=True)
if __name__=='__main__':unittest.main()
