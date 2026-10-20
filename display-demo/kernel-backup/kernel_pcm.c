/* Kernel-owned, bounded PCM stream. Timer A is exclusively reserved while open.
 * The producer copies U8 mono into a permanent kernel ring as YM volume triplets.
 * IRQ consumes only this ring; no caller pointer, mixer, allocation or callback.
 * All ownership transitions disable IRQs. Bulk conversion does NOT mask IRQs. */
#include "kernel_pcm.h"
#include "kernel_audio.h"
#include "kernel_memory.h"
#include "irq.h"
#include "psg.h"
#include "pcm_lut.h"
#ifdef KPCM_HOST_TEST
extern uint8_t kpcm_host_regs[256];
extern uint32_t kpcm_host_vector;
#define R8(a) kpcm_host_regs[(a)&255u]
#define VECTOR kpcm_host_vector
#else
#define R8(a) (*(volatile uint8_t *)(uintptr_t)(a))
static volatile uint32_t *vector_address(void){uintptr_t p=0x134u;__asm__ volatile("" : "+r"(p));return (volatile uint32_t *)p;}
#define VECTOR (*vector_address())
#endif
#define BUSY (-0x3f0)
#define ARG (-0x3f1)
volatile uint32_t kpcm_owner,kpcm_active,kpcm_read,kpcm_write;
volatile uint32_t kpcm_played,kpcm_underruns;
/* Ring w TT-RAM: w ST-RAM jest tylko 4 MB, a bufor ma 32 KB i jest dotykany
   wylacznie przez CPU (przerwanie Timer A czytajace ring to tez CPU). */
