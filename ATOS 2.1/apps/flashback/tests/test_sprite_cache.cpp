#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <fstream>
#include <iterator>
#include "../engine/dos_sprite_cache.h"
static unsigned le16(const uint8_t*p){return p[0]|(p[1]<<8);}
static unsigned le32(const uint8_t*p){return le16(p)|(le16(p+2)<<16);}
static void reference(const uint8_t *dataPtr, uint8_t *dst) {
 const int len=2*((dataPtr[0]<<8)|dataPtr[1]); dataPtr+=2;
 uint8_t *dst2=dst+1024;
 for(int i=0;i<len;++i){*dst2++=dataPtr[i]>>4;*dst2++=dataPtr[i]&15;}
 const uint8_t *src=dst+1024,*end=src+len;
 do {
  const uint8_t code=*src++;
  if(code==15){uint8_t color=*src++;int count=*src++;
   if(color==15){count=(count<<4)|*src++;color=*src++;}
   count+=4;memset(dst,color,count);dst+=count;
  }else *dst++=code;
 }while(src<end);
}
static std::vector<uint8_t> load(const char *p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char **argv) {
 if(argc<3 || !(argc&1)) { fprintf(stderr,"usage: test_sprite_cache SPR OFF [SPR OFF ...]\n"); return 2; }
 uint8_t packed[512], expanded[1032];
 for(unsigned i=0;i<512;++i)packed[i]=uint8_t(i);
 for(unsigned align=0;align<4;++align)for(unsigned n=0;n<=512;++n){
  memset(expanded,0xaa,sizeof(expanded));dosExpandNibbles(packed,n,expanded+align);
  for(unsigned i=0;i<n;++i)if(expanded[align+2*i]!=(packed[i]>>4)||expanded[align+2*i+1]!=(packed[i]&15))abort();
  for(unsigned i=0;i<align;++i)if(expanded[i]!=0xaa)abort();
  for(unsigned i=align+2*n;i<sizeof(expanded);++i)if(expanded[i]!=0xaa)abort();
 }
 auto *cache=new DosSpriteCache();
 uint8_t a[65536],b[65536];unsigned frames=0;
 for(int n=1;n+1<argc;n+=2){
  auto spr=load(argv[n]),off=load(argv[n+1]);
  // The legacy decoder overreads unused packed bytes. Pad its private
  // reference copy only; the new decoder sees the exact file allocation.
  auto padded=spr; padded.resize(spr.size()+4096,0);
  for(unsigned i=0;i+6<=off.size()&&le16(&off[i])!=65535;i+=6){
   unsigned o=le32(&off[i+2]); if(o==0xffffffff)continue;
   if(o+16>spr.size())abort();
   const uint8_t *s=&spr[12+o];if(s[2]&128)continue;
   unsigned bytes=unsigned(s[2]&63)*s[3];if(bytes>sizeof(a))abort();
   for(int k=0;k<3;++k){
    memset(a,0x5a,sizeof(a));memset(b,0x5a,sizeof(b));
    reference(&padded[12+o+4],a);cache->unpack(s+4,b);
    if(memcmp(a,b,bytes)){fprintf(stderr,"sprite mismatch %s %u\n",argv[n],o);abort();}
   }
   ++frames;
  }
 }
 uint8_t changed[]={0,2,0x12,0x34,0,0,0,0};
 for(int i=0;i<10;++i){
  changed[2]=i;reference(changed,a);cache->unpack(changed,b);
  if(memcmp(a,b,4))abort();
 }
 delete cache;printf("sprite cache: %u real compressed frames (cold/warm) + same-address mutation PASS\n",frames);
}
