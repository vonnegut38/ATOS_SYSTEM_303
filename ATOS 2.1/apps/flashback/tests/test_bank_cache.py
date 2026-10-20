#!/usr/bin/env python3
"""Compile the real bank-cache methods with a deterministic decompressor.
Checks publication, cache hits, slot exhaustion and oversized bank rejection.
"""
from pathlib import Path
import subprocess,sys
root=Path(__file__).resolve().parents[1]
s=(root/'engine/resource.cpp').read_text()
methods=[]
for name in ('clearBankData','getBankDataSize','findBankData','loadBankData'):
 i=s.index('Resource::'+name);a=s.rfind('\n',0,i)+1;b=s.index('{',i);depth=1;j=b+1
 while depth:
  if s[j]=='{':depth+=1
  if s[j]=='}':depth-=1
  j+=1
 methods.append(s[a:j])
head=r'''
#include <stdint.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#define NDEBUG
#include <cassert>
#define ARRAYSIZE(a) (sizeof(a)/sizeof(a[0]))
#define READ_BE_UINT16(p) ((unsigned((p)[0])<<8)|(p)[1])
#define READ_BE_UINT32(p) ((READ_BE_UINT16(p)<<16)|READ_BE_UINT16((p)+2))
static unsigned decodes;
static bool bytekiller_unpack(uint8_t *dst,int,const uint8_t*,int) { ++decodes; memset(dst,0x55,32); return true; }
static void warning(const char*,...) {}
static void error(const char*,...) { throw std::runtime_error("bank rejected"); }
enum { kResourceTypeAmiga,kResourceTypeDOS,kResourceTypePC98,kResourceTypeMac };
struct Resource {
 int _type;uint8_t *_mbk,*_bnq,*_bankData,*_bankDataHead,*_bankDataTail;
 struct { unsigned entryNum;uint8_t *ptr; } _bankBuffers[2];int _bankBuffersCount;
 void clearBankData();int getBankDataSize(uint16_t);uint8_t *findBankData(uint16_t);uint8_t *loadBankData(uint16_t);
};
static void require(bool v) { if(!v)abort(); }
'''
main=r'''
int main() {
 Resource r{};uint8_t file[160]={},storage[96]={};
 r._type=kResourceTypeDOS;r._mbk=file;r._bankData=storage;r._bankDataTail=storage+sizeof(storage);r.clearBankData();
 file[3]=32;file[5]=1;file[31]=32; // compressed bank, decoded size 32
 file[9]=68;file[10]=0x80;file[11]=1;
 file[15]=100;file[16]=0x80;file[17]=1;
 memset(file+68,0x66,32);memset(file+100,0x77,32);
 uint8_t *first=r.loadBankData(0);require(decodes==1&&r._bankBuffersCount==1);
 for(int i=0;i<100;++i)require(r.findBankData(0)==first&&first[0]==0x55);
 require(decodes==1);
 require(r.loadBankData(1)[0]==0x66&&r.findBankData(0)==first);
 require(r.loadBankData(2)[0]==0x77&&r.findBankData(0)==nullptr&&r._bankBuffersCount==1);
 require(r.loadBankData(0)[0]==0x55&&decodes==2);
 r.clearBankData();file[5]=4;bool failed=false;
 try {r.loadBankData(0);}catch(const std::runtime_error&){failed=true;}
 require(failed);
 puts("bank cache: repeated hits, slot exhaustion, refills and size guard PASS");
}
'''
out=root/'build/host';out.mkdir(parents=True,exist_ok=True)
cpp=out/'test_bank_cache.cpp';cpp.write_text(head+'\n'.join(methods)+main)
subprocess.run(['c++','-std=c++17','-fsanitize=address,undefined',str(cpp),'-o',str(out/'test_bank_cache')],check=True)
subprocess.run([str(out/'test_bank_cache')],check=True)
