/* Minimal C library for the ATOS Flashback port.
 *
 * Only what REminiscence needs: memory (one big ATOS ALLOC arena with a
 * boundary-tag first-fit allocator), strings, a small printf, buffered
 * read-only files over the ATOS OPEN/READ/SEEK calls, write files kept in
 * memory and stored with SAVE on fclose, console output and exit.
 */
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "ctype.h"
#include "assert.h"
#include "time.h"
#include "atos_sys.h"
#include "atos_port.h"

/* ------------------------------------------------------------------ console */

#define LOG_CAP 16384u
static char log_buf[LOG_CAP];
static uint32_t log_len;
static char log_path[96];

void atos_puts(const char *s)
{
    char chunk[100];
    unsigned n=0;
    for(;*s;++s){
        char c=*s;
        if(log_len<LOG_CAP)log_buf[log_len++]=c;
        if(c=='\r')continue;
        if((unsigned char)c<32u&&c!='\n'&&c!='\t')c='?';
        chunk[n++]=c;
        if(n==sizeof(chunk)-1u){chunk[n]=0;(void)atos_call(ATOS_WRITE,0,0,chunk,0);n=0;}
    }
    if(n){chunk[n]=0;(void)atos_call(ATOS_WRITE,0,0,chunk,0);}
}

void atos_log_to(const char *path)
{
    size_t i;
    for(i=0;path[i]&&i+1<sizeof(log_path);++i)log_path[i]=path[i];
    log_path[i]=0;
}

void atos_log_flush(void)
{
    if(log_path[0]&&log_len)(void)atos_save(log_path,log_buf,log_len);
}

/* ------------------------------------------------------------------ strings */

void *memcpy(void *d,const void *s,size_t n)
{
    uint8_t *dp=(uint8_t *)d;const uint8_t *sp=(const uint8_t *)s;
    if(n>=16&&!(((uintptr_t)dp|(uintptr_t)sp)&3u)){
        uint32_t *dl=(uint32_t *)(void *)dp;const uint32_t *sl=(const uint32_t *)(const void *)sp;
        size_t k=n>>4;
        while(k--){dl[0]=sl[0];dl[1]=sl[1];dl[2]=sl[2];dl[3]=sl[3];dl+=4;sl+=4;}
        dp=(uint8_t *)dl;sp=(const uint8_t *)sl;n&=15u;
    }
    while(n--)*dp++=*sp++;
    return d;
}

void *memmove(void *d,const void *s,size_t n)
{
    uint8_t *dp=(uint8_t *)d;const uint8_t *sp=(const uint8_t *)s;
    if(dp==sp||!n)return d;
    if(dp<sp||dp>=sp+n)return memcpy(d,s,n);
    dp+=n;sp+=n;
    while(n--)*--dp=*--sp;
    return d;
}

void *memset(void *d,int c,size_t n)
{
    uint8_t *p=(uint8_t *)d;uint8_t v=(uint8_t)c;
    while(n&&((uintptr_t)p&3u)){*p++=v;--n;}
    if(n>=16){
        uint32_t w=(uint32_t)v*0x01010101u,*l=(uint32_t *)(void *)p;size_t k=n>>4;
        while(k--){l[0]=w;l[1]=w;l[2]=w;l[3]=w;l+=4;}
        p=(uint8_t *)l;n&=15u;
    }
    while(n--)*p++=v;
    return d;
}

int memcmp(const void *a,const void *b,size_t n)
{
    const uint8_t *x=(const uint8_t *)a,*y=(const uint8_t *)b;
    for(;n;--n,++x,++y)if(*x!=*y)return *x<*y?-1:1;
    return 0;
}

void *memchr(const void *s,int c,size_t n)
{
    const uint8_t *p=(const uint8_t *)s;
    for(;n;--n,++p)if(*p==(uint8_t)c)return (void *)(uintptr_t)p;
    return 0;
}

