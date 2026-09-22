#include "kernel_video.h"
#include "vofa_et4000.h"
#include "kernel_fat.h"
#include "kernel_console.h"
static const kernel_timer_service_t *video_timer;
static uint32_t video_ticks(void){return video_timer->poll();}
static void video_delay(void)
{volatile unsigned i;for(i=0;i<64;i++)__asm__ volatile("nop");}
static int requested(const char *text,uint32_t size)
{
 unsigned i;if(size<4||text[0]!='V'||text[1]!='O'||text[2]!='F'||text[3]!='A')return 0;
 for(i=4;i<size;i++)if(text[i]!=' '&&text[i]!='\r'&&text[i]!='\n'&&text[i]!='\t')return 0;
 return 1;
}
void kvideo_boot(uint8_t mode,uint8_t cpu,uint32_t ram,const kernel_timer_service_t *timer)
{
 static const char name[11]={'V','I','D','E','O',' ',' ',' ','C','F','G'};
 char config[32];uint32_t size;int result;
 static const vofa_io_t io={kvideo_read8,kvideo_write8,video_delay,video_ticks};
 if(kfat_load_system_file(name,config,sizeof(config),&size)!=FAT_FILE_FOUND||!requested(config,size))return;
 kconsole_puts("VIDEO: VOFA REQUESTED / ");(void)cpu;
 /* Boot probe recovery currently supports only the 68000 exception frame.
  * ST-RAM reaching the adapter aperture must never be mistaken for VRAM. */
 if(mode!=2||ram>=VOFA_MEM||!timer||!timer->poll||timer->frequency_hz!=50){
  kconsole_puts("UNSUPPORTED CONFIG - SHIFTER\n");return;
 }
 video_timer=timer;result=vofa_et4000_init(&io);
 if(result==VOFA_OK){
  if(kvideo_activate_vofa((volatile uint8_t *)(uintptr_t)VOFA_MEM))
   kconsole_puts("640x480 MONO / VIEW 640x400\n");
  else kconsole_puts("NO SHADOW RAM - SHIFTER\n");
 }else if(result==VOFA_ABSENT)kconsole_puts("NOT FOUND - SHIFTER\n");
 else if(result==VOFA_CLOCK_ERROR)kconsole_puts("CLOCK CHECK FAILED - SHIFTER\n");
 else if(result==VOFA_MEMORY_ERROR)kconsole_puts("VRAM CHECK FAILED - SHIFTER\n");
 else kconsole_puts("BUS ACCESS FAILED - SHIFTER\n");
}
