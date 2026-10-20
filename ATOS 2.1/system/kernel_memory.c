#include "kernel_memory.h"

#define BLOCK_MAGIC 0x4b4d454dUL
#define FREE_OWNER 0u
#define KMEM_PINNED 0x8000u
typedef struct block {uint32_t magic,size;uint16_t owner,flags;struct block *next;uint32_t guard,reserved[3];} block_t;
typedef struct {block_t *first;uint32_t total,reserved,end;} pool_t;
static pool_t st_pool,tt_pool;
static void stats(const pool_t *p,kmem_stats_t *s);
/* Diagnostic: how many times a full block-list traversal was started.  An
   allocation currently walks the list to size the pool (stats) and again to
   place the block; making that countable is the first step to halving it. */
uint32_t kmem_list_walks;

static uint32_t align_up(uint32_t value,uint32_t alignment){return (value+alignment-1u)&~(alignment-1u);}
static void pool_init(pool_t *p,uint32_t start,uint32_t end,uint32_t total)
{
    start=align_up(start,16u);end&=~15UL;p->total=total;p->reserved=total-(end>start?end-start:0u);p->first=0;p->end=end;
    if(end>start+sizeof(block_t)+16u){p->first=(block_t *)(uintptr_t)start;p->first->magic=BLOCK_MAGIC;p->first->size=end-start-sizeof(block_t);p->first->owner=FREE_OWNER;p->first->flags=0;p->first->next=0;p->first->guard=~BLOCK_MAGIC;}
}
int kmem_init(uint32_t st_total,uint32_t tt_total,uint32_t kernel_end)
{
    uint32_t st_limit=st_total>0x4000u?st_total-0x4000u:0u;
    pool_init(&st_pool,kernel_end,st_limit,st_total);
    {uint32_t tt_end=0x01000000UL+tt_total;
     /* Leave the top 16 MB of 64 MB TT-RAM for the TOS personality TPA. */
     if(tt_end>0x04000000UL&&tt_total>=0x03000000UL)tt_end=0x04000000UL;
     pool_init(&tt_pool,0x01000000UL,tt_end,tt_total);}
    return st_pool.first!=0;
}
static void pool_heal(pool_t *p)
{
    block_t *last;uint32_t used,tail,guard2=0u;
    if(!p->first||!p->end)return;
    last=p->first;
    while(last->next&&++guard2<4096u)last=last->next;
    used=(uint32_t)(uintptr_t)(last+1)+last->size;
    if(used+sizeof(block_t)+16u>p->end)return;
    tail=p->end-used;
    {block_t *g=(block_t *)(uintptr_t)used;
     g->magic=BLOCK_MAGIC;g->guard=~BLOCK_MAGIC;g->owner=FREE_OWNER;g->flags=0;
     g->size=tail-sizeof(block_t);g->next=0;
     last->next=g;}
}
static void *pool_alloc(pool_t *p,uint32_t bytes,uint32_t alignment,uint16_t owner,uint16_t flags)
{
    block_t *b;if(!bytes||!owner)return 0;
    pool_heal(p);if(alignment<16u)alignment=16u;if(alignment&(alignment-1u))return 0;bytes=align_up(bytes,16u);
    ++kmem_list_walks;
    for(b=p->first;b;b=b->next)if(b->magic==BLOCK_MAGIC&&b->owner==FREE_OWNER){if(b->guard!=~BLOCK_MAGIC)b->guard=~BLOCK_MAGIC;uint32_t data=(uint32_t)(uintptr_t)(b+1),aligned=align_up(data,alignment),gap=aligned-data;block_t *a=b;
        if(gap&&gap<sizeof(block_t)+16u){aligned+=alignment;gap=aligned-data;}
        if(b->size<gap+bytes)continue;
        if(gap){a=(block_t *)(uintptr_t)(aligned-sizeof(block_t));a->magic=BLOCK_MAGIC;a->size=b->size-gap;a->owner=FREE_OWNER;a->flags=0;a->next=b->next;a->guard=~BLOCK_MAGIC;b->size=gap-sizeof(block_t);b->next=a;}
        if(a->size>=bytes+sizeof(block_t)+16u){block_t *n=(block_t *)(uintptr_t)((uint8_t *)(a+1)+bytes);n->magic=BLOCK_MAGIC;n->size=a->size-bytes-sizeof(block_t);n->owner=FREE_OWNER;n->flags=0;n->next=a->next;n->guard=~BLOCK_MAGIC;a->next=n;a->size=bytes;}
        a->owner=owner;a->flags=flags;return a+1;}
    return 0;
}
void *kmem_alloc(uint32_t bytes,uint32_t alignment,uint16_t owner,uint16_t flags,int prefer_tt)
{void *p=0;if(prefer_tt)p=pool_alloc(&tt_pool,bytes,alignment,owner,flags);if(!p)p=pool_alloc(&st_pool,bytes,alignment,owner,flags);return p;}

