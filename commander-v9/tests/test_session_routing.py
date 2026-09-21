"""Compile actual routing/completion code with a small process-table fixture."""
from pathlib import Path
import subprocess,tempfile,unittest
class Routing(unittest.TestCase):
 def test_background_routing(self):
  src=Path('system/kernel_exception.c').read_text()
  routing=src[src.index('int32_t kernel_user_session_getchar'):src.index('int kernel_user_take_completion')]
  begin=src[src.index('static void user_console_begin_output'):src.index('\n}',src.index('static void user_console_begin_output'))+2]
  code='''#include <stdint.h>
#include <assert.h>
#include <string.h>
#define USER_PROCESS_SLOTS 6
#define SESSION_COUNT 3
#define USER_PROCESS_FREE 0
#define USER_PROCESS_TERMINATED 5
#define user_active (current_process->state==2)
static struct {uint32_t pid;unsigned terminal,state;} process_table[6],*current_process=process_table;
static unsigned active,output_terminal;
static char output[32];
static unsigned ksessions_current(void){return active;}
static unsigned gui_desktop_pid;
static int32_t kernel_user_terminal_getchar(void){return 65;}
static unsigned terminal_owners[3];
static uint8_t user_console_flags[3],user_completion_notified[4],user_completion_next;
static struct {uint32_t pid;int32_t status;uint8_t terminal;} user_completions[4];
static int kernel_dispatch_active_user(void){return 1;}
static void kconsole_puts_to(unsigned t,const char *s){output_terminal=t;strcat(output,s);}
'''+routing+begin+'''
int main(void){uint32_t pid;int32_t status;
 process_table[0].pid=4;process_table[0].terminal=1;process_table[0].state=2;
 assert(kernel_user_birth_terminal()==1&&kernel_user_last_terminal()==1);
 assert(kernel_user_session_getchar()==-1);active=1;assert(kernel_user_session_getchar()==65);active=0;
 user_console_begin_output();assert(output_terminal==1&&!strcmp(output,"\\n"));
 assert(user_console_flags[1]&&!user_console_flags[0]);
 user_console_begin_output();assert(!strcmp(output,"\\n"));
 assert(kernel_user_session_busy(1)&&!kernel_user_session_busy(0));
 gui_desktop_pid=4;assert(!kernel_user_desktop_visible());active=1;assert(kernel_user_desktop_visible());
 terminal_owners[1]=4;active=0;user_terminal_release(4);assert(!terminal_owners[1]);
 user_record_completion(4,0);assert(!kernel_user_next_completion(&pid,&status));
 active=1;assert(kernel_user_next_completion(&pid,&status)&&pid==4&&status==0);
 assert(!kernel_user_next_completion(&pid,&status));
 kernel_user_assign_terminal(4,2);assert(kernel_user_pid_terminal(4)==2);
 kernel_user_assign_terminal(4,99);assert(kernel_user_pid_terminal(4)==2);
 process_table[0].state=5;assert(!kernel_user_session_busy(2));
 active=0;assert(kernel_user_birth_terminal()==0);
 return 0;}
'''
  with tempfile.TemporaryDirectory() as d:
   p=Path(d);(p/'test.c').write_text(code)
   subprocess.run(['cc','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
   subprocess.run([str(p/'test')],check=True)
