// Test the real ATOS copyRect/rectangle collector against a reference display.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include "../port/systemstub_atos.cpp"
extern "C" int32_t atos_call(uint32_t,uint32_t,uint32_t,const void*,const void*) { abort(); }
extern "C" void atos_puts(const char*) { abort(); }
void error(const char *s,...) { fprintf(stderr,"%s\n",s); abort(); }
static void require(bool ok) { if (!ok) abort(); }
int main() {
 SystemStub_ATOS stub;
 stub._w=256; stub._h=224;
 stub._screen=(uint8_t*)calloc(1,256*224);
 stub._frame=(atos_game_frame_t*)calloc(1,sizeof(atos_game_frame_t));
 uint8_t src[256*224],present[256*224];
 memset(src,0,sizeof(src));memset(present,0,sizeof(present));
 unsigned rng=7;
 for (int frame=0;frame<500;++frame) {
  memcpy(src,present,sizeof(src));
  for (int k=0;k<20;++k) {
   rng=rng*1664525u+1013904223u;
   int x=(rng>>8)%256,y=(rng>>16)%224;
   src[y*256+x]=(uint8_t)rng;
  }
  // Same full damage is common in scene changes. Unchanged frames must cost
  // zero card rectangles, and sparse changes must still reach the display.
  stub.copyRect(0,0,256,224,src,256);
  if(stub._fullDirty)memcpy(present,stub._screen,sizeof(present));
  else {
   int count=stub.collectRects();require(count>=0);
   for(int i=0;i<count;++i) {
    auto r=stub._frame->rect[i];
    for(int y=r.y;y<r.y+r.h;++y)
     memcpy(present+y*256+r.x,stub._screen+y*256+r.x,r.w);
   }
  }
  require(memcmp(src,present,sizeof(src))==0);
  stub._fullDirty=false;memset(stub._blocks,0,sizeof(stub._blocks));
  stub.copyRect(0,0,256,224,src,256);
  require(stub.collectRects()==0);
  // Exercise the unaligned fallback and multiple writes to one dirty block.
  src[3*256+5]^=31;
  stub.copyRect(5,3,13,9,src,256);
  src[3*256+5]^=17;
  stub.copyRect(5,3,13,9,src,256);
  int n=stub.collectRects();require(n>0);
  for(int i=0;i<n;++i) {
   auto r=stub._frame->rect[i];
   for(int y=r.y;y<r.y+r.h;++y)
    memcpy(present+y*256+r.x,stub._screen+y*256+r.x,r.w);
  }
  require(memcmp(src,present,sizeof(src))==0);
  memset(stub._blocks,0,sizeof(stub._blocks));
 }
 free(stub._screen);free(stub._frame);
 puts("damage: 500 sparse frames, identical frames, unaligned and repeated writes PASS");
}
