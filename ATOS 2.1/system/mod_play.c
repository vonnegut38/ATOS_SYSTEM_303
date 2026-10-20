/* MOD player state machine.  See include/mod_play.h for the design note. */
#include "mod_play.h"

int mod_play_init(mod_play_t *p,const uint8_t *data,uint32_t size)
{
    unsigned i;
    if(!p||!data)return -1;
    if(size<1084u)return -2;                       /* header + signature */
    /* M.K. is a 4-channel 31-instrument module; refuse anything else instead of
       guessing (8-channel signatures have a different cell layout). */
    if(!(data[1080]=='M'&&data[1081]=='.'&&data[1082]=='K'&&data[1083]=='.'))return -3;
    p->data=data;p->size=size;
    p->song_len=data[950];
    if(!p->song_len||p->song_len>128u)return -4;
    p->restart=data[951];
    for(i=0;i<31u;++i){
        p->sample_volume[i]=data[20u+30u*i+25u];
        if(p->sample_volume[i]>64u)p->sample_volume[i]=64u;
    }
    {   /* pattern count from the order table, as the player needs to bound it */
        uint8_t maxp=0u;
        for(i=0;i<p->song_len;++i){uint8_t v=data[952u+i];if(v>maxp)maxp=v;}
        if(maxp>=MOD_MAX_PATTERNS)return -4;
        p->pattern_count=(uint8_t)(maxp+1u);
    }
    p->sample_data=1084u+(uint32_t)p->pattern_count*1024u;   /* signature occupies 1080..1083 */
    if(p->sample_data>size)return -4;
    p->order=0u;p->row=0u;p->tick=0u;
    p->speed=6u;p->tempo=125u;p->frac=0u;
    p->finished=0u;p->jump=0u;p->jump_order=0u;p->jump_row=0u;
    for(i=0;i<MOD_VOICES;++i){
        p->voice[i].period=0u;p->voice[i].volume=0u;p->voice[i].psg=0xffu;
    }
    return 0;
}

/* Decode one cell of the current pattern. */
static void decode_cell(const mod_play_t *p,unsigned channel,mod_cell_t *out)
{
    uint8_t pat=p->data[952u+p->order];
    const uint8_t *c=p->data+1084u+(uint32_t)pat*1024u+(uint32_t)p->row*16u+channel*4u;
    out->period=(uint16_t)(((uint16_t)(c[0]&0x0fu)<<8)|c[1]);
    out->instrument=(uint8_t)((c[0]&0xf0u)|(c[2]>>4));
    out->effect=(uint8_t)(c[2]&0x0fu);
    out->param=c[3];
}

uint32_t mod_play_tick(mod_play_t *p,mod_psg_note_t *out,unsigned max,unsigned *count)
{
    uint32_t delay;
    if(count)*count=0u;
    if(p->finished)return MOD_END;
    if(p->tick==0u){
        /* New row: decode all four voices, apply the instrument volumes, then let
           mod_psg_row() handle the note/volume effects. */
        mod_cell_t cell[MOD_VOICES];
        unsigned i;
        for(i=0;i<MOD_VOICES;++i){
            decode_cell(p,i,&cell[i]);
            if(cell[i].instrument&&cell[i].instrument<=31u)
                p->voice[i].volume=p->sample_volume[cell[i].instrument-1u];
            /* Effects that change the song position are collected here; the row
               still plays with its own notes. */
            if(cell[i].effect==0x0Fu){
                if(cell[i].param==0u){p->finished=1u;return MOD_END;}
                else if(cell[i].param<32u)p->speed=cell[i].param;
                else p->tempo=cell[i].param;
            }else if(cell[i].effect==0x0Bu){
                p->jump=1u;p->jump_order=cell[i].param;p->jump_row=0u;
            }else if(cell[i].effect==0x0Du){
                p->jump=1u;p->jump_order=(uint8_t)(p->order+1u);
                p->jump_row=(uint8_t)(((cell[i].param>>4)*10u)+(cell[i].param&0x0fu));
                if(p->jump_row>=64u)p->jump_row=0u;
            }
        }
        {unsigned n=mod_psg_row(cell,p->voice,out,max);if(count)*count=n;}
    }
    /* One player tick lasts 2.5/tempo seconds; accumulate the fraction so the
       tempo does not drift. */
    p->frac+=(uint32_t)MOD_HIRES_PER_SEC*2500u;
    {
        uint32_t divisor=(uint32_t)p->tempo*1000u;
        /* Keep quotient materialised: the m68000 libgcc __umodsi3 helper
           contains absolute calls and cannot live in a relocated flat APP.
           __udivsi3 and __mulsi3 themselves are position independent. */
        volatile uint32_t quotient=p->frac/divisor;
        delay=quotient;
        p->frac-=delay*divisor;
    }
    if(delay<1u)delay=1u;
    if(++p->tick>=p->speed){
        p->tick=0u;
        if(p->jump){
            p->jump=0u;
            p->order=p->jump_order;p->row=p->jump_row;
        }else{
            if(++p->row>=64u){p->row=0u;++p->order;}
        }
        if(p->order>=p->song_len){p->finished=1u;}
    }
    return delay;
}
