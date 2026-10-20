#ifndef DOS_SPRITE_CACHE_H
#define DOS_SPRITE_CACHE_H
#include <stdint.h>
#include <string.h>
#include "dos_pixels.h"

// A bounded cache of decoded DOS RLE sprites. Bank buffers can be overwritten
// at the same address, so a pointer alone is not a valid identity. Compare the
// consumed packed bytes too; changing a level/monster cannot return old pixels.
struct DosSpriteCache {
 enum { Slots = 64, Capacity = 1024 };
 struct Entry {
  const uint8_t *key;
  unsigned nibbleCount, packedSize, outputSize;
  uint8_t packed[Capacity], output[Capacity];
 } entry[Slots];

 static unsigned decode(const uint8_t *data, uint8_t *dst, unsigned *used) {
  const unsigned len = 2u * ((unsigned(data[0]) << 8) | data[1]);
  const uint8_t *packed = data + 2;
  uint8_t *nibbles = dst + 1024;
  // len counts nibbles, not bytes. The original loop read twice the
  // packed payload (and could overrun the final sprite in the bank).
  dosExpandNibbles(packed,len/2,nibbles);
  const uint8_t *src = nibbles, *end = src + len;
  uint8_t *out = dst;
  do {
   unsigned code = *src++;
   if (code == 15) {
    unsigned color = *src++, count = *src++;
    if (color == 15) { count=(count<<4)|*src++; color=*src++; }
    count += 4;
    memset(out,color,count); out += count;
   } else *out++ = code;
  } while(src<end);
  *used = unsigned(src-nibbles+1)/2;
  return unsigned(out-dst);
 }

 void unpack(const uint8_t *data, uint8_t *dst) {
  const unsigned len = 2u * ((unsigned(data[0]) << 8) | data[1]);
  const uintptr_t p = (uintptr_t)data;
  Entry &e = entry[((p>>2)^(p>>9)) & (Slots-1)];
  if (e.key==data && e.nibbleCount==len &&
      !memcmp(e.packed,data+2,e.packedSize)) {
   memcpy(dst,e.output,e.outputSize);
   return;
  }
  unsigned used;
  const unsigned size=decode(data,dst,&used);
  e.key=0;
  if(used<=Capacity && size<=Capacity) {
   memcpy(e.packed,data+2,used); memcpy(e.output,dst,size);
   e.nibbleCount=len; e.packedSize=used; e.outputSize=size; e.key=data;
  }
 }
};
#endif
