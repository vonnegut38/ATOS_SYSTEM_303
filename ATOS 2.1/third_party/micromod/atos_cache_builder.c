#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "micromod.h"
extern long micromod_duration_limit(long);
static uint32_t hash(const uint8_t *b,size_t n,uint32_t h){while(n--)h=(h^*b++)*16777619u;return h;}
static void be32(uint8_t *p,uint32_t n){p[0]=n>>24;p[1]=n>>16;p[2]=n>>8;p[3]=n;}
int main(int argc,char **argv){
 if(argc!=3)return 2;
 FILE *f=fopen(argv[1],"rb");if(!f)return 2;
 fseek(f,0,SEEK_END);long n=ftell(f);rewind(f);signed char *data=malloc(n);
 if(!data||fread(data,1,n,f)!=(size_t)n)return 3;fclose(f);
 if(n<1084||micromod_calculate_mod_file_len(data)>n||micromod_initialise(data,12288))return 4;
 long length=micromod_duration_limit(8*1024*1024);if(length<=0)return 5;
 uint8_t *pcm=malloc(length);short b[1024];
 for(long p=0;p<length;){long count=length-p;if(count>512)count=512;
  memset(b,0,sizeof b);micromod_get_audio(b,count);
  for(int k=0;k<count;k++){int v=((int)b[k*2]+b[k*2+1])>>9;pcm[p++]=(v>127?127:v< -128?-128:v)+128;}}
 uint8_t header[32]={0};memcpy(header,"MMPCM002",8);be32(header+8,12288);be32(header+12,length);
 be32(header+16,n);be32(header+20,hash((uint8_t *)data,n,2166136261u));
 be32(header+24,hash(pcm,length,2166136261u));be32(header+28,0x83b5d9a);
 f=fopen(argv[2],"wb");if(!f)return 6;fwrite(header,1,32,f);fwrite(pcm,1,length,f);fclose(f);
 printf("PCM cache: %ld samples, %.2f seconds\n",length,(double)length/12288);free(data);free(pcm);return 0;
}
