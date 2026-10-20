#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../system/sysinfo.c"
static uint32_t now,reply_pid,reply_op,alloc_calls,free_calls,disk_reads;
static system_report_t fixture;
int32_t gui_call(uint32_t call,uint32_t a,uint32_t b,void *p,void *q)
{
 (void)q;
 if(call==ATOS_CALL_AUDIO_CAPS)return -32;
 if(call==ATOS_CALL_AUDIO_STATE)return -32;
 if(call==56){assert(a==sizeof(fixture));memcpy(p,&fixture,sizeof(fixture));return 0;}
 if(call==58)return (int32_t)now;
 if(call==16){alloc_calls++;return -1;}
 if(call==17){free_calls++;return 0;}
 if(call==57){assert(b==8&&a<=info.boot_sectors-8);disk_reads++;memset(p,0x59,4096);return 4096;}
 if(call==36){uint32_t *v=p;reply_pid=a;reply_op=v[0];return 0;}
 if(call==37)return -1;
 if(call==38){gui_message_t *m=p;memset(m,0,sizeof(*m));m->sender=reply_pid;m->v[0]=reply_op|0x80000000u;return 0;}
 if(call==11||call==1||call==67)return 0;
 if(call==40)return 1;
 assert(!"unexpected syscall");return -1;
}
int main(void){uint32_t r=1;unsigned i;static uint32_t words[8192];
 for(i=0;i<10000;i++){r=r*1664525u+1013904223u;uint32_t n=r;r=r*1664525u+1013904223u;uint32_t d=r|1;assert(quotient(n,d)==n/d);}
 assert(!quotient(15,0));
 memset(&fixture,0,sizeof fixture);fixture.magic=SYSREPORT_MAGIC;fixture.version=SYSREPORT_VERSION;fixture.size=sizeof fixture;
 fixture.width=fixture.output_width=640;fixture.height=fixture.output_height=400;fixture.planes=1;
 fixture.hardware_report=1;fixture.detected_chip=2;fixture.detected_adapter=1;
 fixture.st_total=4<<20;fixture.st_free=2<<20;fixture.clock_source=2;fixture.clock_mhz=8;
 fixture.disk_capacity_known=1;fixture.disk_sectors=1572864;fixture.sector_bytes=512;
 fixture.volumes=14;fixture.boot_sectors=4096;strcpy(fixture.disk_name,"TEST DISK");
 fixture.wm_requests_last=7;fixture.wm_requests_max=9;
 for(i=0;i<14;i++){memcpy(fixture.volume[i].id,"BGM",4);fixture.volume[i].start=i*2048+1;fixture.volume[i].sectors=2048;}
 gfx_pid=gui_pid=1;
 assert(!refresh());
 /* keyboard tabs and partition paging */
 key_event(0x3c);assert(page==1);
 key_event(0x51);assert(disk_page==8);
 key_event(0x49);assert(!disk_page);
 key_event(0x3b);assert(page==0);
 /* mouse tabs: DISK button at (cx 134..254, cy 20..40); wx=120, wy=80 */
 mouse_click(134+120,20+80+16);assert(page==1);
 /* Devices must page independently of fixed controls; negative caps are not success. */
 key_event(0x3e);assert(page==3&&dev_count>dev_visible());
 assert(strstr(dev_val[1],"unavailable"));
 key_event(0x51);assert(dev_first>0);
 key_event(0x49);assert(dev_first==0);
 mouse_click(wx+130,wy+16+client_h-16);assert(!quit&&!bench); /* refresh */
 mouse_click(wx+20,wy+16+client_h-16);assert(quit);quit=0;
 client_w=64;client_h=34;paint();mouse_click(wx+20,wy+36);assert(!quit);
 client_w=WW;client_h=CLIENT_H;
 /* benchmark accounting (unchanged from the text version) */
 begin(1,0);now=start+100;step();assert(!bench&&results[0].state==1&&results[0].amount==1024&&results[0].ticks==100);
 bench=2;all_tests=0;ram=words;start=now;amount=offset=0;seed=0xa5;now+=100;step();assert(!bench&&results[1].state==1&&results[1].amount==4096&&free_calls);
 begin(4,0);now=start+100;step();assert(!bench&&disk_reads==1&&results[3].amount==4096);
 begin(1,1);now=start+100;step();assert(bench==4&&results[1].state==2&&results[2].state==2);
 cancel();assert(!bench&&results[3].state==4);
 info.tt_total=0;begin(2,2);if(!bench)advance(2);assert(!bench&&!all_tests);
 puts("SYSINFO PASS: report, tabs, partition paging, division, benchmark accounting, skips, cancel and mouse tabs");return 0;
}
