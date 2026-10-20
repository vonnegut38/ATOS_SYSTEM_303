#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "settings_config.h"
#include "wm_reformat.h"
int main(void)
{
 unsigned w=0,h=0,d=0,i,x,y;char out[128];int n;
 assert(settings_parse_mode("800x600x16",&w,&h,&d)&&w==800&&h==600&&d==16);
 assert(settings_parse_mode("1024x768x8",&w,&h,&d)&&w==1024);
 assert(settings_parse_mode("640x480x4\r",&w,&h,&d)&&d==4);
 assert(settings_parse_mode("3",&w,&h,&d)&&w==800&&d==8);
 assert(!settings_parse_mode("800x600x16oops",&w,&h,&d));
 assert(!settings_parse_mode("999999999x600x8",&w,&h,&d));
 assert(!settings_parse_mode("800x600x3",&w,&h,&d));
 n=settings_merge_mode("RES=3\nX=1\nRES=4\nY=2",19,"RES=800x600x16",out,sizeof out);assert(n>0);out[n]=0;
 assert(!strcmp(out,"RES=800x600x16\nX=1\nY=2"));
 assert(settings_merge_mode("X=1",3,"RES=800x600x16",out,10)<0);
 /* Exhaustive small in-place conversions versus an independent pixel model. */
 for(i=0;i<4;++i){unsigned ob=(i&1)?8:1,nb=(i&2)?8:1;
  for(w=8;w<=64;w+=8){unsigned ow=40,oh=5,nh=7,op=ob==8?ow:ow/8,np=nb==8?w:w/8;
   uint8_t buf[512],orig[512];memset(buf,0xcc,sizeof buf);
   for(y=0;y<oh;++y)for(x=0;x<op;++x)buf[y*op+x]=(uint8_t)(x*19+y*31);
   memcpy(orig,buf,sizeof buf);repitch_surface(buf,op,ow,oh,ob,np,w,nh,nb);
   for(y=0;y<nh;++y)for(x=0;x<w;++x){unsigned expected=0,actual;
    if(y<oh&&x<ow){expected=ob==8?orig[y*op+x]:!!(orig[y*op+x/8]&(0x80u>>(x%8)));if(nb==1)expected=!!expected;}
    actual=nb==8?buf[y*np+x]:!!(buf[y*np+x/8]&(0x80u>>(x%8)));assert(actual==expected);
   }
   assert(buf[np*nh]==0xcc||np*nh<op*oh);
  }
 }
 puts("PASS config parsing/merge and in-place surface format/stride conversion");return 0;
}
