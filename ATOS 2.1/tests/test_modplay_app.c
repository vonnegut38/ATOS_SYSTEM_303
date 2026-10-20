/* Actual player event/refill code, bounded OS stream double. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "atos_sound.h"
#include "../system/modplay.c"
static as_engine *engine;
static int unavailable,device_busy,lose_reply;
static unsigned sound_calls;
static uint32_t queued,active,accepted,capacity;
static int busy,cache_test;
static uint8_t fake_cache[160];
static unsigned cache_cursor;
static unsigned varied;
static uint8_t streamed[8193];
static unsigned stream_at,stream_reads,stream_closed,draw_frames;
static int stream_fail;
static gui_draw_t test_draws[256];
int32_t gui_call(uint32_t op,uint32_t a,uint32_t b,void *p,void *q)
{
 (void)b;(void)q;
 if(op==36){assert(((uint32_t *)p)[0]==GFX_CONTENT);return 0;}
 if(op==38){gui_message_t *msg=p;msg->sender=(uint32_t)gfx_pid;msg->v[1]=0;return 0;}
 if(op==ATOS_CALL_GUI_FRAME){assert(a<256 && gui_frame_counts_valid(a,b,q));++draw_frames;return 0;}
 if(op==ATOS_CALL_READ&&a==9){++stream_reads;assert(b<=4084);
   if(stream_fail)return -1;
   unsigned count=b;if(stream_at==0 && count>17)count=17;
   assert(stream_at+count<=sizeof streamed);memcpy(p,streamed+stream_at,count);stream_at+=count;return (int32_t)count;}
 if(op==ATOS_CALL_CLOSE&&a==9){++stream_closed;return 0;}
 if(op==ATOS_CALL_SOUND_INFO){((uint32_t *)p)[0]=!unavailable;return 0;}
 if(op==ATOS_CALL_SOUND_XCHG){
  if(device_busy)return AS_E_BUSY;
  ++sound_calls;size_t n=as_exchange(engine,p,a,q,b);
  if(lose_reply){lose_reply=0;return -0x402;}return (int32_t)n;
 }
 if(op==5){if(cache_test){((uint32_t *)q)[0]=sizeof(fake_cache);return 0;}return -0x241;}
 if(op==2&&cache_test){cache_cursor=0;return 3;}
 if(op==3&&cache_test){assert(cache_cursor+b<=sizeof(fake_cache));memcpy(p,fake_cache+cache_cursor,b);cache_cursor+=b;return (int32_t)b;}
 if(op==4)return 0;
 if(op==67)return 12345;
 if(op==ATOS_CALL_PCM_CLIP){capacity=a;return 0;}
 if(op==ATOS_CALL_PCM_CONTROL){
  if(busy)return -1;
  if(a==0){queued=0;return 12288;}
  if(a==1){active=1;return 0;}
  if(a==2){active=0;return 0;}
  if(a==3){active=queued=0;return 0;}
 }
 if(op==ATOS_CALL_PCM_SUBMIT){
  assert(a<=512&&queued+a<=capacity);queued+=a;accepted+=a;
  for(unsigned i=0;i<a;i++)if(((uint8_t *)p)[i]!=128)varied++;
  return (int32_t)a;
 }
 if(op==ATOS_CALL_PCM_STATE){uint32_t *s=p;zero(s,32);s[2]=queued;s[3]=capacity-queued;return 0;}
 assert(!"unexpected syscall");return -1;
}
static uint8_t data[2108+64];
static void put32(uint8_t *p,uint32_t x){p[0]=x>>24;p[1]=x>>16;p[2]=x>>8;p[3]=x;}
int main(void)
{
 memset(data,0,sizeof data);memcpy(data+1080,"M.K.",4);data[950]=1;
 data[43]=32;data[45]=64;data[49]=32; /* 64-byte sample and loop */
 data[1084]=1;data[1085]=172;data[1086]=16;
 for(unsigned i=0;i<64;i++)data[2108+i]=i<32?100:(uint8_t)-100;
 modbuf=data;is_mod=1;loaded=paused=1;toggle_pause();
 assert(owned&&paused&&preparing&&!active);
 while(preparing)pump();
 assert(!paused&&active&&queued==capacity&&accepted==capacity&&varied>0);
 unsigned stable=accepted;pump();assert(accepted==stable); /* IRQ owns the whole clip */
 m.sender=0;m.v[0]=0x4b4b4559;m.v[1]=0x02000039;event();assert(!paused);
 m.v[1]=0x01000039;event();assert(paused&&!active&&queued==capacity);
 unsigned old=accepted;pump();assert(accepted==old);
 event();assert(!paused&&active&&accepted==old); /* resume unread PCM */
 m.v[1]=0x01000001;event();assert(quit);stop_all();assert(!owned&&!active&&!queued);
 paused=1;busy=1;toggle_pause();assert(!owned&&paused);
 busy=0;cache_test=1;cache_disabled=0;mod_size=sizeof(data);preparing=0;
 memcpy(fake_cache,"MMPCM002",8);put32(fake_cache+8,12288);put32(fake_cache+12,128);
 put32(fake_cache+16,sizeof(data));put32(fake_cache+20,fingerprint(data,sizeof(data),2166136261u));
 put32(fake_cache+24,fingerprint(fake_cache+32,128,2166136261u));put32(fake_cache+28,0x83b5d9a);
 toggle_pause();assert(preparing&&cache_mode);pump();assert(!preparing&&!paused&&active);stop_all();paused=1;
 fake_cache[32]^=1;toggle_pause();assert(cache_mode&&preparing);pump();
 assert(cache_disabled&&preparing&&!cache_mode&&!active); /* corrupt payload rebuilt, never played */
 stop_all();preparing=0;cache_test=0;
 assert(load_mod()==-1&&load_error==-0x241);

 as_config cfg={.output_rate=44100,.sample_budget=AS_DEFAULT_BUDGET,.session_seed=1};
 engine=as_create(&cfg);assert(engine);cache_test=0;modbuf=data;mod_size=sizeof data;is_mod=1;loaded=1;paused=1;preparing=0;quit=0;
 accelerator=1;toggle_pause();assert(preparing&&owned);pump();assert(!preparing&&!paused);
 int16_t output[512];as_mix(engine,output,256);unsigned audible=0;for(unsigned i=0;i<512;i++)audible+=output[i]!=0;assert(audible);
 assert(!modsound_module_status(telemetry));assert(as_get32(telemetry+8)==4);
 toggle_pause();assert(paused);as_mix(engine,output,256);for(unsigned i=0;i<512;i++)assert(!output[i]);
 toggle_pause();assert(!paused);as_mix(engine,output,256);assert(output[0]!=0);
 /* Mouse transport + seek are real decoder operations, preserving pause. */
 toggle_pause();assert(paused);assert(!modsound_module_status(telemetry));
 duration_ms=as_get32(telemetry+36);assert(duration_ms>1000);
 m.sender=0;m.v[0]=0x4b4d4f55;m.v[1]=((uint32_t)(wx+340)<<16)|(uint16_t)(wy+16+254);m.v[2]=1u<<24;prev_left=0;event();
 assert(paused);assert(position_ms>duration_ms/2-100&&position_ms<duration_ms/2+100);
 seek_step(0);assert(position_ms==0&&paused);seek_step(1);assert(position_ms>0&&paused);
 transport_stop();assert(paused);assert(!modsound_module_status(telemetry));assert(as_get32(telemetry+32)==0&&as_get32(telemetry+36)==duration_ms);
 /* PLAY and STOP button hitboxes, with release between clicks. */
 m.v[1]=((uint32_t)(wx+270)<<16)|(uint16_t)(wy+16+202);prev_left=0;event();assert(!paused);
 m.v[1]=((uint32_t)(wx+370)<<16)|(uint16_t)(wy+16+202);prev_left=0;event();assert(paused);

 assert(stop_all()==0);as_mix(engine,output,256);assert(!output[0]);
 device_busy=1;paused=1;toggle_pause();assert(!owned&&!preparing);device_busy=0;toggle_pause();pump();assert(!paused);
 assert(stop_all()==0);
 /* Same renderer: unchanged telemetry produces no draw; one frame validates
    every grouped damage list before the kernel receives it. */
 list=test_draws;gfx_pid=2;fast_frames=1;dirty=layout_dirty=content_dirty=1;paused=1;
 paint();unsigned frames_before=draw_frames;assert(frames_before==1);
 paint();assert(draw_frames==frames_before);
 /* Pixel-exact delta meter versus full redraw across growth, fall, color
    threshold and pause. Only changed pixels are ever in the draw list. */
 uint8_t pixels[32][60];memset(pixels,0,sizeof pixels);memset(meter_height,0,32);memset(meter_color,0,32);
 for(unsigned step=0;step<90;step++){
   paused=(step%11)==0;dirty=0;n=group_start=group_count=0;
   for(unsigned c=0;c<32;c++)telemetry[112+c]=(uint8_t)((step*17+c*11)%65);
   meters();
   for(unsigned j=0;j<n;j++){gui_draw_t *d=&list[j];assert(d->kind==GUI_FILL && d->w==14);
     unsigned c=(unsigned)(d->x-20)/20;assert(c<32 && d->y>=346 && d->y+d->h<=406);
     for(unsigned y=0;y<d->h;y++)pixels[c][d->y-346+y]=(uint8_t)d->color;}
   for(unsigned c=0;c<32;c++){unsigned v=paused?0:telemetry[112+c],h=v*60/64;
     for(unsigned y=0;y<60;y++)assert(pixels[c][y]==(y>=60-h?(v>56?11:19):0));}
   n=group_start=group_count=0;meters();assert(n==0);
 }
 n=group_start=group_count=0;
 /* File read/upload pipeline, including an unaligned short read and cancel. */
 memcpy(streamed,data,sizeof data);modbuf=streamed;mod_size=sizeof streamed;
 reading=1;read_fd=9;read_total=stream_at=stream_reads=stream_closed=0;paused=loaded=1;
 toggle_pause();assert(preparing&&owned);pump();assert(read_total==17&&clip_filled==17&&stream_reads==1);
 while(preparing)pump();assert(!reading&&stream_closed==1&&!paused&&clip_filled==sizeof streamed);
 assert(stop_all()==0);paused=1;reading=1;read_fd=9;read_total=stream_at=0;toggle_pause();pump();
 transport_stop();assert(!preparing&&!reading&&!owned&&!loaded&&paused);
 loaded=paused=1;reading=1;read_fd=9;read_total=stream_at=0;toggle_pause();pump();
 m.sender=0;m.v[0]=0x4b4b4559;m.v[1]=0x01000001;event();assert(quit);assert(stop_all()==0&&!reading&&read_fd==-1);quit=0;
 paused=1;reading=1;read_fd=9;read_total=stream_at=0;stream_fail=1;toggle_pause();pump();
 assert(!loaded&&!owned&&!preparing&&!reading);stream_fail=0;
 modbuf=data;mod_size=sizeof data;loaded=paused=1;toggle_pause();assert(preparing);
 lose_reply=1;pump();unsigned calls=sound_calls;assert(stop_all()!=0 && sound_calls==calls);
 as_destroy(engine);
 puts("PASS MODPLAY: Yamaha regression + tracker upload/play/pause/resume/busy/lost reply");
}
