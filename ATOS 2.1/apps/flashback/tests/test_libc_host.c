#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <assert.h>
/* prefixed port libc */
int t_snprintf(char*,size_t,const char*,...);
void *t_malloc(size_t); void t_free(void*); void *t_realloc(void*,size_t); void *t_calloc(size_t,size_t);
int t_atos_heap_init(void);
void *t_fopen(const char*,const char*); size_t t_fread(void*,size_t,size_t,void*); int t_fseek(void*,long,int); long t_ftell(void*); int t_fclose(void*); char *t_fgets(char*,int,void*);
size_t t_fwrite(const void*,size_t,size_t,void*);
void t_atos_path(char*,size_t,const char*);
uint32_t t_atos_heap_size(void);
int t_strcasecmp(const char*,const char*); void *t_memmove(void*,const void*,size_t); void *t_memset(void*,int,size_t); void *t_memcpy(void*,const void*,size_t);
void *t___ctors_start,*t___ctors_end,*t___init_array_start,*t___init_array_end;
int t_atos_main(void){return 0;} void t_atos_port_shutdown(void){}
/* fake kernel */
static unsigned char arena[13<<20]; static int arena_used;
static FILE *fds[4]; static char saved_path[128]; static unsigned char saved[100000]; static unsigned saved_len;
static const char *root="/tmp/atosfs";
/* Host test of port/libc.c: the object is built with every symbol prefixed by
   t_ (objcopy --prefix-symbols) and talks to the fake kernel below. */
static void hostpath(char *o,const char *p){sprintf(o,"%s/%s",root,p);for(char*q=o+strlen(root);*q;q++)if(*q=='\\')*q='/';}
int32_t t_atos_call(uint32_t n,uint32_t d1,uint32_t d2,const void *a0,const void *a1){
 char hp[256];
 switch(n){
 case 1: fputs((const char*)a0,stdout);return 0;
 case 16: if(arena_used||d1>sizeof(arena))return -1; arena_used=1; return (int32_t)(intptr_t)(arena+8); /* 32-bit trick below */
 case 5:{hostpath(hp,a0);FILE*f=fopen(hp,"rb");if(!f)return -1;fseek(f,0,SEEK_END);((uint32_t*)a1)[0]=ftell(f);((uint32_t*)a1)[1]=0;fclose(f);return 0;}
 case 2:{hostpath(hp,a0);for(int i=0;i<4;i++)if(!fds[i]){fds[i]=fopen(hp,"rb");return fds[i]?i:-1;}return -2;}
 case 3: return (int32_t)fread((void*)a0,1,d2,fds[d1]);
 case 6: fseek(fds[d1],d2,SEEK_SET);return d2;
 case 4: fclose(fds[d1]);fds[d1]=0;return 0;
 case 60: strcpy(saved_path,a0);memcpy(saved,a1,d1);saved_len=d1;return 0;
 case 67: return 0;
 }
 return -1;
}
int main(void){
 char b[128];
 t_snprintf(b,sizeof b,"%d|%5d|%-5d|%05d|%02d|%x|%X|%08x|%04X|%u|%ld|%s|%c|%p|%%|%2d|%08u",42,-7,3,-12,5,255,0xbeef,0x1234,0xab,4000000000u,-123456L,"str",'Z',(void*)0x10,7,77u);
 puts(b);
 assert(!strcmp(b,"42|   -7|3    |-0012|05|ff|BEEF|00001234|00AB|4000000000|-123456|str|Z|00000010|%| 7|00000077"));
 t_snprintf(b,6,"%s","truncated"); assert(!strcmp(b,"trunc"));
 t_atos_path(b,sizeof b,"C:/games//flashbk/data/level1.map"); puts(b); assert(!strcmp(b,"\\GAMES\\FLASHBK\\DATA\\LEVEL1.MAP"));
 assert(t_strcasecmp("Level1.MAP","LEVEL1.map")==0);
 /* heap */
 assert(t_atos_heap_init()); printf("heap %u\n",t_atos_heap_size());
 void *p[2000]; size_t sz[2000]; unsigned seed=1;
 for(int round=0;round<20;round++){
  for(int i=0;i<2000;i++){seed=seed*1103515245+12345; sz[i]=(seed>>8)%5000+1; p[i]=t_malloc(sz[i]); assert(p[i]); t_memset(p[i],i&255,sz[i]);}
  for(int i=0;i<2000;i+=2){unsigned char*q=p[i];for(size_t k=0;k<sz[i];k++)assert(q[k]==(i&255)); t_free(p[i]);}
  for(int i=1;i<2000;i+=2){p[i]=t_realloc(p[i],sz[i]*2);unsigned char*q=p[i];for(size_t k=0;k<sz[i];k++)assert(q[k]==(i&255));}
  for(int i=1;i<2000;i+=2)t_free(p[i]);
 }
 void *big=t_malloc(t_atos_heap_size()-64); assert(big); t_free(big); /* fully coalesced */
 /* memmove overlap */
 char m[32]="abcdefghijklmnopqrstuvwxyz"; t_memmove(m+2,m,10); assert(!memcmp(m,"ababcdefghijmn",14));
 /* files */
 system("mkdir -p /tmp/atosfs/D && head -c 20000 /dev/urandom > /tmp/atosfs/D/BIG.BIN && printf 'a=1\\nbb=2\\n' > /tmp/atosfs/D/CFG.TXT");
 void *f=t_fopen("D/big.bin","rb"); assert(f);
 FILE *h=fopen("/tmp/atosfs/D/BIG.BIN","rb"); unsigned char ref[20000]; fread(ref,1,20000,h); fclose(h);
 unsigned char got[20000]; size_t pos=0;
 while(pos<20000){size_t n=(pos*7)%9000+1; if(n>20000-pos)n=20000-pos; assert(t_fread(got+pos,1,n,f)==n); pos+=n;}
 assert(!memcmp(got,ref,20000));
 t_fseek(f,19990,0); assert(t_fread(got,1,100,f)==10); assert(!memcmp(got,ref+19990,10));
 t_fseek(f,5,0); for(int i=0;i<300;i++){unsigned char c; t_fread(&c,1,1,f); assert(c==ref[5+i]);} assert(t_ftell(f)==305);
 t_fclose(f);
 f=t_fopen("D/CFG.TXT","rb"); assert(t_fgets(b,sizeof b,f)&&!strcmp(b,"a=1\n")); assert(t_fgets(b,sizeof b,f)&&!strcmp(b,"bb=2\n")); assert(!t_fgets(b,sizeof b,f)); t_fclose(f);
 assert(!t_fopen("D/NOPE.BIN","rb"));
 f=t_fopen("D/SAVE.BIN","wb"); for(int i=0;i<10000;i++){unsigned char c=i*3;t_fwrite(&c,1,1,f);} t_fclose(f);
 assert(!strcmp(saved_path,"D\\SAVE.BIN")&&saved_len==10000&&saved[9999]==(unsigned char)(9999*3));
 puts("LIBC HOST TEST PASS");
 return 0;
}