static void *pool_alloc_reserved(pool_t *p,uint32_t bytes,uint32_t alignment,
                                 uint16_t owner,uint16_t flags,uint32_t reserve)
{
    kmem_stats_t s;uint32_t charged;
    if(!p->first)return 0;
    if(alignment<16u)alignment=16u;
    if(alignment&(alignment-1u))return 0;
    stats(p,&s);
    if(bytes>0xffffffffu-(alignment-1u))return 0;
    charged=align_up(bytes,alignment);
    /* A split consumes another allocator header. Keeping that header in the
       calculation makes the reserve conservative even on fragmented pools. */
    if(charged>0xffffffffu-(uint32_t)sizeof(block_t))return 0;
    charged+=(uint32_t)sizeof(block_t);
    if(s.free<reserve||charged>s.free-reserve||s.largest<bytes)return 0;
    return pool_alloc(p,bytes,alignment,owner,flags);
}

void *kmem_alloc_user(uint32_t bytes,uint32_t alignment,uint16_t owner,uint16_t flags,int prefer_tt)
{
    void *p=0;
    if(prefer_tt)p=pool_alloc_reserved(&tt_pool,bytes,alignment,owner,flags,64u*1024u);
    if(!p)p=pool_alloc_reserved(&st_pool,bytes,alignment,owner,flags,256u*1024u);
    return p;
}
uint32_t kmem_user_largest(int tt)
{
    pool_t *p=tt?&tt_pool:&st_pool;kmem_stats_t s;
    uint32_t reserve=(tt?64u:256u)*1024u+(uint32_t)sizeof(block_t),n;
    stats(p,&s);
    if(s.free<=reserve)return 0u;
    n=s.free-reserve;if(n>s.largest)n=s.largest;
    return n&~15u;
}
void *kmem_alloc_user_pool(uint32_t bytes,uint16_t owner,int tt)
{
    if(!bytes||bytes>kmem_user_largest(tt))return 0;
    return pool_alloc(tt?&tt_pool:&st_pool,bytes,16u,owner,KMEM_READ|KMEM_WRITE);
}
int kmem_shrink(void *pointer,uint32_t bytes,uint16_t owner)
{
    pool_t *p=(uintptr_t)pointer>=0x01000000u?&tt_pool:&st_pool;
    block_t *b,*n;
    if(!pointer||!bytes||bytes>0xfffffff0u)return 0;
    bytes=align_up(bytes,16u);
    for(b=p->first;b;b=b->next)if((void *)(b+1)==pointer)break;
    if(!b||b->magic!=BLOCK_MAGIC||b->owner!=owner||bytes>b->size)return 0;
    if(b->size>=bytes+sizeof(block_t)+16u){
        n=(block_t *)((uint8_t *)(b+1)+bytes);
        n->magic=BLOCK_MAGIC;n->guard=~BLOCK_MAGIC;n->size=b->size-bytes-sizeof(block_t);
        n->owner=FREE_OWNER;n->flags=0;n->next=b->next;
        b->next=n;b->size=bytes;
        while(n->next&&n->next->owner==FREE_OWNER&&
              (uint8_t *)(n+1)+n->size==(uint8_t *)n->next){
            n->size+=sizeof(block_t)+n->next->size;n->next=n->next->next;
        }
    }
    return 1;
}

static int pool_free(pool_t *p,void *pointer,uint16_t owner)
{
    block_t *b,*n;if(!pointer)return 0;b=((block_t *)pointer)-1;if(b->magic!=BLOCK_MAGIC||b->guard!=~BLOCK_MAGIC||b->owner!=owner||owner==FREE_OWNER)return 0;b->owner=FREE_OWNER;b->flags=0;
    for(b=p->first;b;b=b->next){while((n=b->next)!=0&&b->owner==FREE_OWNER&&n->owner==FREE_OWNER&&(uint8_t *)(b+1)+b->size==(uint8_t *)n){b->size+=sizeof(block_t)+n->size;b->next=n->next;}}
    return 1;
}
int kmem_free(void *pointer,uint16_t owner)
{block_t *b;if(!pointer)return 0;b=((block_t *)pointer)-1;if(b->flags&KMEM_PINNED)return 0;
 {uint32_t a=(uint32_t)(uintptr_t)pointer;if(a>=0x01000000UL)return pool_free(&tt_pool,pointer,owner);return pool_free(&st_pool,pointer,owner);}}
