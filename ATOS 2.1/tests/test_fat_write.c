#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "kernel_fat.h"
#include <stdlib.h>
#include "kernel_memory.h"
/* kernel_fat.c allocates its lazy write buffer through kmem in every build. */
void *kmem_alloc(uint32_t n,uint32_t a,uint16_t o,uint16_t f,int tt)
{(void)a;(void)o;(void)f;(void)tt;return calloc(1,n);}
int kmem_free(void *p,uint16_t o){(void)o;free(p);return 1;}
#ifndef FAT_TEST_CACHE
/* kernel_fat.c follows the transport through kblock_device(); the plain host
   test keeps one implicit device and links no kernel_block.c.  The mock mirrors
   kernel_block.c: a select records the device FIRST, clears readiness and only
   sets it again when the probe succeeds.  select_denied simulates a medium that
   cannot be reached. */
static uint8_t selected_device;static int select_denied,block_ready=1;
uint8_t kblock_device(void){return selected_device;}
int kblock_is_ready(void){return block_ready;}
int kblock_select(uint8_t device){selected_device=device;if(select_denied){block_ready=0;return 0;}block_ready=1;return 1;}
#endif
#ifdef FAT_TEST_CACHE
#include "kernel_block.h"
#define kblock_read physical_read
#define kblock_write physical_write
#endif
#define SECTORS 16384
static uint8_t disk[SECTORS*512],metadata[256*512],buf[1500];
static unsigned writes,fail_at,ratio,fs,root,data;static int persistent;
int kblock_read(uint32_t l,uint16_t n,void *p){if(l+n>SECTORS)return 0;memcpy(p,disk+l*512,n*512);return 1;}
int kblock_write(uint32_t l,uint16_t n,const void *p){assert(l+n<=SECTORS);writes++;if(fail_at&&(writes==fail_at||(persistent&&writes>=fail_at))){
#ifdef FAT_TEST_CACHE
 memcpy(disk+l*512,p,127); /* Failed DMA can still modify part of a sector. */
#endif
 return 0;}memcpy(disk+l*512,p,n*512);return 1;}
