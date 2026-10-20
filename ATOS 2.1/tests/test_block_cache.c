#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kernel_block.h"
#include "kernel_memory.h"
#define N 2048u
static unsigned char disk[2][N*512],out[4096],input[4096];
static unsigned reads,writes,allocs;
static int read_fail,write_fail,no_cache;
void *kmem_alloc(uint32_t n,uint32_t a,uint16_t o,uint16_t f,int tt)
{(void)a;(void)o;(void)f;allocs++;if(tt&&no_cache)return NULL;return calloc(1,n);}
int kmem_free(void *p,uint16_t o){(void)o;free(p);return 1;}
int kernel_acsi_read_blocks(uint8_t d,uint32_t s,uint16_t n,void *p)
{reads++;if(read_fail||d>1||s>=N||n>N-s){memset(p,0xcc,n*512);return 0;}memcpy(p,disk[d]+s*512,n*512);return 1;}
int kblock_test_disk_write(uint8_t d,uint32_t s,uint16_t n,const void *p)
{writes++;if(d>1||s>=N||n>N-s)return 0;if(write_fail){/* Partial DMA completion despite error. */memcpy(disk[d]+s*512,p,127);return 0;}memcpy(disk[d]+s*512,p,n*512);return 1;}
static void check_read(unsigned d,unsigned s,unsigned n)
{memset(out,0xa5,sizeof(out));assert(kblock_read(s,n,out));assert(!memcmp(out,disk[d]+s*512,n*512));if(n<8)assert(out[n*512]==0xa5);}
int main(int argc,char **argv)
{unsigned i,j,r;uint32_t seed=0x12345678;kblock_stats_t st;(void)argv;no_cache=argc>1;
 for(i=0;i<2;i++)for(j=0;j<N*512;j++)disk[i][j]=(unsigned char)(j*13+j/512+i*71);
 /* Selecting a device before init must not claim success. */
 assert(!kblock_select(0));
 assert(kblock_init(0)==BLOCK_READY);
 /* A failed switch must disable I/O, and retry must probe again. */
 read_fail=1;assert(!kblock_select(1));
 r=reads;assert(!kblock_select(1));assert(reads==r+1);
 assert(!kblock_read(0,1,out));assert(!kblock_write(0,1,input));
 read_fail=0;assert(kblock_select(1));check_read(1,0,1);
 assert(kblock_select(0));check_read(0,0,1);
 r=reads;check_read(0,12,1);check_read(0,44,1);check_read(0,12,1);
 assert(reads==r+(no_cache?3:2));
 /* Eviction: read more distinct sectors than the cache can hold, then the
    oldest must miss while the newest still hits.  Driven by the reported cache
    size instead of hardcoded sector numbers, so the geometry can change. */
 if(!no_cache){unsigned k,total=kblock_cache_sectors();
  assert(total>0u && total<N);
  kblock_cache_clear();
  for(k=0;k<total+1u;++k)check_read(0,k,1);
  r=reads;check_read(0,total,1);assert(reads==r);        /* newest still cached */
  check_read(0,0,1);assert(reads==r+1);}                 /* oldest evicted */
 memset(input,0x62,sizeof(input));check_read(0,20,8);assert(kblock_write(22,3,input));assert(!memcmp(disk[0]+22*512,input,3*512));check_read(0,20,8);
 write_fail=1;memset(input,0x93,sizeof(input));assert(!kblock_write(22,3,input));write_fail=0;check_read(0,22,3);assert(out[0]==0x93&&out[127]==0x62);
 kblock_cache_clear();read_fail=1;memset(out,0x5a,sizeof(out));assert(!kblock_read(10,8,out));assert(out[0]==0x5a);read_fail=0;check_read(0,10,8);
 assert(kblock_init(1)==BLOCK_READY);check_read(1,10,8);assert(allocs==(no_cache?3:2));
 read_fail=1;assert(kblock_init(0)==BLOCK_IO_FAILED);assert(!kblock_read(10,1,out));assert(!kblock_write(10,1,input));read_fail=0;assert(kblock_init(0)==BLOCK_READY);
 r=reads;assert(!kblock_read(0,0,out));assert(!kblock_read(0,9,out));assert(!kblock_read(0xffffffffu,2,out));assert(!kblock_write(0xffffffffu,2,input));assert(reads==r);
 /* Diagnostic reads bypass cache without evicting it or writing disk. */
 check_read(0,10,1);r=reads;{unsigned oldwrites=writes;kblock_stats_t before,after;
 kblock_cache_stats(&before);assert(kblock_read_uncached(10,1,out));assert(kblock_read_uncached(10,1,out));
 assert(reads==r+2&&writes==oldwrites);kblock_cache_stats(&after);assert(after.hits==before.hits&&after.misses==before.misses);
 assert(!memcmp(out,disk[0]+10*512,512));read_fail=1;out[0]=0x5a;assert(!kblock_read_uncached(10,1,out)&&out[0]==0x5a);read_fail=0;
 assert(!kblock_read_uncached(0,9,out)&&!kblock_read_uncached(0xffffffffu,2,out));}
 /* Mixed runs, conflicts and partial failed writes against physical bytes. */
 for(i=0;i<10000;i++){unsigned s,n;seed=seed*1664525u+1013904223u;n=1+(seed%8);s=(seed>>8)%(N-8);
 if(seed&1){memset(input,(int)(seed>>24),n*512);write_fail=(i%17==0);assert(kblock_write(s,n,input)==!write_fail);write_fail=0;}check_read(0,s,n);}
 /* A run served entirely from cache must probe each sector exactly ONCE: the
    all-hit path looked the slot up to test it and then looked it up AGAIN to
    fetch it, doubling the set/way searches on the hottest path in the system. */
 if(!no_cache){
  kblock_stats_t b,a;unsigned n;
  check_read(0,10,3);                       /* warm the 3-sector run */
  kblock_cache_stats(&b);
  check_read(0,10,3);                       /* served from cache */
  kblock_cache_stats(&a);
  n=a.searches-b.searches;
  assert(n==3u);                            /* one per sector, not two */
  printf("All-hit read: %u searches for 3 sectors (expected 3) PASS\n",n);
 }
 kblock_cache_stats(&st);assert(st.reads&&st.writes);printf("Block cache %s: LRU, ranges, write-through, partial failures, device reset, 10000 mixed transfers PASS\n",no_cache?"low-memory bypass":"cached");return 0;}
