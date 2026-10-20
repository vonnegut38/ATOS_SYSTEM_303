/* Micromod PCM player for ATOS/68020+. Engine: Martin Cameron, BSD-3-Clause. */
#include "app_abi.h"
#include "gui_app.h"
#include "micromod.h"
#include "modplay_sound.h"
#include "atos_sound.h"
#define MOD_HIRES_PER_SEC 12288u
#define BLOCK 512u
#define TARGET 4096u
extern long micromod_duration_limit(long limit);
#define GFX_BATCH 18u
#define GFX_WINDOW 20u
#define GFX_TITLE 21u
#define GFX_CONTENT 29u
#define GUI_FILL 2u
#define GUI_TEXT 4u
#define GUI_BEVEL 6u
#define WW 680u
#define MODPLAY_SONGS 8u    /* 4 utwory testowe + 4 z katalogu MOD/ */
#define MODPLAY_VISIBLE 4u  /* tyle wierszy widac; reszta przewijana */
#define WH 480u
#define MAXMOD (2u*1024u*1024u)
static int32_t gfx_pid;
static gui_message_t m;
static gui_draw_t *list;
static uint16_t frame_groups[32];
static unsigned group_start,group_count;
static uint8_t fast_frames,layout_dirty=1,content_dirty=1,panel_dirty;
static unsigned n;
volatile uint32_t modplay_profile[8]; /* paints, ticks, max, items, submits, pump calls/ticks/max */
static int16_t wx=50,wy=42;
static uint32_t pcm[8],rate;
static short mix[BLOCK*2];
static uint8_t samples[BLOCK];
static uint32_t clip_length,clip_filled;
static uint8_t preparing,cache_mode,cache_disabled;
static int32_t cache_fd=-1;
static uint32_t mod_size,cache_hash,cache_expected;
static uint8_t cache_buffer[4096];
static uint32_t fingerprint(const uint8_t *p,uint32_t n,uint32_t h)
{while(n--)h=(h^*p++)*16777619u;return h;}
static uint32_t be32(const uint8_t *p)
{return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
static uint8_t *modbuf;
static uint8_t quit,paused,loaded,prev_left,dirty,owned;
static char status[79];
static uint32_t paint_deadline;
static uint16_t client_w=WW,client_h=WH;
static int32_t load_error;
static uint8_t accelerator;
static uint32_t accelerator_budget;
static unsigned song_index;
static unsigned list_top;     /* pierwszy widoczny wiersz listy */
static const char song_files[MODPLAY_SONGS][48]={"\\SYSTEM\\APP\\HYMN.MOD","\\SYSTEM\\APP\\SIREN.XM","\\SYSTEM\\APP\\REFLECTR.XM","\\SYSTEM\\APP\\ADVENT.S3M","\\SYSTEM\\APP\\DEMON.S3M","\\SYSTEM\\APP\\AWAKE.S3M","\\SYSTEM\\APP\\OXYGEN.XM","\\SYSTEM\\APP\\ALGAR4K.XM"};
static const char song_labels[MODPLAY_SONGS][24]={"HYMN TO AURORA  [MOD]","AA SIREN       [XM]","REFLECTR       [XM]","HIGH ADVENTURE [S3M]","DEMON'S GATE   [S3M]","SPRING AWAKE   [S3M]","OXYGENEMIX     [XM]","ALGAR 4K INTRO [XM]"};
static uint8_t telemetry[208];
static uint32_t load_started,load_ticks;
static uint32_t position_ms,duration_ms;
static unsigned progress_width;
static uint32_t painted_bytes,file_ticks;
static uint8_t is_mod;


/* ---- service plumbing (same as SYSINFO) --------------------------------- */
static gui_message_t pend[16];static unsigned pend_head,pend_tail;
static void pend_push(const gui_message_t *s)
{unsigned j;if(pend_tail-pend_head<16u){gui_message_t *d=&pend[pend_tail++%16];
 for(j=0;j<sizeof(*d);++j)((uint8_t *)d)[j]=((const uint8_t *)s)[j];}}
static int pend_pop(gui_message_t *d)
{unsigned j;if(pend_head==pend_tail)return 0;
 {gui_message_t *s=&pend[pend_head++%16];for(j=0;j<sizeof(*d);++j)((uint8_t *)d)[j]=((const uint8_t *)s)[j];}
 return 1;}
static int32_t gfx(uint32_t op,uint32_t a,uint32_t b,uint32_t c)
{
    if(send((uint32_t)gfx_pid,op,a,b,c))return -1;
    for(;;){
        if(receive(&m,1))return -1;
        if(m.sender==(uint32_t)gfx_pid)return (int32_t)m.v[1];
        pend_push(&m);
    }
}
static void add(uint16_t kind,int16_t x,int16_t y,uint16_t w,uint16_t h,uint16_t color,const char *text)
{
    gui_draw_t *d;uint8_t *p;unsigned i;
    if(n>=256u)return;
    d=&list[n++];p=(uint8_t *)d;
    for(i=0;i<sizeof(*d);++i)p[i]=0;
    d->kind=kind;d->x=x;d->y=y;d->w=w;d->h=h;d->color=color;d->background=1u;
    if(text)copytext(d->text,text,sizeof(d->text));
}
static void flush(void)
{
    if(n==group_start)return;
    if(fast_frames){
        if(group_count<32u)frame_groups[group_count++]=(uint16_t)(n-group_start);
        group_start=n;return;
    }
    modplay_profile[3]+=n;++modplay_profile[4];
    (void)gfx(GFX_BATCH,(uint32_t)(uintptr_t)list,n,0);n=group_start=0;
}
static void sappend(char *d,const char *s){unsigned k=0;while(d[k])++k;copytext(d+k,s,79-k);}
static void snum(char *d,uint32_t v){char t[12];number(t,v);sappend(d,t);}

/* A literal PC-relative path works in the flat PIC application format;
 * an array of absolute string pointers would need loader relocations. */
static int32_t read_fd=-1;
static uint32_t read_total;
static uint8_t reading;
static void read_close(void)
{if(read_fd>=0){(void)gui_call(ATOS_CALL_CLOSE,(uint32_t)read_fd,0,0,0);read_fd=-1;}reading=0;}
static int validate_mod(void)
{
    is_mod=(uint8_t)(mod_size>=1084u && modbuf[1080]=='M' &&
        (modbuf[1081]=='.'||modbuf[1081]=='!') && modbuf[1082]=='K' &&
        (modbuf[1083]=='.'||modbuf[1083]=='!'));
    if(is_mod){load_error=micromod_calculate_mod_file_len((signed char *)modbuf);
        if(load_error<1084||(uint32_t)load_error>mod_size)return -5;}
    return 0;
}
static int load_mod(void)
{
    uint32_t info[2];load_started=hires();
    load_error=gui_call(ATOS_CALL_STAT,0,0,(void *)song_files[song_index],info);
    if(load_error<0)return -1;
    if(info[0]<80u||info[0]>MAXMOD)return -2;
    modbuf=(uint8_t *)(uintptr_t)gui_call(ATOS_CALL_ALLOC,info[0],1,0,0);
    if((int32_t)(uintptr_t)modbuf<=0){load_error=(int32_t)(uintptr_t)modbuf;modbuf=0;return -3;}
    read_fd=gui_call(ATOS_CALL_OPEN,0,0,(void *)song_files[song_index],0);
    if(read_fd<0){load_error=read_fd;return -1;}
    mod_size=info[0];read_total=0;reading=1;is_mod=0;return 0;
}
static void set_status(const char *s){copytext(status,s,sizeof(status));dirty=1;}
static int32_t control(unsigned cmd)
{return gui_call(ATOS_CALL_PCM_CONTROL,cmd,0,0,0);}
static void cache_close(void){if(cache_fd>=0){(void)gui_call(ATOS_CALL_CLOSE,(uint32_t)cache_fd,0,0,0);cache_fd=-1;}}
static int stop_all(void)
{
    int rc=0;cache_close();read_close();preparing=0;
    if(owned){if(accelerator)rc=modsound_close();else rc=control(3);if(!rc)owned=0;}
    return rc;
}
static int submit(const uint8_t *p,unsigned count)
{return gui_call(ATOS_CALL_PCM_SUBMIT,count,0,(void *)p,0);}
static int finish_clip(void)
{return 0;}
static int try_cache(void)
{
    uint32_t info[2],i;uint8_t h[32];const char *magic="MMPCM002";
    if(song_index || cache_disabled||gui_call(ATOS_CALL_STAT,0,0,(void *)"\\SYSTEM\\APP\\HYMN.PCM",info)<0)return 0;
    cache_fd=gui_call(ATOS_CALL_OPEN,0,0,(void *)"\\SYSTEM\\APP\\HYMN.PCM",0);
    if(cache_fd<0)return 0;
    if(gui_call(ATOS_CALL_READ,(uint32_t)cache_fd,32,h,0)!=32){cache_close();return 0;}
    for(i=0;i<8;i++)if(h[i]!=(uint8_t)magic[i]){cache_close();return 0;}
    i=be32(h+12);
    if(!i||i>8u*1024u*1024u||info[0]!=i+32u||be32(h+8)!=rate||
       be32(h+16)!=mod_size||be32(h+20)!=fingerprint(modbuf,mod_size,2166136261u)||
       be32(h+28)!=0x83b5d9au){cache_close();return 0;}
    clip_length=i;cache_expected=be32(h+24);cache_hash=2166136261u;return 1;
}
static void select_song(unsigned which);
static void toggle_pause(void)
{
    int32_t rc;
    if(!loaded||preparing)return;
    if(accelerator){
        if(!owned){
            rc=modsound_open(&accelerator_budget);
            if(rc){set_status("Accelerator unavailable / busy");return;}
            owned=1;rc=modsound_module_begin(mod_size);
            if(rc){stop_all();
                /* Claude, 16.10: the ESP32 answers but has no tracker decoder
                   (no AS_CAP_TRACKER - e.g. Hatari without libxmp).  Do not
                   stop silently on "Tracker firmware required": fall back to
                   the Yamaha/PCM output, exactly as if F1 had been pressed. */
                if(audio_caps()&ATOS_AUDIO_CAP_PCM){
                    accelerator=0;panel_dirty=1;dirty=1;paused=1;
                    if(reading||read_total<mod_size){select_song(song_index);return;}
                    clip_length=clip_filled=0;zero(pcm,sizeof pcm);toggle_pause();return;
                }
                if(read_total<mod_size)loaded=0;
                set_status("Tracker firmware required");return;
            }
            preparing=1;paused=1;clip_filled=0;clip_length=mod_size;
            set_status("Uploading original module...");return;
        }
        rc=modsound_module_control(paused?1u:2u);
        if(rc){set_status("Tracker command failed");return;}
        paused=!paused;set_status(paused?"PAUSED":"PLAYING / ESP32 accelerator");return;
    }
    if(reading){preparing=1;clip_length=mod_size;clip_filled=0;set_status("Reading module...");return;}
    if(!is_mod){set_status("XM/S3M require ESP32 output");return;}
    if(!paused){
        rc=control(2);if(rc){set_status("Audio stop failed");return;}
        paused=1;set_status("PAUSED - SPACE resumes");return;
    }
    if(!owned){
        rc=control(0);
        if(rc<8000){set_status("Audio / Timer A busy");return;}
        owned=1;rate=(uint32_t)rc;
        if(micromod_initialise((signed char *)modbuf,rate)<0){stop_all();set_status("MOD decoder error");return;}
        cache_mode=(uint8_t)try_cache();
        rc=cache_mode?(int32_t)clip_length:micromod_duration_limit(8u*1024u*1024u);
        if(rc<=0){stop_all();set_status("Song too long / invalid loop");return;}
        clip_length=(uint32_t)rc;clip_filled=0;
        rc=gui_call(ATOS_CALL_PCM_CLIP,clip_length,0,0,0);
        if(rc<0){stop_all();set_status("Not enough RAM for PCM");return;}
        preparing=1;set_status(cache_mode?"Loading verified PCM cache...":"Preparing samples in RAM...");return;
    }
    if(control(1)!=0){stop_all();set_status("PCM start failed");return;}
    paused=0;set_status(accelerator?"PLAYING - ESP32 accelerator":"PLAYING - Yamaha YM2149");
}
static void pump(void)
{
    unsigned budget=16;
    if(!preparing)return;
    /* One bounded read/upload per turn. Process input before the next block;
       no full-file read monopolizes the application event loop. */
    if(reading){
        uint32_t count=mod_size-read_total;if(count>4084u)count=4084u;
        int32_t got=gui_call(ATOS_CALL_READ,(uint32_t)read_fd,count,modbuf+read_total,0);
        if(got<=0||(uint32_t)got>count){stop_all();loaded=0;set_status("Module read failed");return;}
        read_total+=(uint32_t)got;
        if(read_total==mod_size){read_close();file_ticks=hires()-load_started;
            if(validate_mod()){stop_all();loaded=0;set_status("Invalid module");return;}}
        if(!accelerator){clip_filled=read_total;
            if(!reading){preparing=0;toggle_pause();}return;}
    }
    if(accelerator){
        /* Yield between packets, including uploads from an already read module. */
        for(unsigned part=0;part<1 && clip_filled<clip_length;part++){
            unsigned count=clip_length-clip_filled;if(count>4084)count=4084;
            if(reading && count>read_total-clip_filled)count=read_total-clip_filled;
            if(!count)break;
            if(modsound_module_data(modbuf+clip_filled,count)){
                stop_all();loaded=0;set_status("Module upload failed");return;
            }
            clip_filled+=count;
        }
        if(clip_filled==clip_length){
            if(modsound_module_commit()){stop_all();loaded=0;set_status("Invalid / unsupported module");return;}
            preparing=0;load_ticks=hires()-load_started;toggle_pause();
        }
        return;
    }
    if(cache_mode){
        budget=4;
        while(clip_filled<clip_length&&budget--){
            uint32_t i=0,count=clip_length-clip_filled;
            if(count>sizeof(cache_buffer))count=sizeof(cache_buffer);
            if(gui_call(ATOS_CALL_READ,(uint32_t)cache_fd,count,cache_buffer,0)!=(int32_t)count)break;
            cache_hash=fingerprint(cache_buffer,count,cache_hash);
            while(i<count){uint32_t part=count-i;if(part>BLOCK)part=BLOCK;
                if(submit(cache_buffer+i,part)!=(int32_t)part)break;
                i+=part;clip_filled+=part;}
            if(i!=count)break;
        }
        if(clip_filled==clip_length&&cache_hash==cache_expected){cache_close();preparing=0;if(finish_clip()){stop_all();set_status("Accelerator commit failed");return;}toggle_pause();return;}
        if(clip_filled==clip_length||budget!=(unsigned)-1){
            /* A truncated/corrupt cache never reaches the DAC. Rebuild with
               Micromod instead; do not retry the same invalid file forever. */
            cache_disabled=1;preparing=0;stop_all();toggle_pause();
        }
        return;
    }
    while(clip_filled<clip_length&&budget--){
        unsigned i,count=clip_length-clip_filled;
        if(count>BLOCK)count=BLOCK;
        zero(mix,sizeof(mix));micromod_get_audio(mix,count);
        for(i=0;i<count;i++){
            int v=((int)mix[i*2]+(int)mix[i*2+1])>>9;
            if(v>127)v=127;else if(v< -128)v=-128;
            samples[i]=(uint8_t)(v+128);
        }
        if(submit(samples,count)!=(int32_t)count){
            preparing=0;stop_all();set_status("PCM upload failed");return;
        }
        clip_filled+=count;
    }
    if(clip_filled==clip_length){preparing=0;if(finish_clip()){stop_all();set_status("Accelerator commit failed");return;}toggle_pause();}
}
static void select_song(unsigned which);
static void select_output(unsigned next)
{
    if(next==accelerator)return;
    if(next && !modsound_available()){set_status("ESP32 Sound Accelerator unavailable");return;}
    if(reading||read_total<mod_size){if(stop_all()){set_status("Cannot release audio - close");return;}
        accelerator=(uint8_t)next;select_song(song_index);return;}
    if(stop_all()){set_status("Cannot release audio - close");return;}
    accelerator=(uint8_t)next;panel_dirty=1;paused=1;clip_length=clip_filled=0;
    zero(pcm,sizeof pcm);toggle_pause();dirty=1;
}
static void select_song(unsigned which)
{
    if(which>=MODPLAY_SONGS || stop_all())return;
    if(modbuf){(void)gui_call(ATOS_CALL_FREE,0,0,modbuf,0);modbuf=0;}
    song_index=which;loaded=0;paused=1;cache_disabled=0;clip_length=clip_filled=0;
    position_ms=duration_ms=0;zero(telemetry,sizeof telemetry);dirty=panel_dirty=1;
    if(load_mod()){set_status("Cannot open selected module");return;}
    loaded=1;toggle_pause();
}
static void box(int x,int y,unsigned w,unsigned h,unsigned c)
{add(GUI_FILL,(int16_t)x,(int16_t)y,(uint16_t)w,(uint16_t)h,(uint16_t)c,0);}
static void label(int x,int y,unsigned ink,unsigned paper,const char *s)
{add(GUI_TEXT,(int16_t)x,(int16_t)y,0,0,(uint16_t)ink,s);if(n)list[n-1].background=(uint16_t)paper;}
static void button(int x,int y,unsigned w,const char *s)
{box(x,y,w,26,15);add(GUI_BEVEL,(int16_t)x,(int16_t)y,(uint16_t)w,26,0,0);label(x+8,y+7,0,15,s);}
static char drawn_text[4][32];
static uint8_t meter_height[32],meter_color[32];
static void time_text(char *d,uint32_t ms)
{unsigned sec=ms/1000u;d[0]=0;snum(d,sec/60u);sappend(d,":");if(sec%60u<10)sappend(d,"0");snum(d,sec%60u);}
/* Lista jest przewijana: zaznaczenie zawsze widoczne, wskazniki u gory/dolu. */
static void list_scroll_into_view(void)
{
    if(song_index<list_top)list_top=song_index;
    if(song_index>=list_top+MODPLAY_VISIBLE)list_top=song_index-MODPLAY_VISIBLE+1u;
    if(list_top+MODPLAY_VISIBLE>MODPLAY_SONGS)list_top=MODPLAY_SONGS>MODPLAY_VISIBLE?MODPLAY_SONGS-MODPLAY_VISIBLE:0u;
}
/* Tekst "1-4/8" o stalej szerokosci - krotszy zostawialby resztki po poprzednim. */
static void list_range_text(char *d)
{
    unsigned k=0,last=list_top+MODPLAY_VISIBLE;
    if(last>MODPLAY_SONGS)last=MODPLAY_SONGS;
    d[0]=0;snum(d,list_top+1u);sappend(d,"-");snum(d,last);sappend(d,"/");snum(d,MODPLAY_SONGS);
    while(d[k])++k;
    while(k<7u)d[k++]=' ';
    d[k]=0;
}
static void transport_play(void)
{if(!loaded){select_song(song_index);return;}toggle_pause();}
static void transport_stop(void)
{
    if(preparing){if(stop_all()){set_status("STOP failed");return;}
        loaded=0;paused=1;position_ms=duration_ms=0;set_status("STOPPED - PLAY to reload");return;}
    if(accelerator&&owned){if(modsound_module_control(0)){set_status("STOP failed");return;}}
    else if(owned && stop_all()){set_status("STOP failed");return;}
    paused=1;position_ms=0;set_status("STOPPED");
}
static void seek_to(uint32_t ms)
{
    if(!accelerator||!owned||preparing||!duration_ms)return;
    if(ms>=duration_ms)ms=duration_ms-1u;
    if(modsound_module_seek(ms)){set_status("Seek unavailable on this backend");return;}
    if(!modsound_module_status(telemetry))position_ms=as_get32(telemetry+32);
    dirty=1;
}
static void seek_step(int forward)
{
    if(!accelerator||!owned||preparing)return;
    if(modsound_module_status(telemetry))return;
    position_ms=as_get32(telemetry+32);duration_ms=as_get32(telemetry+36);
    seek_to(forward?(position_ms>0x7fffffff-10000u?0x7fffffff:position_ms+10000u):(position_ms>10000u?position_ms-10000u:0));
}
static void transport_button(unsigned id)
{
    if(id==0)select_song((song_index+3u)%MODPLAY_SONGS);
    else if(id==1)seek_step(0);
    else if(id==2)transport_play();
    else if(id==3)transport_stop();
    else if(id==4)seek_step(1);
    else if(id==5)select_song((song_index+1u)%MODPLAY_SONGS);
}
static int text_changed(unsigned slot,int x,int y,unsigned ink,unsigned paper,const char *s)
{
    char t[32];unsigned a=0,b=0;copytext(t,s,sizeof t);
    if(!layout_dirty && !(panel_dirty&&slot>=16)){while(t[a]&&t[a]==drawn_text[slot][a])++a;
        if(t[a]==drawn_text[slot][a])return 0;}
    while(drawn_text[slot][b])++b;
    copytext(drawn_text[slot],t,32);a=0;while(t[a])++a;
    while(a<b)t[a++]=' ';
    t[a]=0;label(x,y,ink,paper,t);return 1;
}
static void meters(void)
{
    for(unsigned i=0;i<32;i++){
        unsigned v=accelerator&&!paused&&!preparing?telemetry[112+i]:0;if(v>64)v=64;
        unsigned h=v*60u/64u,old=meter_height[i],c=v>56?11:19;int x=20+(int)i*20;
        if(layout_dirty){box(x,346,14,60,0);if(h)box(x,406-(int)h,14,h,c);}
        else{
            if(h<old)box(x,406-(int)old,14,old-h,0);
            if(h&&c!=meter_color[i])box(x,406-(int)h,14,h,c);
            else if(h>old)box(x,406-(int)h,14,h-old,c);
        }
        meter_height[i]=(uint8_t)h;meter_color[i]=(uint8_t)c;
        if((i&7u)==7u)flush();
    }
}
static void paint_inner(void)
{
    char s[79],z[79];unsigned i;
    if(layout_dirty){
        box(0,0,client_w,client_h,14);box(6,6,668,30,12);
        label(16,14,19,12,"ATOS TRACKER");label(290,14,1,12,"MOD / S3M / XM   -   LIBXMP");
        label(16,44,0,14,"MODULE LIBRARY");label(268,44,0,14,"NOW PLAYING");
        box(12,58,240,96,0);
        list_scroll_into_view();
        for(i=0;i<MODPLAY_VISIBLE;i++){unsigned e=list_top+i;if(e>=MODPLAY_SONGS)break;
            box(16,62+(int)i*22,230,20,e==song_index?16:0);
            label(20,66+(int)i*22,e==song_index?19:15,e==song_index?16:0,song_labels[e]);}
        box(16,59,230,3,list_top?15:14);                                     /* wiecej w gore */
        box(16,151,230,3,(list_top+MODPLAY_VISIBLE<MODPLAY_SONGS)?15:14);     /* wiecej w dol */
        {char r[16];list_range_text(r);label(180,44,0,14,r);}
        button(546,66,114,"CLOSE [ESC]");box(268,106,394,46,12);
        label(20,166,0,14,"PLAYBACK");
        box(20,244,640,30,12);add(GUI_BEVEL,20,244,640,30,0,0);
        box(24,248,632,22,0);
        label(350,280,0,14,"REV / FF: 10 SECONDS");
        label(16,320,0,14,"CHANNEL ACTIVITY / 32 VOICES");
        box(12,340,656,72,12);
        button(16,422,154,accelerator?"Yamaha [F1]":"[Yamaha] F1");
        button(182,422,338,accelerator?"[ESP32 Sound Accelerator]":"ESP32 Sound Accelerator [F2]");
        label(530,430,0,14,"< > TRACK");flush();
    }
    if(panel_dirty&&!layout_dirty){
        list_scroll_into_view();
        for(i=0;i<MODPLAY_VISIBLE;i++){unsigned e=list_top+i;if(e>=MODPLAY_SONGS)break;
            box(16,62+(int)i*22,230,20,e==song_index?16:0);
            label(20,66+(int)i*22,e==song_index?19:15,e==song_index?16:0,song_labels[e]);}
        box(16,59,230,3,list_top?15:14);                                     /* wiecej w gore */
        box(16,151,230,3,(list_top+MODPLAY_VISIBLE<MODPLAY_SONGS)?15:14);     /* wiecej w dol */
        {char r[16];list_range_text(r);label(180,44,0,14,r);}flush();
        button(16,422,154,accelerator?"Yamaha [F1]":"[Yamaha] F1");
        button(182,422,338,accelerator?"[ESP32 Sound Accelerator]":"ESP32 Sound Accelerator [F2]");flush();
    }
    if(accelerator && owned && !preparing){
        if(modsound_module_status(telemetry))set_status("Tracker disconnected");
        else{position_ms=as_get32(telemetry+32);duration_ms=as_get32(telemetry+36);}
    }else if(!accelerator&&owned&&!preparing){
        if(gui_call(ATOS_CALL_PCM_STATE,0,0,pcm,0)==0&&pcm[6]){
            duration_ms=pcm[7]/pcm[6]*1000u+(pcm[7]%pcm[6])*1000u/pcm[6];
            uint32_t at=pcm[7]-pcm[2];position_ms=at/pcm[6]*1000u+(at%pcm[6])*1000u/pcm[6];}
    }
    if(dirty||panel_dirty||layout_dirty){
        button(20,190,98,"PREV");button(126,190,98,"REV");
        button(232,190,98,paused?"PLAY":"PAUSE");button(338,190,98,"STOP");
        button(444,190,98,"FF");button(550,190,98,"NEXT");flush();
    }
    if(preparing){s[0]=0;sappend(s,"LOADING ");snum(s,clip_length?clip_filled*100u/clip_length:0);sappend(s,"%   ");snum(s,clip_length/1024u);sappend(s," KB");}
    else if(accelerator){
        /* Tytul z libxmp; gdy modul go nie ma (albo jest nie-ASCII), panel byl
           pusty - wtedy pokazujemy nazwe pliku. */
        const char *nm=(const char *)telemetry+48;unsigned k=0;int ok=0;
        while(k<32u&&nm[k]){unsigned char c=(unsigned char)nm[k];
            if(c<32u||c>126u){ok=0;break;}
            if(c!=' ')ok=1;
            ++k;}
        if(ok)copytext(s,(char *)nm,32);
        else{const char *f=song_files[song_index];unsigned j=0;
            while(f[j])++j;
            while(j>0u&&f[j-1u]!=(char)'\\')--j;
            copytext(s,f+j,32);}
    }
    else copytext(s,"YAMAHA / PCM 12288 Hz",sizeof s);
    (void)text_changed(0,276,114,19,12,s);flush();
    s[0]=0;
    if(accelerator&&!preparing){snum(s,as_get32(telemetry+8));sappend(s," CHANNELS / BPM ");snum(s,as_get32(telemetry+24));}
    else copytext(s,status,32);
    (void)text_changed(1,276,133,1,12,s);flush();
    {
        uint32_t at=preparing?0:position_ms,total=preparing?0:duration_ms;
        if(at>total)at=total;
        /* Whole-second progress is enough for 632 pixels and avoids 64-bit
           division in the hot draw loop. Millisecond target on mouse clicks. */
        unsigned fill=total>=1000u?(unsigned)((at/1000u)*632u/(total/1000u)):(total?at*632u/total:0u);
        if(fill>632u)fill=632u;
        if(layout_dirty||panel_dirty){box(24,248,632,22,0);if(fill)box(24,248,fill,22,19);}
        else if(fill>progress_width)box(24+(int)progress_width,248,fill-progress_width,22,19);
        else if(fill<progress_width)box(24+(int)fill,248,progress_width-fill,22,0);
        progress_width=fill;flush();
        time_text(s,at);sappend(s," / ");time_text(z,total);sappend(s,z);
        (void)text_changed(2,24,280,0,14,s);flush();
        (void)text_changed(3,24,300,0,14,preparing?"LOADING - PLEASE WAIT":(accelerator&&modsound_module_can_seek()?"CLICK BAR TO SEEK":"SEEK: ESP32 BACKEND REQUIRED"));flush();
    }
    if(!preparing||dirty)meters();
    if(dirty){box(12,452,656,14,14);label(18,454,0,14,status);
        if(accelerator&&!preparing){s[0]=0;sappend(s,"START ");snum(s,load_ticks*1000u/MOD_HIRES_PER_SEC);sappend(s," ms / ");snum(s,mod_size/1024u);sappend(s," KB");label(360,454,0,14,s);}}
    flush();if(content_dirty){(void)gfx(GFX_CONTENT,client_h,0,0);content_dirty=0;}
    layout_dirty=panel_dirty=0;
    dirty=0;painted_bytes=clip_filled;paint_deadline=hires()+MOD_HIRES_PER_SEC/25u;
}

static void paint(void)
{uint32_t t=hires();paint_inner();
 if(fast_frames&&n){modplay_profile[3]+=n;++modplay_profile[4];
     if(gui_call(ATOS_CALL_GUI_FRAME,n,group_count,list,frame_groups)<0){fast_frames=0;dirty=1;}
     n=group_start=group_count=0;}
 fast_frames=1;t=hires()-t;++modplay_profile[0];modplay_profile[1]+=t;if(t>modplay_profile[2])modplay_profile[2]=t;}

static void event(void)
{
    if(m.sender)return;
    if(m.v[0]==0x4b4b4559u&&((m.v[1]>>24)&255u)==1u){
        unsigned scan=m.v[1]&127u;
        if(scan==1u)quit=1;
        else if(scan==0x39u)transport_play();
        else if(scan==0x3bu)select_output(0);
        else if(scan==0x3cu)select_output(1);
        else if(scan==0x4bu)select_song((song_index+3u)%MODPLAY_SONGS);
        else if(scan==0x4du)select_song((song_index+1u)%MODPLAY_SONGS);
        else if(scan==0x3du||scan==0x1fu)transport_stop();
        else if(scan==0x48u)seek_step(0);
        else if(scan==0x50u)seek_step(1);
    }else if(m.v[0]==0x4b4d4f55u){
        int x=(int)(uint16_t)(m.v[1]>>16)-wx;
        int y=(int)(uint16_t)m.v[1]-wy-16;
        unsigned left=(m.v[2]>>24)&1u;
        if(left&&!prev_left){
            if(y>=62&&y<150&&x>=16&&x<250)select_song((unsigned)(y-62)/22u);
            else if(y>=66&&y<92&&x>=546&&x<660)quit=1;
            else if(y>=190&&y<216&&x>=20&&x<648){
                unsigned bx=(unsigned)(x-20);if(bx%106u<98u)transport_button(bx/106u);
            }else if(y>=244&&y<274&&x>=24&&x<=656){
                unsigned frac=(unsigned)(x-24);
                seek_to((duration_ms/632u)*frac+(duration_ms%632u)*frac/632u);
            }else if(y>=422&&y<450){if(x>=16&&x<170)select_output(0);else if(x>=182&&x<520)select_output(1);}
        }
        prev_left=(uint8_t)left;
    }else if(m.v[0]==0x57434c53u)quit=1;
    else if(m.v[0]==0x574d4f56u){
        uint16_t w=(uint16_t)(m.v[2]>>16),h=(uint16_t)m.v[2];
        wx=(int16_t)(m.v[1]>>16);wy=(int16_t)m.v[1];
        if(w!=client_w||h!=client_h+16u){client_w=w;client_h=h>16u?h-16u:0u;dirty=layout_dirty=content_dirty=1;}
    }
}
int app_main(void)
{
    int rc;
    gfx_pid=lookup("SYSTEM.GFX");if(gfx_pid<0)return 1;
    list=(gui_draw_t *)(uintptr_t)gui_call(ATOS_CALL_ALLOC,256u*sizeof(*list),1,0,0);
    if((int32_t)(uintptr_t)list<=0)return 1;
    (void)gfx(GFX_WINDOW,((uint32_t)(uint16_t)wx<<16)|(uint16_t)wy,((uint32_t)WW<<16)|(WH+16u),0);
    (void)gfx(GFX_TITLE,(uint32_t)(uintptr_t)"MODPLAY",0,0);
    accelerator=(uint8_t)modsound_available();paused=1;
    set_status("Opening tracker...");paint();
    /* The first batch creates the WM surface. Focus only afterwards, without
       taking the desktop's global keyboard registration. */
    (void)gfx(32u,0,0,(uint32_t)gui_call(ATOS_CALL_GETPID,0,0,0,0));
    rc=load_mod();paused=1;
    if(rc==0){loaded=1;if(accelerator || (audio_caps()&ATOS_AUDIO_CAP_PCM))toggle_pause();else set_status("PCM kernel required");}
    else{
        char s[79];s[0]=0;
        if(rc==-1)sappend(s,"HYMN.MOD file error ");
        else if(rc==-2)sappend(s,"MOD size invalid ");
        else if(rc==-3)sappend(s,"Allocation failed ");
        else if(rc==-4)sappend(s,"MOD read error ");
        else sappend(s,"Unsupported / damaged MOD ");
        snum(s,(uint32_t)(load_error<0?-load_error:load_error));set_status(s);
    }
    paint();
    while(!quit){
        unsigned budget=0;
        while(budget++<16u&&(pend_pop(&m)||receive(&m,0)==0)){event();if(quit)break;}
        if(quit)break;
        {uint32_t t=hires();pump();t=hires()-t;++modplay_profile[5];modplay_profile[6]+=t;if(t>modplay_profile[7])modplay_profile[7]=t;}
        if(dirty || (preparing ? (clip_filled-painted_bytes>=clip_length/20u) : (!paused&&(int32_t)(hires()-paint_deadline)>=0)))paint();
        if(preparing)(void)gui_call(10,0,0,0,0);else pause_tick();
    }
    stop_all();
    if(modbuf)(void)gui_call(ATOS_CALL_FREE,0,0,modbuf,0);
    (void)gui_call(ATOS_CALL_FREE,0,0,list,0);
    return 0;
}
