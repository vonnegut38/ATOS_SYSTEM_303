#ifndef DOS_PIXELS_H
#define DOS_PIXELS_H
#include <stdint.h>
struct DosNibbleTable {
 uint16_t pixels[256];
 constexpr DosNibbleTable() : pixels{} {
  for(unsigned v=0;v<256;++v) {
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
   pixels[v]=uint16_t((v>>4)|((v&15)<<8));
#else
   pixels[v]=uint16_t(((v>>4)<<8)|(v&15));
#endif
  }
 }
};
static constexpr DosNibbleTable dosNibbleTable;
static inline void dosExpandNibbles(const uint8_t *src,unsigned size,uint8_t *dst) {
 if ((uintptr_t)dst & 1u) {
  while(size--){unsigned v=*src++;*dst++=v>>4;*dst++=v&15;}
  return;
 }
 uint16_t *out=(uint16_t *)dst;
 while(size>=4) {
  out[0]=dosNibbleTable.pixels[src[0]];out[1]=dosNibbleTable.pixels[src[1]];
  out[2]=dosNibbleTable.pixels[src[2]];out[3]=dosNibbleTable.pixels[src[3]];
  src+=4;out+=4;size-=4;
 }
 while(size--)*out++=dosNibbleTable.pixels[*src++];
}
#endif
