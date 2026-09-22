#include "gui_app.h"
#include "kernel_fat.h"
static unsigned char a[512],b[512];
static int compare(void){int h1,h2,r1,r2;unsigned i;h1=gui_call(2,0,0,"\\SYSTEM\\KERNEL.SYS",0);h2=gui_call(2,0,0,"\\MCPROBE\\COPY.SYS",0);if(h1<0||h2<0)return 1;
 do{r1=gui_call(3,h1,512,a,0);r2=gui_call(3,h2,512,b,0);if(r1<0||r1!=r2)return 2;for(i=0;i<(unsigned)r1;i++)if(a[i]!=b[i])return 3;}while(r1);
 gui_call(4,h1,0,0,0);gui_call(4,h2,0,0,0);return 0;}
int app_main(void){int r;uint32_t stat[2];
 gui_call(1,0,0,"FATWRITE: CREATE / COPY / COMPARE / DELETE\n",0);
 r=gui_call(50,0,0,"\\MCPROBE",0);if(r)return 10-r;
 r=gui_call(49,0,0,"\\SYSTEM\\KERNEL.SYS","\\MCPROBE\\COPY.SYS");if(r)return 20-r;
 if(compare())return 30;
 if(gui_call(49,0,0,"\\SYSTEM\\KERNEL.SYS","\\MCPROBE\\COPY.SYS")!=-FAT_WRITE_EXISTS)return 31;
 if(gui_call(51,0,0,"\\MCPROBE",0)!=-FAT_WRITE_NOT_EMPTY)return 32;
 if(gui_call(51,0,0,"\\MCPROBE\\COPY.SYS",0))return 33;
 if(gui_call(51,0,0,"\\MCPROBE",0))return 34;
 if(gui_call(5,0,0,"\\MCPROBE",stat)>=0)return 35;
 gui_call(1,0,0,"FATWRITE: ALL PASS\n",0);return 0;
}
