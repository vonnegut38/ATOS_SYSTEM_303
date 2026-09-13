#include "gui_app.h"
static uint32_t manager;
static int reliable(uint32_t op,uint32_t a,uint32_t b,uint32_t c){int32_t r;unsigned tries=0;
 do{r=send(manager,op,a,b,c);if(r!=-0x3c2)return r;pause_tick();}while(++tries<200);return -1;}
#if APP_KIND == 2
static char note[36];
static int publish(void){unsigned i,j;uint32_t v[3];for(i=0;i<3;i++){
 for(j=0;j<12;j++)((uint8_t *)v)[j]=(uint8_t)note[i*12+j];
 if(reliable(WM_TEXT0+i,v[0],v[1],v[2]))return -1;}return 0;}
#endif
int app_main(void)
{
    gui_message_t m;int32_t pid=lookup("SYSTEM.WM");
    if(pid<=0)return 1;
    manager=(uint32_t)pid;
    if(reliable(WM_HELLO,APP_KIND,0,0))return 2;
    do{if(receive(&m,1))return 3;if(m.sender==manager&&m.v[0]==WM_QUIT)return 0;}while(m.sender!=manager||m.v[0]!=(WM_HELLO|0x80000000u));
    if(m.v[1]!=0)return 4;
#if APP_KIND == 1
    {uint32_t last=(uint32_t)ticks(),count=0;int paused=0,dirty=1;
    for(;;){uint32_t now;unsigned n;
        for(n=0;n<4&&receive(&m,0)==0;n++)if(m.sender==manager){
            if(m.v[0]==WM_QUIT)return 0;
            if(m.v[0]==WM_INPUT&&((m.v[1]&127)==0x1c||m.v[2]==' ')){paused=!paused;dirty=1;}
        }
        now=(uint32_t)ticks();
        if(now-last>=50){if(!paused){count++;dirty=1;}last=now;}
        if(dirty){if(reliable(WM_COUNT,count,(uint32_t)paused,0))return 5;dirty=0;}
        pause_tick();
    }}
#else
    {unsigned length=0;if(publish())return 5;
    for(;;){unsigned scan;char c;
        if(receive(&m,1))return 6;
        if(m.sender!=manager)continue;
        if(m.v[0]==WM_QUIT)return 0;
        if(m.v[0]!=WM_INPUT)continue;
        scan=m.v[1]&127;c=(char)m.v[2];
        if(scan==0x0e){if(length)note[--length]=0;}
        else if(c>=32&&c<=126&&length<35){note[length++]=c;note[length]=0;}
        else continue;
        if(publish())return 7;
    }}
#endif
}
