#include <stdio.h>
#include <string.h>
#include "lang.h"
static int fails;
static void eq(const char *a,const char *b,const char *what)
{if(!a||!b||strcmp(a,b)){printf("FAIL %s: '%s' != '%s'\n",what,a?a:"(null)",b?b:"(null)");++fails;}}
int main(void)
{
    const char *src="# ATOS language test\n@code=PL\nBIOS_TITLE=ATARI BIOS PL\nMACHINE=MASZYNA\nUNKNOWN_KEY=x\n";
    int applied=0,got;
    lang_init();
    eq(lang_str(LANG_ID_BIOS_TITLE),"ATARI BIOS 0.03","english default title");
    eq(lang_code(),"EN","default code");
    got=lang_parse(src,(uint32_t)strlen(src),&applied);
    eq(lang_str(LANG_ID_BIOS_TITLE),"ATARI BIOS PL","override title");
    eq(lang_str(LANG_ID_MACHINE),"MASZYNA","override machine");
    eq(lang_str(LANG_ID_CPU),"CPU","english fallback cpu");
    eq(lang_code(),"PL","code applied");
    if(got!=2||applied!=2){printf("FAIL count got=%d applied=%d (want 2)\n",got,applied);++fails;}
    if(!lang_is_overridden(LANG_ID_BIOS_TITLE)||lang_is_overridden(LANG_ID_CPU)){printf("FAIL override flags\n");++fails;}
    if(fails){printf("LANG: %d FAIL\n",fails);return 1;}
    printf("LANG: ALL PASS\n");return 0;
}
