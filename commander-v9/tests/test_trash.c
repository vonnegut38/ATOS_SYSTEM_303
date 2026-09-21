#define main legacy_test_main
#include "test_fat_write.c"
#undef main
static uint8_t snapshot[sizeof(disk)];
static int exists(const char *p){uint32_t size;uint8_t attr;return kfat_stat_path(p,&size,&attr)==FAT_FILE_FOUND;}
static void content(const char *p){uint32_t size;unsigned i;assert(kfat_load_path(p,buf,sizeof(buf),&size)==FAT_FILE_FOUND&&size==1500);for(i=0;i<1500;i++)assert(buf[i]==(uint8_t)(i*7));}
static unsigned char owned[16384];
static unsigned rd16(const unsigned char *p){return p[0]|((unsigned)p[1]<<8);}
static void audit_entries(const unsigned char *p,unsigned bytes,unsigned parent);
static void audit_chain(unsigned c,int dir,unsigned parent){unsigned first=c,steps=0;
 while(c>=2&&c<0xfff8){const unsigned char *p;assert(c<16384&&!owned[c]&&++steps<16384);owned[c]=1;
  p=disk+(data+(c-2)*ratio)*512;
  if(dir){if(c==first){assert(p[0]=='.'&&p[32]=='.'&&p[33]=='.');assert(rd16(p+26)==c&&rd16(p+58)==parent);}
   audit_entries(p,ratio*512,first);}
  c=rd16(disk+(1+ratio)*512+c*2);
 }
 assert(c==0||c>=0xfff8);
}
static void audit_entries(const unsigned char *p,unsigned bytes,unsigned parent){unsigned i;
 for(i=0;i<bytes;i+=32){unsigned c;if(!p[i])break;if(p[i]==0xe5||p[i]=='.'||(p[i+11]&15)==15||(p[i+11]&8))continue;
  c=rd16(p+i+26);if(c)audit_chain(c,(p[i+11]&16)!=0,parent);
 }}
static void audit(void){unsigned c;memset(owned,0,sizeof(owned));audit_entries(disk+root*512,ratio*512,0);
 for(c=2;c<16384;c++)assert((rd16(disk+(1+ratio)*512+c*2)!=0)==(owned[c]!=0));
 verify_fats();}
static void tree(void){fixture(1);assert(!kfat_mkdir_path("/DIR"));assert(!kfat_mkdir_path("/DIR/SUB"));assert(!kfat_copy_path("/SOURCE.TXT","/DIR/SUB/COPY.TXT"));assert(!kfat_mkdir_path("/DEST"));writes=0;}
int main(void){unsigned n,total;uint32_t size;uint8_t attr;
 tree();assert(kfat_move_path("/DIR","/DIR/SUB/CYCLE")==-FAT_WRITE_INVALID);assert(!writes);
 assert(!kfat_move_path("/DIR","/DEST/RENAMED"));total=writes;content("/DEST/RENAMED/SUB/COPY.TXT");assert(!exists("/DIR"));audit();
 for(n=1;n<=total;n++){tree();memcpy(snapshot,disk,sizeof(disk));fail_at=n;assert(kfat_move_path("/DIR","/DEST/RENAMED")==-FAT_WRITE_IO);assert(!memcmp(snapshot,disk,sizeof(disk)));content("/DIR/SUB/COPY.TXT");assert(!exists("/DEST/RENAMED"));audit();}
 tree();assert(kfat_move_path("/SOURCE.TXT","/SOURCE.TXT")==-FAT_WRITE_EXISTS);assert(!writes);
 assert(kfat_purge_path("/DIR")==-FAT_WRITE_READ_ONLY);assert(exists("/DIR"));
 assert(!kfat_trash_path("/DIR"));total=writes;assert(!exists("/DIR"));content("/TRASH/T0000001/DIR/SUB/COPY.TXT");verify_source();audit();
 assert(kfat_trash_path("/TRASH")==-FAT_WRITE_READ_ONLY);assert(kfat_trash_path("/TRASH/T0000001")==-FAT_WRITE_READ_ONLY);assert(kfat_purge_path("/TRASH")==-FAT_WRITE_READ_ONLY);
 assert(!kfat_move_path("/TRASH/T0000001/DIR","/DIR"));content("/DIR/SUB/COPY.TXT");
 assert(!kfat_trash_path("/DIR"));assert(exists("/TRASH/T0000001"));content("/TRASH/T0000002/DIR/SUB/COPY.TXT");
 assert(!kfat_purge_path("/TRASH/T0000001"));assert(!kfat_purge_path("/TRASH/T0000002"));assert(!exists("/TRASH/T0000002"));verify_source();audit();
 for(n=1;n<=total;n++){tree();fail_at=n;assert(kfat_trash_path("/DIR")==-FAT_WRITE_IO);assert(exists("/DIR"));content("/DIR/SUB/COPY.TXT");assert(!exists("/TRASH/T0000001/DIR"));audit();}
 tree();assert(!kfat_trash_path("/DIR"));writes=0;assert(!kfat_purge_path("/TRASH/T0000001"));total=writes;
 for(n=1;n<=total;n++){tree();assert(!kfat_trash_path("/DIR"));writes=0;fail_at=n;assert(kfat_purge_path("/TRASH/T0000001")==-FAT_WRITE_IO);verify_source();audit();fail_at=0;assert(!kfat_purge_path("/TRASH/T0000001"));audit();}
 /* Read-only descendant must reject purge during preflight, before any deletion. */
 tree();assert(!kfat_trash_path("/DIR"));
 /* DIR=5, SUB=6 in this fixture; first real SUB entry at offset 64. */
 disk[(data+4)*512+64+11]|=1;
#ifdef FAT_TEST_CACHE
 kblock_cache_clear();
#endif
 writes=0;assert(kfat_purge_path("/TRASH/T0000001")==-FAT_WRITE_READ_ONLY);assert(!writes);content("/TRASH/T0000001/DIR/SUB/COPY.TXT");
 assert(kfat_stat_path("/SOURCE.TXT",&size,&attr)==FAT_FILE_FOUND);
 puts("Trash PASS: subtree metadata move, restore, collisions, cycle/root guards, recursive purge, read-only preflight, every failed write/rollback with partial DMA");return 0;}