size_t strlen(const char *s){const char *p=s;while(*p)++p;return (size_t)(p-s);}
char *strcpy(char *d,const char *s){char *r=d;while((*d++=*s++)!=0){}return r;}
char *strncpy(char *d,const char *s,size_t n){size_t i=0;for(;i<n&&s[i];++i)d[i]=s[i];for(;i<n;++i)d[i]=0;return d;}
char *strcat(char *d,const char *s){strcpy(d+strlen(d),s);return d;}
char *strncat(char *d,const char *s,size_t n){char *e=d+strlen(d);while(n--&&*s)*e++=*s++;*e=0;return d;}
int strcmp(const char *a,const char *b){while(*a&&*a==*b){++a;++b;}return (int)(uint8_t)*a-(int)(uint8_t)*b;}
int strncmp(const char *a,const char *b,size_t n){for(;n;--n,++a,++b){if(*a!=*b||!*a)return (int)(uint8_t)*a-(int)(uint8_t)*b;}return 0;}
int strcasecmp(const char *a,const char *b){int x,y;do{x=tolower((uint8_t)*a++);y=tolower((uint8_t)*b++);}while(x&&x==y);return x-y;}
int strncasecmp(const char *a,const char *b,size_t n){int x=0,y=0;while(n--){x=tolower((uint8_t)*a++);y=tolower((uint8_t)*b++);if(!x||x!=y)break;}return x-y;}
char *strchr(const char *s,int c){for(;;++s){if(*s==(char)c)return (char *)(uintptr_t)s;if(!*s)return 0;}}
char *strrchr(const char *s,int c){const char *r=0;for(;;++s){if(*s==(char)c)r=s;if(!*s)return (char *)(uintptr_t)r;}}
char *strstr(const char *h,const char *n){size_t l=strlen(n);if(!l)return (char *)(uintptr_t)h;for(;*h;++h)if(!strncmp(h,n,l))return (char *)(uintptr_t)h;return 0;}
char *strdup(const char *s){size_t l=strlen(s)+1;char *d=(char *)malloc(l);if(d)memcpy(d,s,l);return d;}

/* ------------------------------------------------------------------- memory */
/* One arena from ATOS ALLOC (fast RAM preferred).  Blocks carry a size word
   before and after (boundary tags) so free() coalesces in O(1); the free list
   is address-ordered first fit. */

typedef struct block {uint32_t size;uint32_t used;struct block *next,*prev;} block_t;
#define HDR ((uint32_t)sizeof(block_t))
#define FOOT 4u
#define ALIGN_UP(x) (((x)+15u)&~15u)
#define MIN_BLOCK ALIGN_UP(HDR+FOOT+16u)
static uint8_t *arena_lo,*arena_hi;
static block_t *free_list;
static uint32_t heap_used,heap_peak,heap_size;

static void set_foot(block_t *b){*(uint32_t *)(void *)((uint8_t *)b+b->size-FOOT)=b->size;}
static void list_remove(block_t *b)
{
    if(b->prev)b->prev->next=b->next;else free_list=b->next;
    if(b->next)b->next->prev=b->prev;
    b->next=b->prev=0;
}
static void list_insert(block_t *b)
{
    block_t *p=0,*c=free_list;
    while(c&&c<b){p=c;c=c->next;}
    b->prev=p;b->next=c;
    if(p)p->next=b;else free_list=b;
    if(c)c->prev=b;
}

int atos_heap_init(void)
{
    static const uint32_t fast_sizes[]={12u<<20,8u<<20,6u<<20,4u<<20,3u<<20,2u<<20,0};
    unsigned i;uint8_t *p=0;uint32_t n=0;
    for(i=0;fast_sizes[i]&&!p;++i){
        n=fast_sizes[i];p=(uint8_t *)atos_alloc(n,1);
        if(p&&(uintptr_t)p<0x01000000u){/* landed in ST-RAM: kernel keeps the 1 MB rule */}
    }
    if(!p){n=1000u*1024u;p=(uint8_t *)atos_alloc(n,0);}
    if(!p)return 0;
    arena_lo=(uint8_t *)(((uintptr_t)p+15u)&~(uintptr_t)15u);
    arena_hi=(uint8_t *)(((uintptr_t)p+n)&~(uintptr_t)15u);
    heap_size=(uint32_t)(arena_hi-arena_lo);
    free_list=(block_t *)(void *)arena_lo;
    free_list->size=heap_size;free_list->used=0;free_list->next=free_list->prev=0;
    set_foot(free_list);
    return 1;
}

uint32_t atos_heap_size(void){return heap_size;}
uint32_t atos_heap_peak(void){return heap_peak;}