#ifdef FAT_TEST_CACHE
#undef kblock_read
#undef kblock_write
int kernel_acsi_read_blocks(uint8_t d,uint32_t s,uint16_t n,void *p){(void)d;return physical_read(s,n,p);}
int kblock_test_disk_write(uint8_t d,uint32_t s,uint16_t n,const void *p){(void)d;return physical_write(s,n,p);}
#endif
static void w16(uint8_t *p,unsigned v){p[0]=v;p[1]=v>>8;}
static void fat(unsigned c,unsigned v){w16(disk+(1+ratio)*512+c*2,v);w16(disk+(1+ratio+fs)*512+c*2,v);}
static void fixture(unsigned r){unsigned i;uint8_t *b;ratio=r;fs=64;root=1+r+2*fs;data=root+r;memset(disk,0,sizeof(disk));b=disk+512;
 w16(b+11,512*r);b[13]=1;w16(b+14,1);b[16]=2;w16(b+17,16*r);w16(b+19,(SECTORS-1)/r);w16(b+22,fs/r);
 fat(0,0xfff8);fat(1,0xffff);fat(2,3);fat(3,r==1?4:0xffff);if(r==1)fat(4,0xffff);
 b=disk+root*512;memcpy(b,"SOURCE  TXT",11);b[11]=32;w16(b+26,2);w16(b+28,1500);
 for(i=0;i<1500;i++)disk[data*512+i]=(uint8_t)(i*7);
 writes=fail_at=0;persistent=0;
#ifdef FAT_TEST_CACHE
 assert(kblock_init(0)==BLOCK_READY);
#endif
assert(kfat_mount(1,"BGM")==FAT_MOUNTED);
#ifdef FAT_TEST_CACHE
 /* Fixture edits below bypass the block API, so start with no read copies. */
 kblock_cache_clear();
#endif
}
static void verify_source(void){uint32_t got;unsigned i;assert(kfat_load_path("/SOURCE.TXT",buf,sizeof(buf),&got)==FAT_FILE_FOUND&&got==1500);for(i=0;i<1500;i++)assert(buf[i]==(uint8_t)(i*7));}
static void verify_fats(void){assert(!memcmp(disk+(1+ratio)*512,disk+(1+ratio+fs)*512,fs*512));}
int main(void){unsigned r,n,total;uint32_t size;uint8_t attr;
 for(r=1;r<=2;r++){
 fixture(r);assert(kfat_mkdir_path("/DEST")==0);assert(kfat_copy_path("/SOURCE.TXT","/DEST/COPY.TXT")==0);
 assert(kfat_load_path("/DEST/COPY.TXT",buf,sizeof(buf),&size)==FAT_FILE_FOUND&&size==1500);for(n=0;n<1500;n++)assert(buf[n]==(uint8_t)(n*7));
 assert(kfat_copy_path("/SOURCE.TXT","/DEST/COPY.TXT")==-FAT_WRITE_EXISTS);
 assert(kfat_delete_path("/DEST")==-FAT_WRITE_NOT_EMPTY);assert(kfat_delete_path("/DEST/COPY.TXT")==0);assert(kfat_delete_path("/DEST")==0);
 assert(kfat_stat_path("/DEST",&size,&attr)==FAT_FILE_NOT_FOUND);verify_source();verify_fats();
 fixture(r);assert(kfat_copy_path("/SOURCE.TXT","/COPY.TXT")==0);total=writes;
 for(n=1;n<=total;n++){fixture(r);memcpy(metadata,disk,data*512);fail_at=n;assert(kfat_copy_path("/SOURCE.TXT","/COPY.TXT")==-FAT_WRITE_IO);assert(!memcmp(metadata,disk,data*512));verify_source();verify_fats();}
 fixture(r);assert(kfat_mkdir_path("/DEST")==0);total=writes;
 for(n=1;n<=total;n++){fixture(r);memcpy(metadata,disk,data*512);fail_at=n;assert(kfat_mkdir_path("/DEST")==-FAT_WRITE_IO);assert(!memcmp(metadata,disk,data*512));}
 fixture(r);assert(kfat_delete_path("/SOURCE.TXT")==0);total=writes;
 for(n=1;n<=total;n++){fixture(r);memcpy(metadata,disk,data*512);fail_at=n;assert(kfat_delete_path("/SOURCE.TXT")==-FAT_WRITE_IO);assert(!memcmp(metadata,disk,data*512));verify_source();}
 fixture(r);for(n=5;n<16384;n++)fat(n,0xffff);if(r==2)fat(4,0xffff);memcpy(metadata,disk,data*512);
 assert(kfat_mkdir_path("/FULL")==-FAT_WRITE_FULL);assert(!memcmp(metadata,disk,data*512));
 fixture(r);assert(kfat_mkdir_path("/A")==0);assert(kfat_mkdir_path("/A/B")==0);
 assert(kfat_copy_path("/SOURCE.TXT","/A/B/C.TXT")==0);assert(kfat_delete_path("/A/B/C.TXT")==0);assert(kfat_delete_path("/A/B")==0);assert(kfat_delete_path("/A")==0);verify_fats();
 fixture(r);w16(disk+root*512+26,0);w16(disk+root*512+28,0);assert(kfat_copy_path("/SOURCE.TXT","/EMPTY.TXT")==0);assert(kfat_delete_path("/EMPTY.TXT")==0);
 fixture(r);disk[root*512+11]|=1;assert(kfat_delete_path("/SOURCE.TXT")==-FAT_WRITE_READ_ONLY);assert(!writes);
 fixture(r);fat(3,2);assert(kfat_delete_path("/SOURCE.TXT")==-FAT_WRITE_INVALID);assert(!writes);
 fixture(r);assert(kfat_delete_path("/")==-FAT_WRITE_INVALID);assert(kfat_mkdir_path("/../BAD")==-FAT_WRITE_INVALID);assert(!writes);
 fixture(r);fail_at=3;persistent=1;assert(kfat_mkdir_path("/FAIL")==-FAT_WRITE_IO);assert(kfat_mkdir_path("/NEXT")==-FAT_WRITE_READ_ONLY);
 }
#ifndef FAT_TEST_CACHE
 /* Session/transport contract: a console whose volume lives on another
    transport must not be reported as usable when that transport cannot be
    selected, and must recover once it can.  The plain variant owns the
    kblock_select/kblock_is_ready stubs; the cached variant links the real
    kernel_block.c. */
 assert(kfat_select_session(0)==1);              /* deterministic context */
 fixture(1);                                     /* session 0 = "BGM" on device 0 */
 assert(kblock_device()==0&&kblock_is_ready());
 selected_device=1;                              /* session 1's volume is on device 1 */
 assert(kfat_select_session(1)==0);              /* empty session: nothing to switch */
 assert(kfat_mount(1,"S2A")==FAT_MOUNTED);       /* session 1 = "S2A" on device 1 */
 assert(kfat_select_session(0)==1);              /* follows the transport back */
 assert(kblock_device()==0);
 select_denied=1;
 assert(kfat_select_session(1)==0);              /* unreachable: NOT usable ... */
 assert(!kblock_is_ready());                     /* ... and the block layer agrees */
 assert(kfat_select_session(1)==0);              /* stays honest while it stays down */
 select_denied=0;
 assert(kfat_select_session(1)==1);              /* recovers when it can be reached */
 assert(kblock_is_ready()&&kblock_device()==1);
#endif
 puts("FAT writes: copy/mkdir/delete, both FATs, 512/1024-byte BPB, full disk, cycles, every write failure rollback, session transport contract PASS");return 0;}
