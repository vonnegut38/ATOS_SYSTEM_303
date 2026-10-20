#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "app_lng.h"
static FILE *opened;
static int fail_read,fail_close,opens,closes,mode;
static const char *host_path(const char *p)
{
 if(!strcmp(p,"\\SYSTEM\\LANGUAGE.CFG"))return "languages/LANGUAGE.CFG";
 if(!strcmp(p,"\\SYSTEM\\LANG\\EN.LNG"))return "languages/EN.LNG";
 if(!strcmp(p,"\\SYSTEM\\LANG\\PL.LNG"))return "languages/PL.LNG";
 assert(!"unexpected/non-absolute path");return "";
}
int32_t gui_call(uint32_t call,uint32_t a,uint32_t b,void *p,void *q)
{
 if(call==5){FILE *f=fopen(host_path(p),"rb");uint32_t *st=q;assert(f);fseek(f,0,SEEK_END);st[0]=(uint32_t)ftell(f);st[1]=0;fclose(f);if(mode==1)st[0]=LNG_CAPACITY+1;return 0;}
 if(call==2){assert(!opened);opened=fopen(host_path(p),"rb");opens++;return 2;}
 if(call==3){assert(a==2&&opened);if(fail_read)return -1;if(b>13)b=13;return (int32_t)fread(p,1,b,opened);}
 if(call==4){assert(a==2&&opened);fclose(opened);opened=0;closes++;return fail_close?-1:0;}
 assert(!"unexpected syscall");return -1;
}
static int parse(const char *s){strcpy(lng_data,s);return lng_parse((unsigned)strlen(s));}
int main(void)
{
 unsigned i;char s[256];lng_init();assert(lng_selected==0);
 for(i=0;i<LNG_COUNT;i++)assert(lng_offsets[i]&& !strcmp(lng_text(i),lng_english(i)));
 assert(!lng_load(1));for(i=0;i<LNG_COUNT;i++)assert(lng_offsets[i]);
 assert(strcmp(lng_text(0),lng_english(0)));
 snprintf(s,sizeof s,"ATOS-LNG-1\r\n# comment\r\n%s= leading and trailing \\nline \\n\r\nFUTURE_KEY=value\r\n",lng_key(0));
 assert(!parse(s));assert(!strcmp(lng_text(0)," leading and trailing \nline \n"));assert(!strcmp(lng_text(1),lng_english(1)));
 snprintf(s,sizeof s,"ATOS-LNG-1\n%s=one\n%s=two\n",lng_key(0),lng_key(0));assert(parse(s)<0);assert(!strcmp(lng_text(0),lng_english(0)));
 assert(parse("wrong version\n")<0);assert(parse("ATOS-LNG-1\nMALFORMED\n")<0);assert(parse("ATOS-LNG-1\nKEY=\\x\n")<0);
 assert(parse("ATOS-LNG-1\nKEY=\001\n")<0);assert(parse("ATOS-LNG-1\nKEY=\200\n")<0);
 assert(parse("ATOS-LNG-1\nKEY=\n")<0);
 snprintf(s,sizeof s,"ATOS-LNG-1\n%s=",lng_key(0));{unsigned n=(unsigned)strlen(s);memset(s+n,'x',100);s[n+100]=0;}assert(parse(s)<0);
 memcpy(lng_data,"ATOS-LNG-1\nKEY=a\0b",18);assert(lng_parse(18)<0);assert(lng_parse(LNG_CAPACITY+1)<0);
 fail_read=1;assert(lng_load(0)<0);assert(!opened&&opens==closes);fail_read=0;
 fail_close=1;assert(lng_load(0)<0);assert(!opened&&opens==closes);fail_close=0;
 mode=1;assert(lng_load(0)<0);assert(!opened&&opens==closes);
 puts("Language packs PASS: EN/PL completeness, English fallback, CRLF, escapes, spaces, duplicates, bounds, short reads and handle cleanup");return 0;
}
