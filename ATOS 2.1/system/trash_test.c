#include "gui_app.h"
#include "kernel_fat.h"
static unsigned char a[512],b[512];
static int stat(char *path){uint32_t info[2];return gui_call(5,0,0,path,info);}
static void print(char *s){gui_call(1,0,0,s,0);}
static int compare(char *path){int h1,h2,n,m;unsigned i;h1=gui_call(2,0,0,"\\SYSTEM\\KERNEL.SYS",0);h2=gui_call(2,0,0,path,0);if(h1<0||h2<0)return 0;
 do{n=gui_call(3,h1,512,a,0);m=gui_call(3,h2,512,b,0);if(n<0||n!=m)return 0;for(i=0;i<(unsigned)n;i++)if(a[i]!=b[i])return 0;}while(n);
 gui_call(4,h1,0,0,0);gui_call(4,h2,0,0,0);return 1;}
#define CHECK(c,n) do{if(!(c)){char d[12];number(d,n);print("TRASHTST: FAIL ");print(d);print("\n");return n;}}while(0)
int app_main(void){
 print("TRASHTST: MOVE / RESTORE / PURGE\n");
 /* Intended only for a fresh test image: never touch an existing trash. */
 CHECK(stat("\\TRASH")<0&&stat("\\V4PROBE")<0,1);
 CHECK(!gui_call(50,0,0,"\\V4PROBE",0),2);CHECK(!gui_call(50,0,0,"\\V4PROBE\\SUB",0),3);
 CHECK(!gui_call(49,0,0,"\\SYSTEM\\KERNEL.SYS","\\V4PROBE\\SUB\\COPY.SYS"),4);
 CHECK(!gui_call(53,0,0,"\\V4PROBE",0),5);CHECK(stat("\\V4PROBE")<0,6);
 CHECK(compare("\\TRASH\\T0000001\\V4PROBE\\SUB\\COPY.SYS"),7);
 CHECK(!gui_call(54,0,0,"\\TRASH\\T0000001\\V4PROBE","\\V4PROBE"),8);
 CHECK(compare("\\V4PROBE\\SUB\\COPY.SYS"),9);
 CHECK(!gui_call(53,0,0,"\\V4PROBE",0),10);
 CHECK(!gui_call(55,0,0,"\\TRASH\\T0000002",0),11);CHECK(stat("\\TRASH\\T0000002")<0,12);
 CHECK(!gui_call(55,0,0,"\\TRASH\\T0000001",0),13);
 CHECK(gui_call(55,0,0,"\\TRASH",0)==-FAT_WRITE_READ_ONLY,14);
 CHECK(stat("\\SYSTEM\\KERNEL.SYS")>=0&&stat("\\V4PROBE")<0,15);
 print("TRASHTST: ALL PASS\n");return 0;}
