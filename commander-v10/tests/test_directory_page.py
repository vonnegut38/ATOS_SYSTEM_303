"""Exercise the kernel's actual page callback, including output guards."""
from pathlib import Path
import subprocess
import tempfile
import unittest

class DirectoryPageTest(unittest.TestCase):
    def test_pagination_and_guards(self):
        s=Path('system/kernel_exception.c').read_text()
        start=s.index('typedef struct {uint32_t skip,seen,count,capacity;user_dir_entry_t *entries;}')
        end=s.index('int32_t kernel_user_syscall_result',start)
        code='''#include <stdint.h>
#include <assert.h>
#include <string.h>
typedef struct {char name[13];uint8_t attributes,padding[2];uint32_t size;} user_dir_entry_t;
'''+s[start:end]+'''
int main(void){unsigned skip,cap,i;
 for(skip=0;skip<50;skip++)for(cap=1;cap<=32;cap++){
  struct {uint32_t before;user_dir_entry_t entries[32];uint32_t after;} out;
  user_dir_page_t p={skip,0,0,cap,out.entries};memset(&out,0xcc,sizeof(out));out.before=out.after=0x12345678;
  for(i=0;i<40;i++)if(!user_dir_page_callback("FILE1234.TXT",i,32,&p))break;
  unsigned want=skip<40?40-skip:0;if(want>cap)want=cap;
  assert(p.count==want&&out.before==0x12345678&&out.after==0x12345678);
  for(i=0;i<want;i++){assert(out.entries[i].size==skip+i);assert(!strcmp(out.entries[i].name,"FILE1234.TXT"));assert(out.entries[i].attributes==32);}
  for(i=want*sizeof(user_dir_entry_t);i<sizeof(out.entries);i++)assert(((uint8_t *)out.entries)[i]==0xcc);
 }
 return 0;}
'''
        with tempfile.TemporaryDirectory() as d:
            c=Path(d)/'page.c';c.write_text(code);binary=Path(d)/'page'
            subprocess.run(['cc','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(c),'-o',str(binary)],check=True)
            subprocess.run([str(binary)],check=True)
