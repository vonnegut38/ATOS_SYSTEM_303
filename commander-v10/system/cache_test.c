#include "gui_app.h"
typedef struct {char name[13];uint8_t attr,pad[2];uint32_t size;} entry_t;
static entry_t page[32];
static uint32_t checks[3];
static int counts[3];
static char paths[3][20]={"\\","\\SYSTEM\\APP","\\SYSTEM\\TEST"};
static void print(char *s){gui_call(1,0,0,s,0);}
static uint32_t checksum(unsigned n){unsigned i,j;uint32_t sum=0;for(i=0;i<n;i++){for(j=0;j<13&&page[i].name[j];j++)sum=sum*33u+(uint8_t)page[i].name[j];sum=sum*33u+page[i].attr;sum=sum*33u+page[i].size;}return sum;}
int app_main(void){unsigned i,j;int n;uint32_t start,elapsed;char digits[12];
 print("CACHETST: 300 DIRECTORY PAGES (NO DRAW)\n");
 for(j=0;j<3;j++){n=gui_call(52,0,32,paths[j],page);if(n<1)return 10+j;counts[j]=n;checks[j]=checksum((unsigned)n);}
 start=(uint32_t)ticks();
 for(i=0;i<100;i++)for(j=0;j<3;j++){n=gui_call(52,0,32,paths[j],page);if(n!=counts[j]||checksum((unsigned)n)!=checks[j])return 20+j;}
 elapsed=(uint32_t)ticks()-start;number(digits,elapsed);print("CACHETST: PASS, TICKS = ");print(digits);print("\n");return 0;}
