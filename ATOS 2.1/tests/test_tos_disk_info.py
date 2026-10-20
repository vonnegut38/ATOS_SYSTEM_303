#!/usr/bin/env python3
"""Validate actual mounted-volume conversion, including BGM large sectors."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'system/kernel_fat.c').read_text()
a=s.index('int kfat_tos_bpb(');b=s.index('static enum fat_file_state scan_directory_sector',a)
code=r'''
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
static uint32_t logical_sector_size,cluster_physical_sectors,root_physical_sectors;
static uint32_t fat_sectors,fat_start,volume_start,fat_copies,data_start,cluster_count,fat_bits;
static int io_ok=1;
static int read_fat_entry(uint16_t c,uint16_t *e){*e=c%2;return io_ok;}
'''+s[a:b]+r'''
int main(void){
 uint16_t w[9]={0};uint32_t n=123;
 assert(!kfat_tos_bpb(w)&&!kfat_tos_bpb(0)&&!kfat_free_clusters(&n)&&n==123);
 logical_sector_size=1024;cluster_physical_sectors=4;root_physical_sectors=32;
 volume_start=1;fat_start=3;fat_sectors=64;fat_copies=2;
 data_start=163;cluster_count=16000;fat_bits=16;
 assert(kfat_tos_bpb(w));
 assert(w[0]==1024&&w[1]==2&&w[2]==2048&&w[3]==16&&w[4]==32);
 assert(w[5]==33&&w[6]==81&&w[7]==16000&&w[8]==1);
 assert(kfat_free_clusters(&n)&&n==8000);
 io_ok=0;n=123;assert(!kfat_free_clusters(&n)&&n==123);
 logical_sector_size=512;fat_copies=1;fat_bits=12;cluster_count=2000;
 assert(kfat_tos_bpb(w)&&w[5]==2&&w[8]==2);
 cluster_physical_sectors=128;assert(!kfat_tos_bpb(w));
 puts("TOS BPB / free clusters: 512/1024-byte sectors, FAT12/16, one/two FATs, overflow and I/O errors: PASS");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'t.c').write_text(code)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'t.c'),'-o',str(p/'t')],check=True)
 subprocess.run([str(p/'t')],check=True)
