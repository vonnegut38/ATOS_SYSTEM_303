#ifndef ASF_EXCEPTION_FORMAT_H
#define ASF_EXCEPTION_FORMAT_H
#include <stdint.h>
#include <stddef.h>
/* Motorola MC68030 UM, section 8: SR at +0, PC at +2,
 * format/vector offset at +6. Format 0 also has the format word.
 * The caller supplies CPU knowledge; SR bits cannot identify the frame. */
typedef struct {
    uint16_t format,vector;
    uint32_t sr,pc,fault;
    int fault_valid;
} exc_frame_t;
static inline void exc_parse_frame_sized(const uint16_t *w,size_t words,
                                         int has_format_word,exc_frame_t *out)
{
    out->format=out->vector=0xffffu;
    out->sr=out->pc=out->fault=0;out->fault_valid=0;
    if(!w||words<3u)return;
    out->sr=w[0];out->pc=((uint32_t)w[1]<<16)|w[2];
    if(!has_format_word){out->format=0;return;}
    if(words<4u)return;
    out->format=w[3]>>12;
    if(!(w[3]&3u))out->vector=(w[3]&0xfffu)>>2;
    /* A/B data fault address is meaningful only with SSW.DF set.
       Instruction-prefetch faults require separate pipeline analysis. */
    if((out->format==0xau||out->format==0xbu)&&words>=10u&&(w[5]&0x100u)){
        out->fault=((uint32_t)w[8]<<16)|w[9];out->fault_valid=1;
    }
}
/* Live CPU frame: only recognized A/B formats permit the extended read.
   For copied/truncated frames use the sized API above. */
static inline void exc_parse_frame(const uint16_t *w,int has_format_word,exc_frame_t *out)
{
    size_t words=has_format_word?4u:3u;
    if(w&&has_format_word&&((w[3]>>12)==0xau||(w[3]>>12)==0xbu))words=10u;
    exc_parse_frame_sized(w,words,has_format_word,out);
}
static inline const char *exc_format_name(uint16_t format)
{
    switch(format){
    case 0:return "SHORT";
    case 1:return "THROWAWAY";
    case 2:return "SIX-WORD";
    case 9:return "COPROCESSOR MID-INSTRUCTION";
    case 0xa:return "SHORT BUS FAULT";
    case 0xb:return "LONG BUS FAULT";
    default:return "UNKNOWN";
    }
}
#endif
