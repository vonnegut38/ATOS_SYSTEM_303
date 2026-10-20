#include <stdio.h>
#include <string.h>
#include "cpu_ops.h"
static int fails;
static void check(int ok,const char*w){if(!ok){printf("FAIL %s\n",w);++fails;}}
int main(void)
{
    check(cpu_ops_family_for(CPU_68000)==CPU_FAM_68000,"68000 family");
    check(cpu_ops_family_for(CPU_68030)==CPU_FAM_68030,"68030 family");
    check(cpu_ops_family_for(CPU_68060)==CPU_FAM_68060,"68060 family");
    check(cpu_ops_family_for(99u)==CPU_FAM_68000,"unknown -> 68000");
    check(!cpu_ops_for(CPU_68000)->has_vbr,"68000 no vbr");
    check(cpu_ops_for(CPU_68010)->has_vbr,"68010 vbr");
    check(!cpu_ops_for(CPU_68010)->has_cacr,"68010 no cacr");
    check(cpu_ops_for(CPU_68020)->has_cacr,"68020 cacr");
    check(!cpu_ops_for(CPU_68020)->has_pmmu,"68020 no pmmu");
    check(cpu_ops_for(CPU_68030)->has_pmmu&&cpu_ops_for(CPU_68030)->has_data_cache,"68030 pmmu+dcache");
    check(!cpu_ops_for(CPU_68030)->has_fpu_onchip,"68030 fpu external");
    check(cpu_ops_for(CPU_68040)->has_fpu_onchip,"68040 fpu onchip");
    check(strcmp(cpu_ops_for(CPU_68030)->name,"68030")==0,"name");
    if(fails){printf("CPU_OPS: %d FAIL\n",fails);return 1;}
    printf("CPU_OPS: ALL PASS\n");return 0;
}