int kmem_pin(void *pointer,uint16_t owner)
{block_t *b;if(!pointer||!owner)return 0;b=((block_t *)pointer)-1;
 if(b->magic!=BLOCK_MAGIC||b->guard!=~BLOCK_MAGIC||b->owner!=owner)return 0;
 b->flags|=KMEM_PINNED;return 1;}
int kmem_release(void *pointer,uint16_t owner)
{block_t *b;uint32_t a;if(!pointer||!owner)return 0;b=((block_t *)pointer)-1;
 if(b->magic!=BLOCK_MAGIC||b->guard!=~BLOCK_MAGIC||b->owner!=owner)return 0;
 b->flags&=(uint16_t)~KMEM_PINNED;a=(uint32_t)(uintptr_t)pointer;
 return a>=0x01000000UL?pool_free(&tt_pool,pointer,owner):pool_free(&st_pool,pointer,owner);}
int kmem_change_owner(void *pointer,uint16_t old_owner,uint16_t new_owner)
{
    block_t *b;
    if(!pointer||old_owner==FREE_OWNER||new_owner==FREE_OWNER)return 0;
    b=((block_t *)pointer)-1;
    if(b->magic!=BLOCK_MAGIC||b->guard!=~BLOCK_MAGIC||b->owner!=old_owner)return 0;
    b->owner=new_owner;return 1;
}
uint16_t kmem_owner(const void *pointer)
{
    const block_t *b;if(!pointer)return FREE_OWNER;b=((const block_t *)pointer)-1;
    if(b->magic!=BLOCK_MAGIC||b->guard!=~BLOCK_MAGIC)return FREE_OWNER;
    return b->owner;
}
static void stats(const pool_t *p,kmem_stats_t *s){const block_t *b;++kmem_list_walks;s->total=p->total;s->reserved=p->reserved;s->free=0;s->largest=0;for(b=p->first;b;b=b->next)if(b->magic==BLOCK_MAGIC&&b->owner==FREE_OWNER){s->free+=b->size;if(b->size>s->largest)s->largest=b->size;}}
void kmem_get_st_stats(kmem_stats_t *s){stats(&st_pool,s);}void kmem_get_tt_stats(kmem_stats_t *s){stats(&tt_pool,s);}
volatile uint32_t kmem_audit[1+3*24+8];
void kmem_report(void (*puts)(const char *),void (*put_u32)(uint32_t))
{
    uint16_t owners[24];uint32_t sums[24];uint32_t counts[24];unsigned n=0,pi;pool_t *pools[2]={&st_pool,&tt_pool};
    kmem_stats_t st,tt;
    if(!puts||!put_u32)return;
    for(pi=0;pi<2u;++pi){block_t *b=pools[pi]->first;unsigned g=0,i;
        for(;b&&g<16384u;b=b->next,++g){
            if(b->magic!=BLOCK_MAGIC||b->owner==FREE_OWNER)continue;
            for(i=0;i<n;++i)if(owners[i]==b->owner)break;
            if(i==n&&n<24u){owners[n]=b->owner;sums[n]=0;counts[n]=0;++n;}
            if(i<n){sums[i]+=b->size;++counts[i];}
        }
    }
    for(pi=0;pi<n;++pi){puts("KMEM ");put_u32(owners[pi]);puts(" ");put_u32(sums[pi]);puts(" B x");put_u32(counts[pi]);puts("\n");}
    kmem_audit[0]=n;
    for(pi=0;pi<n&&pi<24u;++pi){kmem_audit[1+3*pi]=owners[pi];kmem_audit[2+3*pi]=sums[pi];kmem_audit[3+3*pi]=counts[pi];}
    stats(&st_pool,&st);stats(&tt_pool,&tt);
    kmem_audit[1+3*24]=st.total;kmem_audit[2+3*24]=st.free;kmem_audit[3+3*24]=st.reserved;kmem_audit[4+3*24]=st.largest;
    kmem_audit[5+3*24]=tt.total;kmem_audit[6+3*24]=tt.free;kmem_audit[7+3*24]=tt.reserved;kmem_audit[8+3*24]=tt.largest;
}
int kmem_self_test(void){void *a=kmem_alloc(256u,256u,KMEM_OWNER_KERNEL,KMEM_READ|KMEM_WRITE,0),*b=kmem_alloc(1024u,16u,KMEM_OWNER_KERNEL,KMEM_READ|KMEM_WRITE,0);if(!a||!b||((uint32_t)(uintptr_t)a&255u))return 0;if(!kmem_change_owner(a,KMEM_OWNER_KERNEL,2u)||kmem_free(a,KMEM_OWNER_KERNEL)||!kmem_free(a,2u)||kmem_free(b,2u)||!kmem_free(b,KMEM_OWNER_KERNEL))return 0;return 1;}
