#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include "emutos_vdi_mono.h"
int main(void){uint16_t a[80],b[80];uint8_t g[16];unsigned x,k,f,bg,o,i,row,bit;
 for(x=0;x<16;x++)for(k=0;k<256;k++)for(f=0;f<2;f++)for(bg=0;bg<2;bg++)for(o=0;o<2;o++){
 for(i=0;i<80;i++)a[i]=b[i]=0xa55a;for(row=0;row<16;row++)g[row]=(uint8_t)(k+row*19);
 mono_glyph(a,4,x,1,g,f,bg,o);
 for(row=0;row<16;row++)for(bit=0;bit<8;bit++){unsigned set=g[row]&(128u>>bit),px=x+bit;uint16_t mask=0x8000u>>(px&15);uint16_t *p=&b[(row+1)*4+(px>>4)];if(set||o){if(set?f:bg)*p|=mask;else *p&=~mask;}}
 assert(!memcmp(a,b,sizeof(a)));}
 puts("Mono glyph: 32768 combinations vs pixel reference, guards/offsets/transparent PASS");return 0;}