void *malloc(size_t n)
{
    block_t *b;uint32_t need;
    if(!arena_lo)return 0;
    if(!n)n=1;
    if(n>heap_size)return 0;
    need=ALIGN_UP((uint32_t)n+HDR+FOOT);
    for(b=free_list;b;b=b->next)if(b->size>=need)break;
    if(!b)return 0;
    list_remove(b);
    if(b->size-need>=MIN_BLOCK){
        block_t *r=(block_t *)(void *)((uint8_t *)b+need);
        r->size=b->size-need;r->used=0;set_foot(r);list_insert(r);
        b->size=need;
    }
    b->used=0x55534544u; /* "USED" */
    set_foot(b);
    heap_used+=b->size;if(heap_used>heap_peak)heap_peak=heap_used;
    return (uint8_t *)b+HDR;
}

void free(void *p)
{
    block_t *b,*n;
    if(!p)return;
    b=(block_t *)(void *)((uint8_t *)p-HDR);
    if((uint8_t *)b<arena_lo||(uint8_t *)b>=arena_hi||b->used!=0x55534544u){
        atos_puts("FLASHBACK: free() of an invalid pointer\n");return;}
    heap_used-=b->size;b->used=0;
    /* merge with the following block */
    n=(block_t *)(void *)((uint8_t *)b+b->size);
    if((uint8_t *)n<arena_hi&&!n->used){list_remove(n);b->size+=n->size;}
    /* merge with the preceding block */
    if((uint8_t *)b>arena_lo){
        uint32_t psize=*(uint32_t *)(void *)((uint8_t *)b-FOOT);
        block_t *pb=(block_t *)(void *)((uint8_t *)b-psize);
        if(psize&&(uint8_t *)pb>=arena_lo&&!pb->used){list_remove(pb);pb->size+=b->size;b=pb;}
    }
    set_foot(b);list_insert(b);
}

void *calloc(size_t n,size_t size)
{
    size_t t=n*size;void *p;
    if(size&&t/size!=n)return 0;
    p=malloc(t);if(p)memset(p,0,t);
    return p;
}

void *realloc(void *p,size_t n)
{
    block_t *b;void *q;uint32_t have;
    if(!p)return malloc(n);
    if(!n){free(p);return 0;}
    b=(block_t *)(void *)((uint8_t *)p-HDR);
    have=b->size-HDR-FOOT;
    if(n<=have)return p;
    q=malloc(n);if(!q)return 0;
    memcpy(q,p,have);free(p);
    return q;
}

/* ------------------------------------------------------------------ numbers */

int abs(int v){return v<0?-v:v;}
long labs(long v){return v<0?-v:v;}
unsigned long strtoul(const char *s,char **end,int base)
{
    unsigned long v=0;
    while(isspace((uint8_t)*s))++s;
    if((base==0||base==16)&&s[0]=='0'&&(s[1]=='x'||s[1]=='X')){s+=2;base=16;}
    else if(base==0)base=s[0]=='0'?8:10;
    for(;;++s){
        int d;
        if(isdigit((uint8_t)*s))d=*s-'0';
        else if(isalpha((uint8_t)*s))d=tolower((uint8_t)*s)-'a'+10;
        else break;
        if(d>=base)break;
        v=v*(unsigned long)base+(unsigned long)d;
    }
    if(end)*end=(char *)(uintptr_t)s;
    return v;
}
long strtol(const char *s,char **end,int base)
{
    int neg=0;
    while(isspace((uint8_t)*s))++s;
    if(*s=='-'){neg=1;++s;}else if(*s=='+')++s;
    {long v=(long)strtoul(s,end,base);return neg?-v:v;}
}
int atoi(const char *s){return (int)strtol(s,0,10);}
long atol(const char *s){return strtol(s,0,10);}
static uint32_t rand_state=1;
int rand(void){rand_state=rand_state*1103515245u+12345u;return (int)((rand_state>>16)&0x7fffu);}
void srand(unsigned seed){rand_state=seed;}
char *getenv(const char *name){(void)name;return 0;}

static void swap_bytes(uint8_t *a,uint8_t *b,size_t n){while(n--){uint8_t t=*a;*a++=*b;*b++=t;}}
void qsort(void *base,size_t n,size_t size,int (*cmp)(const void *,const void *))
{
    /* insertion sort: the engine sorts one small archive directory */
    size_t i,j;uint8_t *b=(uint8_t *)base;
    for(i=1;i<n;++i)for(j=i;j>0&&cmp(b+(j-1)*size,b+j*size)>0;--j)swap_bytes(b+(j-1)*size,b+j*size,size);
}
void *bsearch(const void *key,const void *base,size_t n,size_t size,int (*cmp)(const void *,const void *))
{
    size_t lo=0,hi=n;
    while(lo<hi){
        size_t mid=lo+(hi-lo)/2;const uint8_t *e=(const uint8_t *)base+mid*size;
        int c=cmp(key,e);
        if(!c)return (void *)(uintptr_t)e;
        if(c<0)hi=mid;else lo=mid+1;
    }
    return 0;
}

