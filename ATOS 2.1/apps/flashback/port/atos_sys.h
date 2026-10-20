/* ATOS native application calls used by the Flashback port (docs/APP-ABI.md). */
#ifndef ATOS_SYS_H
#define ATOS_SYS_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* crt0.S: trap #0 with d0=number, d1, d2, a0, a1; result in d0. */
int32_t atos_call(uint32_t number,uint32_t d1,uint32_t d2,const void *a0,const void *a1);

enum {
    ATOS_EXIT=0,ATOS_WRITE=1,ATOS_OPEN=2,ATOS_READ=3,ATOS_CLOSE=4,ATOS_STAT=5,ATOS_SEEK=6,
    ATOS_ARGS=7,ATOS_TICKS=10,ATOS_SLEEP=11,ATOS_GETCWD=14,ATOS_ALLOC=16,ATOS_FREE=17,
    ATOS_GETPID=18,ATOS_SEND=36,ATOS_RECV=37,ATOS_RECV_WAIT=38,ATOS_SERVICE_GET=40,
    ATOS_MKDIR=50,ATOS_SAVE=60,ATOS_ABI_VERSION=66,ATOS_HIRES=67
};

typedef struct {uint32_t sender,v[4];} atos_message_t;

static inline int32_t atos_open(const char *path){return atos_call(ATOS_OPEN,0,0,path,0);}
static inline int32_t atos_read(int32_t fd,void *buf,uint32_t n){return atos_call(ATOS_READ,(uint32_t)fd,n,buf,0);}
static inline int32_t atos_close(int32_t fd){return atos_call(ATOS_CLOSE,(uint32_t)fd,0,0,0);}
static inline int32_t atos_seek(int32_t fd,uint32_t off){return atos_call(ATOS_SEEK,(uint32_t)fd,off,0,0);}
static inline int32_t atos_stat(const char *path,uint32_t info[2]){return atos_call(ATOS_STAT,0,0,path,info);}
static inline int32_t atos_save(const char *path,const void *buf,uint32_t n){return atos_call(ATOS_SAVE,n,0,path,buf);}
static inline int32_t atos_mkdir(const char *path){return atos_call(ATOS_MKDIR,0,0,path,0);}
static inline void *atos_alloc(uint32_t n,int fast){int32_t r=atos_call(ATOS_ALLOC,n,fast?1u:0u,0,0);return r<0?0:(void *)(uintptr_t)r;}
static inline uint32_t atos_hires(void){return (uint32_t)atos_call(ATOS_HIRES,0,0,0,0);}
static inline void atos_sleep_ticks(uint32_t t){(void)atos_call(ATOS_SLEEP,t,0,0,0);}
static inline int32_t atos_lookup(const char *name){return atos_call(ATOS_SERVICE_GET,0,0,name,0);}
static inline int32_t atos_send(uint32_t pid,uint32_t a,uint32_t b,uint32_t c,uint32_t d)
{uint32_t v[4];v[0]=a;v[1]=b;v[2]=c;v[3]=d;return atos_call(ATOS_SEND,pid,0,v,0);}
static inline int32_t atos_receive(atos_message_t *m,int wait){return atos_call(wait?ATOS_RECV_WAIT:ATOS_RECV,0,0,m,0);}
static inline int32_t atos_getpid(void){return atos_call(ATOS_GETPID,0,0,0,0);}

/* 12288 counts per second (ABI 1.1 HIRES_CLOCK). */
#define ATOS_HIRES_HZ 12288u

/* Console output (the session's terminal) - for diagnostics before/after the
   game owns the screen. */
void atos_puts(const char *s);

#ifdef __cplusplus
}
#endif
#endif
