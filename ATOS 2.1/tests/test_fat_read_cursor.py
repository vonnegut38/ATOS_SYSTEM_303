"""A sequential file read must not walk the FAT chain once per chunk.

kfat_read_path_at() resolved the path and then stepped the cluster chain from the
file's FIRST cluster up to the requested offset, on every call.  Reading one file
in 512-byte chunks re-walked the whole prefix for each chunk - quadratic.  The
cursor added in kfat_read_cursor() continues where the previous read stopped.

This runs the REAL read_at_cursor() (and the real read_fat_entry()) from
system/kernel_fat.c over a synthetic FAT16 volume, counts the chain steps, and
asserts the chunked read is linear.  It also runs the same loop with no cursor and
asserts THAT is quadratic - so the test fails if the cursor is ever removed.
"""
from pathlib import Path
import subprocess
import tempfile
import unittest

class FatReadCursorTest(unittest.TestCase):
    def test_chunked_read_is_linear_with_a_cursor(self):
        src = Path('system/kernel_fat.c').read_text()

        def take(name):
            i = src.index(name)
            j = src.index('{', i)
            depth = 0
            while True:
                if src[j] == '{':
                    depth += 1
                elif src[j] == '}':
                    depth -= 1
                    if depth == 0:
                        return src[i:j+1] + '\n'
                j += 1

        reader = take('static enum fat_file_state read_at_cursor(')

        stub = r'''
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

enum fat_file_state {FAT_FILE_NOT_FOUND,FAT_FILE_IO_FAILED,FAT_FILE_CORRUPT,FAT_FILE_FOUND};

/* The cursor contract, exactly as include/kernel_fat.h declares it. */
typedef struct {
    uint32_t offset,size;
    uint16_t cluster,start_cluster;
    uint8_t valid,reserved;
} fat_cursor_t;

/* Synthetic volume geometry.  In the kernel these are macros over the mount
   struct; here they are plain constants so the real reader can be compiled
   without dragging the mount layer in. */
#define CLUSTERS 64u
#define FILE_BYTES (CLUSTERS*512u)
#define fat_start 0u
#define data_start 8u
#define cluster_count (CLUSTERS+2u)
#define cluster_physical_sectors 1u
#define fat_bits 16u

static uint8_t sector_buffer[512];
uint32_t kfat_counts[4];
static uint8_t fake_disk[(8u+CLUSTERS)*512u];

static void copy_bytes(uint8_t *destination,const uint8_t *source,uint16_t count)
{while(count--)*destination++=*source++;}

static int kblock_read(uint32_t lba,uint16_t count,uint8_t *destination)
{(void)count;memcpy(destination,fake_disk+(size_t)lba*512u,512u);return 1;}

static int fat_is_eoc(uint16_t v){return v>=(fat_bits==12u?0x0ff8u:0xfff8u);}
static int fat_is_bad(uint16_t v){return v==(fat_bits==12u?0x0ff7u:0xfff7u);}

static int read_fat_entry(uint16_t cluster,uint16_t *next)
{
    uint32_t offset=(uint32_t)cluster*2u;
    ++kfat_counts[0];
    if(!kblock_read(fat_start+offset/512u,1u,sector_buffer))return 0;
    *next=(uint16_t)(sector_buffer[offset&511u]|((uint16_t)sector_buffer[(offset&511u)+1u]<<8));
    return 1;
}

/* Cluster c holds byte value c, so every read can be checked. */
static enum fat_file_state resolve_path(const char *path,uint16_t *cluster,
                                        uint32_t *size,uint8_t *attributes)
{
    (void)path;
    if(cluster)*cluster=2u;
    if(size)*size=FILE_BYTES;
    if(attributes)*attributes=0u;
    return FAT_FILE_FOUND;
}
'''
        main = r'''
int main(void)
{
    unsigned i;uint32_t steps;
    fat_cursor_t cursor;
    memset(fake_disk,0,sizeof fake_disk);
    /* FAT16 chain 2 -> 3 -> ... -> last, and cluster data = cluster number. */
    for(i=0;i<CLUSTERS;++i){
        uint16_t c=(uint16_t)(2u+i),next=(uint16_t)((i+1u<CLUSTERS)?(3u+i):0xffffu);
        fake_disk[c*2u]=(uint8_t)next;
        fake_disk[c*2u+1]=(uint8_t)(next>>8);
        memset(fake_disk+((size_t)8u+i)*512u,(int)c,512u);
    }

    memset(&cursor,0,sizeof cursor);
    {uint8_t out[512];uint32_t got=0,total=0;
     while(total<FILE_BYTES){
        if(read_at_cursor("F",total,out,sizeof out,&got,&cursor)!=FAT_FILE_FOUND)return 2;
        if(!got)break;
        assert(out[0]==(uint8_t)(2u+total/512u));
        assert(out[511]==(uint8_t)(2u+total/512u));
        total+=got;
     }
     assert(total==FILE_BYTES);
     steps=kfat_counts[2];}
    printf("z kursorem:  %u krokow lancucha na %u porcji 512 B\n",
           steps,(unsigned)(FILE_BYTES/512u));
    assert(steps<=CLUSTERS+1u);
    assert(kfat_counts[1]>=CLUSTERS-1u);

    kfat_counts[0]=kfat_counts[1]=kfat_counts[2]=0;
    {uint8_t out[512];uint32_t got=0,total=0;
     while(total<FILE_BYTES){
        if(read_at_cursor("F",total,out,sizeof out,&got,0)!=FAT_FILE_FOUND)return 3;
        if(!got)break;
        total+=got;
     }}
    printf("bez kursora: %u krokow lancucha na %u porcji 512 B\n",
           kfat_counts[2],(unsigned)(FILE_BYTES/512u));
    assert(kfat_counts[2]>steps*4u);
    printf("stosunek: %u vs %u\n",kfat_counts[2],steps);
    return 0;
}
'''
        code = stub + reader + main
        with tempfile.TemporaryDirectory() as d:
            c = Path(d)/'fat_cursor.c'
            c.write_text(code)
            exe = Path(d)/'fat_cursor'
            r = subprocess.run(['cc','-Wall','-Wextra','-Werror','-O1',
                                '-fsanitize=address,undefined',str(c),'-o',str(exe)],
                               capture_output=True,text=True)
            if r.returncode:
                self.fail("kompilacja nieudana:\n"+r.stderr[:2500])
            r = subprocess.run([str(exe)],capture_output=True,text=True)
            if r.returncode:
                self.fail("test nie przeszedl:\n"+r.stdout+r.stderr[:1500])
            print(r.stdout.strip())

if __name__ == '__main__':
    unittest.main(verbosity=2)
