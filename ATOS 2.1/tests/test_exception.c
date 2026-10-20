#include <stdio.h>
#include <assert.h>
#include "exception_format.h"
int main(void)
{
    exc_frame_t f;
    const uint16_t old[3]={0x2700,0x00e0,0x1234};
    const uint16_t illegal[4]={0x0500,0x00e0,0x3000,0x0010};
    uint16_t bus[16]={0x2700,0x00e0,0x2000,0xa008,0,0x0105,0,0,0x00ff,0x1234};
    exc_parse_frame(old,0,&f);
    assert(f.sr==0x2700&&f.pc==0x00e01234&&f.vector==0xffff&&!f.fault_valid);
    exc_parse_frame(illegal,1,&f);
    assert(f.format==0&&f.vector==4&&f.sr==0x0500&&f.pc==0x00e03000&&!f.fault_valid);
    exc_parse_frame(bus,1,&f);
    assert(f.format==0xa&&f.vector==2&&f.sr==0x2700&&f.pc==0x00e02000);
    assert(f.fault_valid&&f.fault==0x00ff1234);
    bus[3]=0xb00c;exc_parse_frame(bus,1,&f);
    assert(f.format==0xb&&f.vector==3&&f.fault_valid);
    bus[5]=0x4000;exc_parse_frame(bus,1,&f);assert(!f.fault_valid);
    bus[5]=0x0100;
    for(size_t n=0;n<10;n++){
        exc_parse_frame_sized(bus,n,1,&f);assert(!f.fault_valid);
    }
    bus[3]=0x7008;exc_parse_frame_sized(bus,16,1,&f);assert(!f.fault_valid);
    bus[3]=0x0011;exc_parse_frame_sized(bus,4,1,&f);assert(f.vector==0xffff);
    exc_parse_frame(0,1,&f);assert(f.format==0xffff&&f.pc==0);
    puts("EXCEPTION: ALL PASS");return 0;
}
