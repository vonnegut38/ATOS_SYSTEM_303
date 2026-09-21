#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "../system/kernel_video_boot.c"
static const char *contents;static int probes,activations,result_code,alloc_ok=1;static char logtext[256];
static uint32_t tick(void){return 0;}
enum fat_file_state kfat_load_system_file(const char name[11],void *out,uint32_t cap,uint32_t *size)
{assert(!memcmp(name,"VIDEO   CFG",11));if(!contents)return FAT_FILE_NOT_FOUND;*size=(uint32_t)strlen(contents);if(*size>cap)return FAT_FILE_IO_FAILED;memcpy(out,contents,*size);return FAT_FILE_FOUND;}
void kconsole_puts(const char *s){strcat(logtext,s);}
int kvideo_read8(uint32_t a,uint8_t *b){(void)a;(void)b;assert(0);return 0;}
int kvideo_write8(uint32_t a,uint32_t b){(void)a;(void)b;assert(0);return 0;}
int vofa_et4000_init(const vofa_io_t *io){assert(io->read&&io->write&&io->ticks);probes++;return result_code;}
int kvideo_activate_vofa(volatile uint8_t *p){assert((uintptr_t)p==VOFA_MEM);activations++;return alloc_ok;}
static void run(uint8_t mode,uint8_t cpu,uint32_t ram,const kernel_timer_service_t *t)
{logtext[0]=0;probes=activations=0;kvideo_boot(mode,cpu,ram,t);}
int main(void){
 kernel_timer_service_t timer={0};timer.poll=tick;timer.frequency_hz=50;
 contents=0;run(2,0,4<<20,&timer);assert(!probes&&!logtext[0]);
 contents="SHIFTER\n";run(2,0,4<<20,&timer);assert(!probes);
 contents="VOFA_extra";run(2,0,4<<20,&timer);assert(!probes);
 contents="VOF";run(2,0,4<<20,&timer);assert(!probes);
 contents="VOFA\r\n";
 run(1,0,4<<20,&timer);assert(!probes);
 run(2,3,4<<20,&timer);assert(!probes);
 run(2,0,VOFA_MEM,&timer);assert(!probes);
 run(2,0,4<<20,0);assert(!probes);
 timer.frequency_hz=200;run(2,0,4<<20,&timer);assert(!probes);timer.frequency_hz=50;
 for(result_code=-3;result_code<=0;result_code++){
  run(2,0,4<<20,&timer);assert(probes==1&&!activations&&strstr(logtext,"SHIFTER"));
 }
 result_code=1;run(2,0,4<<20,&timer);assert(probes==1&&activations==1&&strstr(logtext,"640x480"));
 alloc_ok=0;run(2,0,4<<20,&timer);assert(strstr(logtext,"NO SHADOW RAM"));
 puts("Video boot PASS: explicit opt-in, invalid config, CPU/RAM/mode/timer guards and every fallback");return 0;
}
