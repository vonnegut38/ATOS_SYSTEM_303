#ifndef GUI_APP_H
#define GUI_APP_H
#include <stdint.h>
#include "gui_draw.h"
typedef struct {uint32_t sender,v[4];} gui_message_t;
typedef struct {char path[64],args[64];uint32_t pid;int32_t status;} gui_spawn_t;
extern int32_t gui_call(uint32_t,uint32_t,uint32_t,void *,void *);
static inline int32_t ticks(void){return gui_call(10,0,0,0,0);}
static inline void pause_tick(void){gui_call(11,1,0,0,0);}
static inline int32_t lookup(const char *s){return gui_call(40,0,0,(void *)s,0);}
static inline int32_t receive(gui_message_t *m,int wait){return gui_call(wait?38:37,0,0,m,0);}
static inline int32_t send(uint32_t pid,uint32_t a,uint32_t b,uint32_t c,uint32_t d){uint32_t v[4]={a,b,c,d};return gui_call(36,pid,0,v,0);}
static inline void copytext(char *d,const char *s,unsigned n){unsigned i;for(i=0;i+1<n&&s[i];i++)d[i]=s[i];d[i]=0;}
static inline void zero(void *p,unsigned n){uint8_t *b=p;while(n--)*b++=0;}
static inline void number(char *s,uint32_t n){
 static const uint32_t places[10]={1000000000u,100000000u,10000000u,1000000u,100000u,10000u,1000u,100u,10u,1u};
 unsigned i,out=0;for(i=0;i<10;i++){unsigned digit=0;while(n>=places[i]){n-=places[i];digit++;}
 if(digit||out||i==9)s[out++]=(char)('0'+digit);}s[out]=0;}

enum {WM_HELLO=0x1000,WM_COUNT,WM_TEXT0,WM_TEXT1,WM_TEXT2,WM_INPUT=0x1100,WM_QUIT,WM_FOCUS};
#endif