time_t time(time_t *t)
{
    time_t v=(time_t)atos_hires();   /* only used as a random seed */
    if(t)*t=v;
    return v;
}

/* ------------------------------------------------------------------- printf */

typedef struct {char *buf;size_t cap,len;FILE *f;} out_t;
static void out_char(out_t *o,char c)
{
    if(o->len+1<o->cap)o->buf[o->len]=c;
    ++o->len;
}
static void out_pad(out_t *o,char c,int n){while(n-->0)out_char(o,c);}

static int format(out_t *o,const char *fmt,va_list ap)
{
    for(;*fmt;++fmt){
        char tmp[24];const char *s;int left=0,zero=0,width=0,prec=-1,longs=0,len,neg=0;
        unsigned long u;unsigned base=10;int upper=0;
        if(*fmt!='%'){out_char(o,*fmt);continue;}
        ++fmt;
        for(;;++fmt){if(*fmt=='-')left=1;else if(*fmt=='0')zero=1;else if(*fmt==' '||*fmt=='+'||*fmt=='#'){}else break;}
        if(*fmt=='*'){width=va_arg(ap,int);++fmt;if(width<0){left=1;width=-width;}}
        else while(isdigit((uint8_t)*fmt))width=width*10+(*fmt++-'0');
        if(*fmt=='.'){++fmt;prec=0;if(*fmt=='*'){prec=va_arg(ap,int);++fmt;}else while(isdigit((uint8_t)*fmt))prec=prec*10+(*fmt++-'0');}
        while(*fmt=='l'||*fmt=='h'||*fmt=='z'){if(*fmt=='l')longs=1;++fmt;}
        switch(*fmt){
        case 0:return (int)o->len;
        case '%':out_char(o,'%');continue;
        case 'c':tmp[0]=(char)va_arg(ap,int);tmp[1]=0;s=tmp;len=1;goto emit;
        case 's':s=va_arg(ap,const char *);if(!s)s="(null)";len=(int)strlen(s);if(prec>=0&&len>prec)len=prec;goto emit;
        case 'd':case 'i':{long v=longs?va_arg(ap,long):va_arg(ap,int);if(v<0){neg=1;u=(unsigned long)(-v);}else u=(unsigned long)v;}goto number;
        case 'u':u=longs?va_arg(ap,unsigned long):va_arg(ap,unsigned);goto number;
        case 'X':upper=1;/* fall through */
        case 'x':u=longs?va_arg(ap,unsigned long):va_arg(ap,unsigned);base=16;goto number;
        case 'p':u=(unsigned long)(uintptr_t)va_arg(ap,void *);base=16;zero=1;width=8;goto number;
        default:out_char(o,'%');out_char(o,*fmt);continue;
        }
    number:
        {
            char *e=tmp+sizeof(tmp);int digits;
            *--e=0;
            do{unsigned d=(unsigned)(u%base);*--e=(char)(d<10?'0'+d:(upper?'A':'a')+d-10);u/=base;}while(u);
            digits=(int)(tmp+sizeof(tmp)-1-e);
            while(prec>digits){*--e='0';++digits;}
            if(neg){
                if(zero&&!left){out_char(o,'-');out_pad(o,'0',width-digits-1);s=e;len=digits;width=0;goto emit;}
                *--e='-';++digits;
            }
            s=e;len=digits;
            if(zero&&!left&&prec<0){out_pad(o,'0',width-len);width=0;}
        }
    emit:
        if(!left)out_pad(o,' ',width-len);
        {int i;for(i=0;i<len;++i)out_char(o,s[i]);}
        if(left)out_pad(o,' ',width-len);
    }
    return (int)o->len;
}