uint32_t *kpcm_ring;
static uint32_t generation,saved_vector;
uint8_t *kpcm_clip;
volatile uint32_t kpcm_clip_pos,kpcm_clip_size,kpcm_clip_written;
static uint8_t saved_data;
extern void kpcm_irq(void);
extern volatile uint32_t psg_pcm_sample;
extern volatile uint8_t psg_pcm_active;
#ifdef KPCM_HOST_TEST
static uint16_t lock(void){return 0;}
static void unlock(uint16_t sr){(void)sr;}
#else
static uint16_t lock(void){uint16_t sr;__asm__ volatile("move.w %%sr,%0; ori.w #0x0700,%%sr":"=d"(sr)::"memory","cc");return sr;}
static void unlock(uint16_t sr){__asm__ volatile("move.w %0,%%sr"::"d"(sr):"memory","cc");}
#endif
/* Resource reservation marker. Actual Timer A fast ISR is in pcm_irq.S. */
static void reservation(irq_source_t s,void *p){(void)s;(void)p;}
static void silence(void){unsigned c;for(c=0;c<3;c++)psg_write((uint8_t)(8+c),0);}
static void halt(void)
{
    R8(0xfffffa19)=0;R8(0xfffffa13)&=(uint8_t)~0x20u;
    R8(0xfffffa07)&=(uint8_t)~0x20u;
    R8(0xfffffa0b)=0xdf;R8(0xfffffa0f)=0xdf;
    kpcm_active=0;psg_pcm_active=0;silence();
}
int kpcm_owned(void){return kpcm_owner!=0;}
void kpcm_release(uint32_t pid)
{
    uint16_t sr=lock();
    if(pid&&pid==kpcm_owner){
        halt();VECTOR=saved_vector;R8(0xfffffa1f)=saved_data;
        (void)irq_unregister(IRQ_SRC_MFP_A,reservation);
        if(kpcm_clip)kmem_free(kpcm_clip,KMEM_OWNER_KERNEL);
        kpcm_clip=0;kpcm_clip_size=kpcm_clip_pos=kpcm_clip_written=0;
        kpcm_owner=0;kpcm_read=kpcm_write=0;++generation;
    }
    unlock(sr);
}
int32_t kpcm_control(uint32_t pid,unsigned session,unsigned cmd)
{
    int32_t rc=0;uint16_t sr;
    if(!pid||cmd>3)return ARG;
    sr=lock();
    if(cmd==0){
        if(kpcm_owner){rc=kpcm_owner==pid?(int32_t)KPCM_RATE:BUSY;goto done;}
        /* Ring alokujemy PRZED przejeciem Timer A, zeby nie trzeba bylo cofac. */
        if(!kpcm_ring){kpcm_ring=(uint32_t *)kmem_alloc((uint32_t)KPCM_CAPACITY*(uint32_t)sizeof(uint32_t),
            16u,KMEM_OWNER_KERNEL,KMEM_READ|KMEM_WRITE,1);if(!kpcm_ring){rc=ARG;goto done;}}
        /* Never steal Timer A, a vector owned by a registered driver, or PSG. */
        if((R8(0xfffffa19)&15u)||(R8(0xfffffa07)&0x20u)||
           (R8(0xfffffa13)&0x20u)||irq_handler_count(IRQ_SRC_MFP_A)||
           (R8(0xfffffa17)&0xf0u)!=0x40u||kaudio_owner()) {rc=BUSY;goto done;}
        if(!irq_register(IRQ_SRC_MFP_A,reservation,0)){rc=BUSY;goto done;}
        rc=kaudio_claim(pid,session);
        if(rc<0){(void)irq_unregister(IRQ_SRC_MFP_A,reservation);goto done;}
        saved_vector=VECTOR;saved_data=R8(0xfffffa1f);
        VECTOR=(uint32_t)(uintptr_t)kpcm_irq;
        kpcm_read=kpcm_write=kpcm_played=kpcm_underruns=0;
        ++generation;kpcm_owner=pid;kpcm_active=0;
        /* Disable tone and noise generators, preserve I/O direction bits. */
        psg_mixer_modify(0,0x3f);silence();rc=KPCM_RATE;
    }else if(kpcm_owner!=pid){rc=BUSY;}
    else if(cmd==1){
        if(!kpcm_active){
            if(kpcm_clip?(kpcm_clip_written!=kpcm_clip_size):(kpcm_write==kpcm_read)){rc=-0x3f3;goto done;}
            psg_mixer_modify(0,0x3f);
            R8(0xfffffa19)=0;R8(0xfffffa1f)=50; /* 2457600 / 4 / 50 = 12288 Hz */
            R8(0xfffffa0b)=0xdf;R8(0xfffffa0f)=0xdf;
            psg_pcm_sample=kpcm_lut[128];psg_pcm_active=1;kpcm_active=1;
            R8(0xfffffa07)|=0x20;R8(0xfffffa13)|=0x20;R8(0xfffffa19)=1;
        }
    }else if(cmd==2){halt();} /* keep unread samples, preserving pause position */
    else {kaudio_release(pid);} /* also calls kpcm_release before owner changes */
done:unlock(sr);return rc;
}
int32_t kpcm_submit(uint32_t pid,const uint8_t *data,uint32_t count)
{
    uint32_t i,w,g,free;uint16_t sr;
    if(!data||!count||count>KPCM_MAX_SUBMIT||!kpcm_ring)return ARG;
    sr=lock();
    if(!pid||pid!=kpcm_owner){unlock(sr);return BUSY;}
    if(kpcm_clip){
        uint8_t *clip=kpcm_clip;
        if(kpcm_active){unlock(sr);return BUSY;}
        w=kpcm_clip_written;g=generation;free=kpcm_clip_size-w;
        if(count>free)count=free;
        unlock(sr);
        for(i=0;i<count;i++)clip[w+i]=data[i];
        sr=lock();
        if(g!=generation||pid!=kpcm_owner||clip!=kpcm_clip){unlock(sr);return BUSY;}
        kpcm_clip_written=w+count;unlock(sr);return (int32_t)count;
    }
    w=kpcm_write;g=generation;free=KPCM_CAPACITY-(w-kpcm_read);
    if(count>free)count=free;
    unlock(sr);
    /* Consumer cannot read these slots before publication of kpcm_write. */
    for(i=0;i<count;i++)kpcm_ring[(w+i)&(KPCM_CAPACITY-1u)]=kpcm_lut[data[i]];
    sr=lock();
    if(g!=generation||pid!=kpcm_owner||w!=kpcm_write){unlock(sr);return BUSY;}
    kpcm_write=w+count;unlock(sr);return (int32_t)count;
}
void kpcm_state(uint32_t *s)
{
    uint16_t sr=lock();uint32_t n=kpcm_write-kpcm_read;
    s[0]=kpcm_owner;s[1]=kpcm_active;s[2]=n;s[3]=KPCM_CAPACITY-n;
    if(kpcm_clip){s[2]=kpcm_clip_written-kpcm_clip_pos;s[3]=kpcm_clip_size-kpcm_clip_written;}
    s[4]=kpcm_played;s[5]=kpcm_underruns;s[6]=KPCM_RATE;s[7]=kpcm_clip?kpcm_clip_size:KPCM_CAPACITY;
    unlock(sr);
}

/* Optional pre-rendered clip, for players that must survive modal UI stalls.
 * Owned kernel storage, filled incrementally; immutable once fully uploaded.
 * Pause preserves position. Play loops the complete clip until close/fault. */
int32_t kpcm_clip_create(uint32_t pid,uint32_t bytes)
{
    uint8_t *p;uint32_t g;uint16_t sr;
    if(!bytes||bytes>8u*1024u*1024u)return ARG;
    sr=lock();
    if(!pid||pid!=kpcm_owner||kpcm_active||kpcm_clip||kpcm_write!=kpcm_read){unlock(sr);return BUSY;}
    g=generation;unlock(sr);
    p=kmem_alloc(bytes,16,KMEM_OWNER_KERNEL,KMEM_READ|KMEM_WRITE,1);
    if(!p)return -0x3f4;
    sr=lock();
    if(g!=generation||pid!=kpcm_owner||kpcm_active||kpcm_clip){
        kmem_free(p,KMEM_OWNER_KERNEL);unlock(sr);return BUSY;}
    kpcm_clip_size=bytes;kpcm_clip_pos=kpcm_clip_written=0;kpcm_clip=p;
    unlock(sr);return 0;
}
