#ifndef ASF_EXCEPTION_FORMAT_H
#define ASF_EXCEPTION_FORMAT_H
#include <stdint.h>
/* Phase 5 (v10): pure exception-frame decoder.  A single entry point is
 * installed for all fatal vectors; on 68010+ the frame's first word carries
 * format (high nibble) and vector (low 12 bits).  The 68000 has no format
 * word (short frame: SR, PC), so vector is unknown there.  Host-tested in
 * tests/test_exception.c. */
typedef struct {
    uint16_t format;    /* frame format code (0 = 68000 short) */
    uint16_t vector;    /* 0xFFFF when the CPU gave no format word */
    uint32_t sr;
    uint32_t pc;
    uint32_t fault;     /* bus/address fault address when available */
    int      fault_valid;
} exc_frame_t;

static inline int exc_frame_has_format(uint16_t first_word)
{
    return (first_word&0xF000u)!=0u;
}

static inline void exc_parse_frame(const uint16_t *w,int has_format_word,exc_frame_t *out)
{
    out->fault=0u;out->fault_valid=0;
    if(!w){out->format=0xFFFFu;out->vector=0xFFFFu;out->sr=0u;out->pc=0u;return;}
    if(has_format_word&&exc_frame_has_format(w[0])){
        out->format=(uint16_t)((w[0]>>12)&0x0Fu);
        out->vector=(uint16_t)(w[0]&0x0FFFu);
        out->sr=(uint32_t)w[1];
        out->pc=((uint32_t)w[2]<<16)|w[3];
        /* 68000/68010/68020+ bus & address long frames carry the fault address
           at offset 0x0A (word 5). */
        if(out->format==1u||out->format==2u||out->format==7u){
            out->fault=((uint32_t)w[5]<<16)|w[6];
            out->fault_valid=1;
        }
    }else{
        out->format=0u;
        out->vector=0xFFFFu;
        out->sr=(uint32_t)w[0];
        out->pc=((uint32_t)w[1]<<16)|w[2];
    }
}
static inline const char *exc_format_name(uint16_t format)
{
    switch(format){
    case 0u:  return "SHORT (68000)";
    case 1u:  return "68000 BUS/ADDRESS";
    case 2u:  return "68010 BUS/ADDRESS";
    case 3u:  return "68010+ INSTRUCTION";
    case 7u:  return "68020+ BUS/ADDRESS";
    case 0xAu:return "68020+ SHORT";
    case 0xBu:return "68020+ INSTRUCTION";
    default:  return "UNKNOWN";
    }
}
#endif
