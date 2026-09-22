#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../system/sysinfo.c"
static uint32_t now,reply_pid,reply_op,alloc_calls,free_calls,disk_reads;
static system_report_t fixture;
int32_t gui_call(uint32_t call,uint32_t a,uint32_t b,void *p,void *q)
{
 (void)q;
 if(call==56){assert(a==sizeof(fixture));memcpy(p,&fixture,sizeof(fixture));return 0;}
 if(call==58)return (int32_t)now;
 if(call==16){alloc_calls++;return -1;}
 if(call==17){free_calls++;return 0;}
 if(call==57){assert(b==8&&a<=info.boot_sectors-8);disk_reads++;memset(p,0x59,4096);return 4096;}
 if(call==36){uint32_t *v=p;reply_pid=a;reply_op=v[0];return 0;}
 if(call==37)return -1;
 if(call==38){gui_message_t *m=p;memset(m,0,sizeof(*m));m->sender=reply_pid;m->v[0]=reply_op|0x80000000u;return 0;}
 if(call==11||call==1)return 0;
 if(call==40)return 1;
 assert(!"unexpected syscall (including any disk write)");return -1;
}
static int has(const char *s){char linebuf[81];unsigned y,x;for(y=0;y<25;y++){for(x=0;x<80;x++)linebuf[x]=(char)(screen.cells[y*80+x]&255);linebuf[80]=0;if(strstr(linebuf,s))return 1;}return 0;}
int main(void){uint32_t r=1;unsigned i;static uint32_t words[8192];
 for(i=0;i<10000;i++){r=r*1664525u+1013904223u;uint32_t n=r;r=r*1664525u+1013904223u;uint32_t d=r|1;assert(quotient(n,d)==n/d);}
 assert(!quotient(15,0));
 memset(&fixture,0,sizeof fixture);fixture.magic=SYSREPORT_MAGIC;fixture.version=1;fixture.size=sizeof fixture;
 fixture.width=fixture.output_width=640;fixture.height=fixture.output_height=400;fixture.planes=1;
 fixture.hardware_report=1;fixture.detected_chip=2;fixture.detected_adapter=1;
 fixture.st_total=4<<20;fixture.st_free=2<<20;fixture.clock_source=2;fixture.clock_mhz=8;
 fixture.disk_capacity_known=1;fixture.disk_sectors=1572864;fixture.sector_bytes=512;
 fixture.volumes=14;fixture.boot_sectors=4096;strcpy(fixture.disk_name,"TEST DISK");
 for(i=0;i<14;i++){memcpy(fixture.volume[i].id,"BGM",4);fixture.volume[i].start=i*2048+1;fixture.volume[i].sectors=2048;}
 assert(!refresh());gfx=gui=1;page=0;render();assert(has("Shifter")&&has("ATI Mach32")&&has("nominalnie")&&has("4096 KiB"));
 info.active_video=1;info.output_height=480;render();assert(has("VOFA + ET4000")&&has("640 x 480"));
 page=1;render();assert(has("ACSI / port DMA")&&has("768.0 MiB")&&has("TEST DISK"));key(0x51);render();assert(disk_page==8&&has("P:"));key(0x49);assert(!disk_page);
 info.disk_capacity_known=0;render();assert(has("nieustalona"));
 begin(1,0);now=start+100;step();assert(!bench&&results[0].state==1&&results[0].amount==1024&&results[0].ticks==100);
 bench=2;all_tests=0;ram=words;start=now;amount=offset=0;seed=0xa5;now+=100;step();assert(!bench&&results[1].state==1&&results[1].amount==8192&&words[0]==0xa5&&free_calls);
 begin(4,0);now=start+100;step();assert(!bench&&disk_reads==1&&results[3].amount==4096);
 begin(1,1);now=start+100;step();assert(bench==4&&results[1].state==2&&results[2].state==2);
 cancel();assert(!bench&&results[3].state==4);page=2;render();assert(has("przerwany"));
 info.tt_total=0;begin(2,2);if(!bench)advance(2);assert(!bench&&!all_tests);
 /* Mouse tabs and footer; physical mapping is shared with Commander. */
 {gui_message_t m={0};m.v[0]=0x4b4d4f55;m.v[1]=(32u*8u<<16)|32u;m.v[2]=20u<<24;handle(&m);assert(page==1);}
 puts("SYSINFO PASS: report views, output vs detected card, unknown values, partition paging, division, CPU/RAM/disk accounting, skips, cancel and mouse tabs");return 0;
}