int vsnprintf(char *s,size_t n,const char *fmt,va_list ap)
{
    out_t o;int r;o.buf=s;o.cap=n;o.len=0;o.f=0;
    r=format(&o,fmt,ap);
    if(n)s[o.len<n?o.len:n-1]=0;
    return r;
}
int vsprintf(char *s,const char *fmt,va_list ap){return vsnprintf(s,0x7fffffff,fmt,ap);}
int snprintf(char *s,size_t n,const char *fmt,...){va_list ap;int r;va_start(ap,fmt);r=vsnprintf(s,n,fmt,ap);va_end(ap);return r;}
int sprintf(char *s,const char *fmt,...){va_list ap;int r;va_start(ap,fmt);r=vsprintf(s,fmt,ap);va_end(ap);return r;}

/* --------------------------------------------------------------------- files */

#define RBUF 4096u
#define MAX_FILES 8
struct atos_file {
    int used,console,writing,eof,err;
    int32_t fd;
    uint32_t size,pos;            /* logical position */
    uint32_t buf_at,buf_len;      /* buffer holds [buf_at, buf_at+buf_len) */
    uint8_t *buf;
    uint8_t *wbuf;uint32_t wlen,wcap;
    char path[96];
};
static struct atos_file files[MAX_FILES];
static struct atos_file console_out={1,1,1,0,0,-1,0,0,0,0,0,0,0,0,{0}};
FILE *stdout=&console_out,*stderr=&console_out,*stdin=0;

/* ATOS paths: backslashes, no drive letter (the boot volume), 63 chars. */
void atos_path(char *dst,size_t cap,const char *src)
{
    size_t n=0;
    if(isalpha((uint8_t)src[0])&&src[1]==':')src+=2;
    for(;*src&&n+1<cap;++src){
        char c=*src=='/'?'\\':*src;
        if(c=='\\'&&n&&dst[n-1]=='\\')continue;
        dst[n++]=(char)toupper((uint8_t)c);
    }
    dst[n]=0;
}

FILE *fopen(const char *path,const char *mode)
{
    int i;struct atos_file *f=0;
    for(i=0;i<MAX_FILES;++i)if(!files[i].used){f=&files[i];break;}
    if(!f)return 0;
    memset(f,0,sizeof(*f));
    atos_path(f->path,sizeof(f->path),path);
    if(mode[0]=='w'||mode[0]=='a'){
        f->writing=1;f->wcap=4096u;f->wbuf=(uint8_t *)malloc(f->wcap);
        if(!f->wbuf)return 0;
        f->used=1;f->fd=-1;
        return f;
    }
    {
        uint32_t info[2];
        if(atos_stat(f->path,info)<0||(info[1]&0x10u))return 0;
        f->fd=atos_open(f->path);
        if(f->fd<0)return 0;
        f->size=info[0];
    }
    f->buf=(uint8_t *)malloc(RBUF);
    if(!f->buf){(void)atos_close(f->fd);return 0;}
    f->used=1;
    return f;
}

int fclose(FILE *f)
{
    int r=0;
    if(!f||f->console)return 0;
    if(f->writing){
        if(atos_save(f->path,f->wbuf,f->wlen)<0)r=EOF;
        free(f->wbuf);
    }else{
        (void)atos_close(f->fd);
        free(f->buf);
    }
    f->used=0;
    return r;
}

static int raw_read_at(FILE *f,uint32_t at,void *dst,uint32_t n,uint32_t *got)
{
    int32_t r;
    if(atos_seek(f->fd,at)<0)return 0;
    r=atos_read(f->fd,dst,n);
    if(r<0)return 0;
    *got=(uint32_t)r;
    return 1;
}

size_t fread(void *ptr,size_t size,size_t n,FILE *f)
{
    uint8_t *d=(uint8_t *)ptr;uint32_t want=(uint32_t)(size*n),done=0;
    if(!f||f->writing||!want)return 0;
    while(done<want){
        uint32_t left=want-done;
        if(f->pos>=f->size){f->eof=1;break;}
        if(f->pos>=f->buf_at&&f->pos<f->buf_at+f->buf_len){
            uint32_t k=f->buf_at+f->buf_len-f->pos;if(k>left)k=left;
            memcpy(d+done,f->buf+(f->pos-f->buf_at),k);
            done+=k;f->pos+=k;continue;
        }
        if(left>=RBUF){
            uint32_t got=0;
            if(!raw_read_at(f,f->pos,d+done,left,&got)||!got){f->err=1;break;}
            done+=got;f->pos+=got;continue;
        }
        {
            uint32_t got=0;
            if(!raw_read_at(f,f->pos,f->buf,RBUF,&got)||!got){f->err=1;break;}
            f->buf_at=f->pos;f->buf_len=got;
        }
    }
    return size?done/size:0;
}

