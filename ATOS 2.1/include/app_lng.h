#ifndef ATOS_APP_LNG_H
#define ATOS_APP_LNG_H
#include "gui_app.h"
#include "lng_catalog.h"
/* One private, bounded dictionary per process. No shared mutable UI strings.
 * ASCII until a documented font/encoding contract supports Polish diacritics. */
#define LNG_CAPACITY 9216u
static char lng_data[LNG_CAPACITY+1];
static unsigned short lng_offsets[LNG_COUNT]; /* offset + 1; zero => English */
static unsigned lng_selected;
static int lng_equal(const char *a,const char *b)
{while(*a&&*a==*b){a++;b++;}return *a==*b;}
static const char *lng_text(unsigned id)
{return id<LNG_COUNT&&lng_offsets[id]?lng_data+lng_offsets[id]-1:lng_english(id);}
static void lng_reset(void)
{unsigned i;for(i=0;i<LNG_COUNT;i++)lng_offsets[i]=0;}
/* Direct-mapped key index. The old parser rescanned every catalog key for every
 * file line (lines x keys calls to lng_key/lng_equal); with -fno-jump-tables the
 * key switch is a comparison tree, so that scan cost seconds at app startup.
 * Build the key map once per parse and probe it per line instead. */
#define LNG_HASH_SIZE 256u
static unsigned short lng_key_map[LNG_HASH_SIZE]; /* catalog index + 1; 0 empty */
/* 32-bit multiply would call libgcc __mulsi3 (shift-add on 68000); *33 is a
 * shift and an add, so keep the hash multiply-free. */
static unsigned lng_hash(const char *s)
{unsigned h=5381u;while(*s)h=((h<<5)+h)^(unsigned char)*s++;return h;}
static void lng_build_map(void)
{unsigned i,b;for(i=0;i<LNG_HASH_SIZE;i++)lng_key_map[i]=0;
 for(i=0;i<LNG_COUNT;i++){b=lng_hash(lng_key(i))&(LNG_HASH_SIZE-1u);
  while(lng_key_map[b])b=(b+1u)&(LNG_HASH_SIZE-1u);
  lng_key_map[b]=(unsigned short)(i+1u);}}
static int lng_lookup(const char *key)
{unsigned b=lng_hash(key)&(LNG_HASH_SIZE-1u);unsigned short slot;
 for(;;){slot=lng_key_map[b];if(!slot)return -1;
  if(lng_equal(key,lng_key(slot-1u)))return (int)(slot-1u);
  b=(b+1u)&(LNG_HASH_SIZE-1u);}}
/* Parses in place, preserves value spaces, accepts LF/CRLF, rejects duplicate
 * known keys, bad escapes and control characters. Unknown keys allow extensions.
 * Missing keys use English. A malformed file uses English for every key. */
static int lng_parse(unsigned size)
{
 unsigned pos=0,line=0,i;lng_reset();lng_build_map();
 if(size>LNG_CAPACITY)return -1;
 lng_data[size]=0;
 while(pos<size){unsigned begin=pos,end,eq,out,in;
  while(pos<size&&lng_data[pos]!='\n'){if(!lng_data[pos])goto bad;pos++;}
  end=pos;if(pos<size)pos++;
  if(end>begin&&lng_data[end-1]=='\r')end--;
  lng_data[end]=0;
  if(!line++){if(!lng_equal(lng_data+begin,"ATOS-LNG-1"))goto bad;continue;}
  if(begin==end||lng_data[begin]=='#')continue;
  eq=begin;while(eq<end&&lng_data[eq]!='=')eq++;
  if(eq==begin||eq==end)goto bad;
  lng_data[eq]=0;
  for(i=begin;i<eq;i++)if(!((lng_data[i]>='A'&&lng_data[i]<='Z')||(lng_data[i]>='0'&&lng_data[i]<='9')||lng_data[i]=='_'))goto bad;
  in=out=eq+1;
  while(in<end){unsigned char c=(unsigned char)lng_data[in++];
   if(c<32||c>126)goto bad;
   if(c=='\\'){if(in==end)goto bad;c=(unsigned char)lng_data[in++];if(c=='n')c='\n';else if(c!='\\')goto bad;}
   lng_data[out++]=(char)c;
  }
  if(out==eq+1||out-(eq+1)>78)goto bad;
  lng_data[out]=0;
  {int found=lng_lookup(lng_data+begin);
   if(found>=0){if(lng_offsets[found])goto bad;lng_offsets[found]=(unsigned short)(eq+2);}}
 }
 if(!line)goto bad;
 return 0;
 bad:lng_reset();return -1;
}
static int lng_read(const char *path,char *buffer,unsigned capacity)
{
 uint32_t st[2];unsigned used=0;int fd,r;
 if(gui_call(5,0,0,(void *)path,st)||st[0]>capacity||(st[1]&16))return -1;
 fd=gui_call(2,0,0,(void *)path,0);if(fd<0)return -1;
 while(used<st[0]){r=gui_call(3,(uint32_t)fd,st[0]-used,buffer+used,0);
  if(r<=0||(unsigned)r>st[0]-used){gui_call(4,(uint32_t)fd,0,0,0);return -1;}used+=(unsigned)r;}
 r=gui_call(4,(uint32_t)fd,0,0,0);return r<0?-1:(int)used;
}
static int lng_header_ok(void)
{static const char h[]="ATOS-LNG-1";unsigned i;for(i=0;i<10u;i++)if(lng_data[i]!=h[i])return 0;return 1;}
static int lng_load(unsigned language)
{
 int n;lng_selected=language?1u:0u;lng_reset();
 n=lng_read(language?"\\SYSTEM\\LANG\\PL.LNG":"\\SYSTEM\\LANG\\EN.LNG",lng_data,LNG_CAPACITY);
 if(n<0)return -1;
#ifndef LNG_FORCE_PARSE
 /* The build records the byte offset of every value, so adopt the pack without
  * matching key names. Each value is bounded by its own LF, so unescaping it in
  * place never moves a later value and the generated offsets stay valid. A
  * length+header match guards a user-edited pack, which uses the parser below. */
 if((unsigned)n==(unsigned)(language?lng_len_pl:lng_len_en)&&n>=11&&lng_header_ok()){
  const unsigned short *table=language?lng_off_pl:lng_off_en;unsigned i;
  lng_data[n]=0;
  for(i=0;i<LNG_COUNT;i++){
   char *p=lng_data+table[i]-1u,*q=p;lng_offsets[i]=table[i];
   while(*p&&*p!='\n'){
    char c=*p++;
    if(c=='\\'&&*p=='n'){c='\n';++p;}
    else if(c=='\\'&&*p=='\\'){c='\\';++p;}
    *q++=c;
   }
   *q=0;
  }
  return 0;
 }
#endif
 return lng_parse((unsigned)n);
}
static void lng_init(void)
{
 char setting[3];int n=lng_read("\\SYSTEM\\LANGUAGE.CFG",setting,sizeof(setting));
 lng_load(n==3&&setting[0]=='P'&&setting[1]=='L'&&setting[2]=='\n');
}
#endif
