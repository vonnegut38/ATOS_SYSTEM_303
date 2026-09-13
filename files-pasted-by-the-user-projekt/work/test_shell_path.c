#include <stdio.h>
#include <string.h>
#include <stdint.h>
int shell_build_path(const char *,const char *,char *,uint32_t);
static int check(const char *cwd,const char *arg,const char *want)
{char out[64];int ok=shell_build_path(cwd,arg,out,sizeof(out));printf("%s + %s = %s\n",cwd,arg,ok?out:"FAIL");return ok&&strcmp(out,want)==0;}
int main(void)
{return !(check("\\","system","\\system")&&check("\\","\\system","\\system")&&check("\\SYSTEM","drivers","\\SYSTEM\\drivers")&&check("\\SYSTEM","","\\SYSTEM"));}