size_t fwrite(const void *ptr,size_t size,size_t n,FILE *f)
{
    uint32_t len=(uint32_t)(size*n);
    if(!f||!len)return 0;
    if(f->console){
        char line[128];uint32_t i=0;const char *s=(const char *)ptr;
        while(i<len){uint32_t k=len-i;if(k>sizeof(line)-1u)k=sizeof(line)-1u;memcpy(line,s+i,k);line[k]=0;atos_puts(line);i+=k;}
        return n;
    }
    if(!f->writing)return 0;
    if(f->pos+len>f->wcap){
        uint32_t cap=f->wcap;uint8_t *nb;
        while(f->pos+len>cap)cap*=2u;
        nb=(uint8_t *)realloc(f->wbuf,cap);
        if(!nb){f->err=1;return 0;}
        f->wbuf=nb;f->wcap=cap;
    }
    memcpy(f->wbuf+f->pos,ptr,len);
    f->pos+=len;if(f->pos>f->wlen)f->wlen=f->pos;
    return n;
}

int fseek(FILE *f,long off,int whence)
{
    long base;
    if(!f||f->console)return -1;
    base=whence==SEEK_SET?0:whence==SEEK_CUR?(long)f->pos:(long)(f->writing?f->wlen:f->size);
    if(base+off<0)return -1;
    f->pos=(uint32_t)(base+off);f->eof=0;
    return 0;
}
long ftell(FILE *f){return f?(long)f->pos:-1;}
int feof(FILE *f){return f?f->eof:1;}
int fflush(FILE *f){(void)f;return 0;}
int fgetc(FILE *f){uint8_t c;return fread(&c,1,1,f)==1?c:EOF;}
char *fgets(char *s,int n,FILE *f)
{
    int i=0,c=0;
    if(n<=0)return 0;
    while(i<n-1&&(c=fgetc(f))!=EOF){s[i++]=(char)c;if(c=='\n')break;}
    if(!i&&c==EOF)return 0;
    s[i]=0;return s;
}
int fputc(int c,FILE *f){char ch=(char)c;return fwrite(&ch,1,1,f)==1?c:EOF;}
int fputs(const char *s,FILE *f){return fwrite(s,1,strlen(s),f)?0:EOF;}
int puts(const char *s){atos_puts(s);atos_puts("\n");return 0;}
int putchar(int c){char t[2];t[0]=(char)c;t[1]=0;atos_puts(t);return c;}
int vfprintf(FILE *f,const char *fmt,va_list ap)
{
    char buf[512];int r=vsnprintf(buf,sizeof(buf),fmt,ap);
    (void)fwrite(buf,1,strlen(buf),f);
    return r;
}
int vprintf(const char *fmt,va_list ap){return vfprintf(stdout,fmt,ap);}
int fprintf(FILE *f,const char *fmt,...){va_list ap;int r;va_start(ap,fmt);r=vfprintf(f,fmt,ap);va_end(ap);return r;}
int printf(const char *fmt,...){va_list ap;int r;va_start(ap,fmt);r=vfprintf(stdout,fmt,ap);va_end(ap);return r;}
int remove(const char *path){(void)path;return -1;}

/* --------------------------------------------------------------------- exit */

void exit(int status)
{
    atos_port_shutdown();
    atos_log_flush();
    (void)atos_call(ATOS_EXIT,(uint32_t)status,0,0,0);
    for(;;){}
}
void abort(void){exit(3);}
void atos_assert_fail(const char *expr,const char *file,int line)
{
    char b[200];
    snprintf(b,sizeof(b),"FLASHBACK: assertion '%s' failed at %s:%d\n",expr,file,line);
    atos_puts(b);
    exit(2);
}

/* ------------------------------------------------------------------ startup */

typedef void (*ctor_t)(void);
extern ctor_t __init_array_start[],__init_array_end[],__ctors_start[],__ctors_end[];

int atos_crt_start(void)
{
    ctor_t *c;
    if(!atos_heap_init()){atos_puts("FLASHBACK: not enough memory for the heap\n");return 1;}
    for(c=__ctors_end;c>__ctors_start;)(*--c)();
    for(c=__init_array_start;c<__init_array_end;++c)(*c)();
    {
        int r=atos_main();
        atos_port_shutdown();
        atos_log_flush();
        return r;
    }
}
