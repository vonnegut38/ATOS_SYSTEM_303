#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../system/setup_app.c"
static FILE *opened;
static char setting[3]={'E','N','\n'};
static unsigned writes,reply_pid,reply_op;
static int bad_pack,save_error,setting_fd;
static const char *pack_path(const char *p)
{if(!strcmp(p,"\\SYSTEM\\LANG\\PL.LNG"))return "languages/PL.LNG";
 assert(!strcmp(p,"\\SYSTEM\\LANG\\EN.LNG"));return "languages/EN.LNG";}
int32_t gui_call(uint32_t call,uint32_t a,uint32_t b,void *p,void *q)
{
 if(call==5){uint32_t *st=q;st[1]=0;if(!strcmp(p,"\\SYSTEM\\LANGUAGE.CFG")){st[0]=3;return 0;}
  if(bad_pack)return -1;
  FILE *f=fopen(pack_path(p),"rb");assert(f);fseek(f,0,SEEK_END);st[0]=(uint32_t)ftell(f);fclose(f);return 0;}
 if(call==2){setting_fd=!strcmp(p,"\\SYSTEM\\LANGUAGE.CFG");if(!setting_fd){opened=fopen(pack_path(p),"rb");assert(opened);}return 1;}
 if(call==3){if(setting_fd){assert(b==3);memcpy(p,setting,3);return 3;}return (int32_t)fread(p,1,b,opened);}
 if(call==4){if(opened)fclose(opened);opened=0;return 0;}
 if(call==59){writes++;if(save_error)return -1;memcpy(setting,a?"PL\n":"EN\n",3);return 0;}
 if(call==36){uint32_t *v=p;reply_pid=a;reply_op=v[0];return 0;}
 if(call==38){gui_message_t *m=p;memset(m,0,sizeof(*m));m->sender=reply_pid;m->v[0]=reply_op|0x80000000u;return 0;}
 assert(!"unexpected syscall");return -1;
}
static int has(const char *s)
{char row[81];unsigned x,y;for(y=0;y<25;y++){for(x=0;x<80;x++)row[x]=(char)screen.cells[y*80+x];row[80]=0;if(strstr(row,s))return 1;}return 0;}
int main(void)
{gfx=gui=1;lng_init();choose(lng_selected);assert(!selected&&pack_ok);assert(!render()&&has("Application language:"));
 key(0x50);assert(selected&&pack_ok);assert(!render()&&has("Jezyk aplikacji:"));key(28);assert(writes==1&&status_id==SET_SAVED&&!memcmp(setting,"PL\n",3));
 lng_init();assert(lng_selected==1);key(0x48);save_error=1;key(28);assert(status_id==SET_ERROR&&!memcmp(setting,"PL\n",3));save_error=0;
 key(28);assert(status_id==SET_SAVED&&!memcmp(setting,"EN\n",3));lng_init();assert(!lng_selected);
 bad_pack=1;choose(1);assert(!pack_ok);unsigned before=writes;key(28);assert(status_id==SET_PACK_ERROR&&writes==before);
 key(1);assert(quit&&writes==before);memcpy(setting,"??\n",3);lng_init();assert(!lng_selected);
 puts("SETUP PASS: EN/PL preview and selection, persisted preference, failed save, missing pack, cancel and invalid-config fallback");return 0;}
