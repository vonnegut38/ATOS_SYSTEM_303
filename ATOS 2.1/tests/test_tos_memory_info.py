#!/usr/bin/env python3
"""Check pool limits and guest ownership without running a whole desktop."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'system/tos_prg.c').read_text();m=(root/'system/kernel_memory.c').read_text()
a=s.index('#define TOS_ALLOC_SLOTS');b=s.index('static uint16_t tos_bpb',a)
# The real persona functions call a deterministic two-pool allocator here.
code=r'''
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <stdio.h>
#define KMEM_OWNER_KERNEL 1
static unsigned frees,shrinks,which;
static uint8_t *tos_current_basepage;
static uint32_t tos_process_limit;
static unsigned char stmem[256],ttmem[256];
static uint32_t available[2]={128,256};
static uint32_t kmem_user_largest(int tt){return available[tt];}
static void *kmem_alloc_user_pool(uint32_t n,uint16_t owner,int tt){
 assert(owner==1);which=tt;if(n>available[tt])return 0;return tt?ttmem:stmem;
}
static int kmem_free(void *p,uint16_t o){assert(p&&o==1);frees++;return 1;}
static int kmem_shrink(void *p,uint32_t n,uint16_t o){assert(p&&n&&o==1);shrinks++;return 1;}
/* Wyciety fragment tos_prg.c wola hak Dosound, ktory doszedl PO napisaniu tego
   testu (prawdziwa deklaracja jest w include/kernel_audio.h, ktorego generowana
   persona nie includuje).  Nieruchoma zaslepka utrzymuje fragment linkowalnym. */
static void kaudio_dosound_revalidate(const uint8_t *a,const uint8_t *b,const uint8_t *c)
{(void)a;(void)b;(void)c;}
'''+s[a:b]+r'''
int main(void){
 uint8_t proc1,proc2;void *p;
 tos_current_basepage=&proc1;
 assert(tos_largest_free()==128&&tos_largest_mode(1)==256);
 assert(tos_largest_mode(2)==256&&tos_largest_mode(3)==256);
 assert(!tos_mxalloc(129,0)&&!tos_mxalloc(257,1));
 p=tos_mxalloc(129,2);assert(p==ttmem&&which==1);
 assert(tos_mshrink(p,130)==-39&&tos_mshrink(p,64)==0&&shrinks==1);
 tos_current_basepage=&proc2;
 assert(tos_mfree(p)==-40&&tos_mshrink(p,32)==-40&&frees==0);
 tos_alloc_cleanup(&proc2);assert(frees==0);
 tos_current_basepage=&proc1;
 assert(tos_mfree(p)==0&&frees==1&&tos_mfree(p)==-40);
 available[1]=0;p=tos_mxalloc(100,3);assert(p==stmem&&which==0);
 assert(tos_mfree(p)==0&&frees==2);
 assert(!tos_malloc(0)&&!tos_malloc(0xfffffff0u));
 for(unsigned i=0;i<TOS_ALLOC_SLOTS;i++)tos_allocs[i].ptr=(void*)1;
 assert(tos_largest_free()==0&&!tos_malloc(1));
 puts("TOS Malloc/Mxalloc: separate pools, preference fallback, query, owner checks, shrink/free, slot limit: PASS");
}
'''
# Actual kmem query/allocation boundary with representative fragmented pools.
a=m.index('uint32_t kmem_user_largest(');b=m.index('int kmem_shrink(',a)
limits=r'''
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#define KMEM_READ 1
#define KMEM_WRITE 2
typedef struct {uint32_t total,free,reserved,largest;} kmem_stats_t;
typedef struct {unsigned char h[32];} block_t;
typedef struct {kmem_stats_t s;} pool_t;
static pool_t st_pool,tt_pool;
static void stats(pool_t*p,kmem_stats_t*s){*s=p->s;}
static void *pool_alloc(pool_t*p,uint32_t n,uint32_t a,uint16_t o,uint16_t f){assert(a==16&&o==1&&f==3&&n<=p->s.largest);return p;}
'''+m[a:b]+r'''
int main(void){
 st_pool.s.free=300000;st_pool.s.largest=100000;
 assert(kmem_user_largest(0)==37824);
 assert(kmem_alloc_user_pool(37824,1,0)==&st_pool);
 assert(!kmem_alloc_user_pool(37825,1,0));
 st_pool.s.largest=17;assert(kmem_user_largest(0)==16);
 tt_pool.s.free=65568;tt_pool.s.largest=65568;
 assert(kmem_user_largest(1)==0&&!kmem_alloc_user_pool(1,1,1));
 puts("Kernel user pool query agrees with reserve/alignment allocation boundaries: PASS");
}
'''
a=m.index('int kmem_shrink(');b=m.index('static int pool_free(',a)
shrink=r'''
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#define BLOCK_MAGIC 0x4b4d454dU
#define FREE_OWNER 0
#define KMEM_OWNER_KERNEL 1
typedef struct block {uint32_t magic,size;uint16_t owner,flags;struct block *next;uint32_t guard,reserved[3];} block_t;
typedef struct {block_t *first;} pool_t;
static pool_t st_pool,tt_pool;
static uint32_t align_up(uint32_t v,uint32_t a){return (v+a-1)&~(a-1);}
'''+m[a:b]+r'''
int main(void){
 _Alignas(16) unsigned char arena[2048]={0};
 block_t *b=(block_t*)arena,*n=(block_t*)((unsigned char*)(b+1)+512);
 b->magic=BLOCK_MAGIC;b->owner=1;b->size=512;b->next=n;
 n->magic=BLOCK_MAGIC;n->owner=0;n->size=256;
 tt_pool.first=b;
 assert(!kmem_shrink(b+1,256,2)&&b->size==512);
 assert(!kmem_shrink(b+1,513,1)&&b->size==512);
 assert(!kmem_shrink((unsigned char*)(b+1)+4,128,1));
 assert(kmem_shrink(b+1,256,1)&&b->size==256);
 assert(b->next->size==512&&b->next->next==0&&b->next->owner==0);
 assert(!kmem_shrink(b+1,0,1)&&!kmem_shrink(b+1,0xfffffff1u,1));
 puts("Kernel shrink: bounds, owner, exact pointer, split and adjacent-free coalescing: PASS");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)
 for name,text in [('persona',code),('limits',limits),('shrink',shrink)]:
  (p/(name+'.c')).write_text(text)
  # Fragment pochodzi z kodu m68k, gdzie wskaznik ma 32 bity, wiec cast
  # uint32_t -> wskaznik jest tam poprawny; na hoscie 64-bitowym clang tylko
  # ostrzega, a -Werror zamienialoby to w blad.  Ostrzezenie wyciszamy TU,
  # zeby nie zmieniac testowanego kodu.
  subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-int-to-pointer-cast','-fsanitize=address,undefined',str(p/(name+'.c')),'-o',str(p/name)],check=True)
  subprocess.run([str(p/name)],check=True)
