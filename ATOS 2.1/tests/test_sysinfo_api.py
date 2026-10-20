"""Run the actual diagnostic syscall dispatch with guarded caller memory."""
from pathlib import Path
import subprocess
import tempfile
import unittest

class SysinfoApiTest(unittest.TestCase):
    def test_readonly_bounds(self):
        source = Path('system/kernel_exception.c').read_text()
        # Select diagnostic/process branches without pulling in the audio ABI.
        dispatch = (source[source.index('    if(call==56u)'):source.index('    if(call==75u)')]
                    + source[source.index('    if(call==68u)'):source.index('    if(call==12u)')])
        code = r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "system_report.h"
static uint8_t memory[8192];
static int32_t user_syscall_result;
static uint32_t part_start=100,part_size=32,reads,reports,last;
static int boot=1,success=1;
static uint32_t kernel_user_clock(void){return 123;}
static uint32_t kernel_user_hires_clock(void){return 0;}
static int user_write_range(void *p,uint32_t n){uintptr_t a=(uintptr_t)p,b=(uintptr_t)memory;return a>=b&&a-b<=sizeof(memory)&&n<=sizeof(memory)-(a-b);}
static int user_write_aligned(void *p,uint32_t n,unsigned a){return !((uintptr_t)p&(a-1))&&user_write_range(p,n);}
int kernel_system_report(system_report_t *out){reports++;memset(out,0,sizeof(*out));out->magic=SYSREPORT_MAGIC;return 0;}
static unsigned kpart_count(void){return 1;}
static int kpart_is_boot(uint8_t p){(void)p;return boot;}
static uint32_t kpart_start(uint8_t p){(void)p;return part_start;}
static uint32_t kpart_size(uint8_t p){(void)p;return part_size;}
static int kblock_read_uncached(uint32_t lba,uint16_t n,void *p){assert(n<=8&&user_write_range(p,n*512u));reads++;last=lba;return success;}
static uint32_t kernel_user_process_name(uint32_t slot,char name[13]){(void)slot;(void)name;return 0;}
static uint32_t kernel_user_process_resources(uint32_t slot,uint32_t info[4]){(void)slot;(void)info;return 0;}
static uint32_t kernel_user_process_cpu_ticks(uint32_t slot){(void)slot;return 7;}
static int kernel_user_kill(uint32_t pid){(void)pid;return 1;}
static int kernel_user_suspend(uint32_t pid){(void)pid;return 1;}
static int kernel_user_resume(uint32_t pid){(void)pid;return 1;}
static int dispatch(uint32_t call,uint32_t argument0,uint32_t argument1,void *pointer0){
''' + dispatch + r'''
return -1;}
int main(void){
 dispatch(56,sizeof(system_report_t),0,memory);assert(reports==1&&user_syscall_result==0);
 dispatch(56,sizeof(system_report_t)-1,0,memory);assert(reports==1&&user_syscall_result<0);
 dispatch(56,sizeof(system_report_t),0,memory+1);assert(reports==1&&user_syscall_result<0);
 dispatch(56,sizeof(system_report_t),0,memory+8190);assert(reports==1&&user_syscall_result<0);
 dispatch(56,sizeof(system_report_t),0,0);assert(reports==1&&user_syscall_result<0);
 dispatch(57,24,8,memory);assert(reads==1&&last==124&&user_syscall_result==4096);
 dispatch(57,25,8,memory);assert(reads==1&&user_syscall_result<0);
 dispatch(57,0,0,memory);assert(reads==1&&user_syscall_result<0);
 dispatch(57,0,9,memory);assert(reads==1&&user_syscall_result<0);
 dispatch(57,32,1,memory);assert(reads==1&&user_syscall_result<0);
 dispatch(57,0,8,memory+8191);assert(reads==1&&user_syscall_result<0);
 dispatch(57,0,1,0);assert(reads==1&&user_syscall_result<0);
 boot=0;dispatch(57,0,1,memory);assert(reads==1&&user_syscall_result<0);boot=1;
 part_start=0xfffffff0u;dispatch(57,17,1,memory);assert(reads==1&&user_syscall_result<0);
 part_start=100;success=0;dispatch(57,0,1,memory);assert(reads==2&&user_syscall_result==-0x282);
 dispatch(58,0,0,0);assert(user_syscall_result==123);
 return 0;}
'''
        with tempfile.TemporaryDirectory() as directory:
            c=Path(directory)/'api.c'; c.write_text(code)
            binary=Path(directory)/'api'
            subprocess.run(['cc','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-Iinclude',str(c),'-o',str(binary)],check=True)
            subprocess.run([str(binary)],check=True)
