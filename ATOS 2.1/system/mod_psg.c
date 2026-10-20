/* MOD -> PSG conversion.  See include/mod_psg.h for the honest scope note. */
#include "mod_psg.h"

/* ProTracker period table for notes 0..35 (C-1 .. B-3). */
static const uint16_t note_period[36]={
    856,808,762,720,678,640,604,570,538,508,480,453,
    428,404,381,360,339,320,302,285,269,254,240,226,
    214,202,190,180,170,160,151,143,135,127,120,113
};

uint16_t mod_note_period(uint8_t note)
{
    if(note>=36u)return 0u;
    return note_period[note];
}

uint16_t mod_psg_period(uint16_t amiga_period)
{
    /* f = 3546895 / (16 * P_amiga)  and  f = 2000000 / (16 * P_psg), so
       P_psg = 2000000 * P_amiga / 3546895 = P_amiga * 0.56385 (0.002 % error).
       The ratio is applied as fixed point on purpose: P_amiga * 2000000
       overflows 32 bits (3374 * 2000000 = 6.7e9) and a 68k app should not need
       64-bit math for this. */
    uint32_t n;
    if(!amiga_period)return 0u;
    if(amiga_period>7264u)return 4095u;     /* would exceed the 12-bit register anyway */
    n=((uint32_t)amiga_period*56385u+50000u)/100000u;
    if(n<1u)n=1u;
    if(n>4095u)n=4095u;                     /* 12-bit register */
    return (uint16_t)n;
}

uint8_t mod_volume_apply(uint8_t volume,uint8_t effect,uint8_t param)
{
    if(volume>64u)volume=64u;
    if(effect==0x0Cu){                  /* Cxx: set volume */
        volume=param>64u?64u:param;
    }else if(effect==0x0Au){            /* Axy: volume slide */
        unsigned up=(unsigned)(param>>4),down=(unsigned)(param&0x0fu);
        if(up&&!down){unsigned v=(unsigned)volume+up;volume=(uint8_t)(v>64u?64u:v);}
        else if(down&&!up){unsigned v=(unsigned)volume;volume=(uint8_t)(v>down?v-down:0u);}
    }
    return volume;
}

/* Pick a PSG channel for a voice that needs one.  Never steals from a voice
   that is louder; on a tie the lowest MOD voice index loses, which keeps the
   behaviour deterministic and testable. */
static unsigned pick_channel(mod_voice_t voice[MOD_VOICES],unsigned me)
{
    unsigned j,free_ch=MOD_PSG_CH,best=MOD_VOICES;
    uint8_t best_vol=0xffu;
    for(j=0;j<MOD_PSG_CH;++j){          /* free? */
        unsigned k,used=0u;
        for(k=0;k<MOD_VOICES;++k)if(k!=me&&voice[k].psg==(uint8_t)j)used=1u;
        if(!used){free_ch=j;break;}
    }
    if(free_ch<MOD_PSG_CH)return free_ch;
    for(j=0;j<MOD_VOICES;++j){          /* steal the quietest other voice */
        if(j==me)continue;
        if(voice[j].psg>=MOD_PSG_CH)continue;
        if(voice[j].volume<best_vol){best_vol=voice[j].volume;best=j;}
    }
    if(best<MOD_VOICES){
        uint8_t ch=voice[best].psg;
        voice[best].psg=0xffu;
        return ch;
    }
    return 0u;                          /* should not happen */
}

unsigned mod_psg_row(const mod_cell_t cell[MOD_VOICES],mod_voice_t voice[MOD_VOICES],
                     mod_psg_note_t *out,unsigned max)
{
    unsigned i,ch,n=0u;
    for(i=0;i<MOD_VOICES;++i){
        if(cell[i].period){                     /* new note on this voice */
            voice[i].period=cell[i].period;
            if(voice[i].psg>=MOD_PSG_CH)voice[i].psg=0xffu;   /* needs a channel */
        }
        /* Volume 0 silences the voice but keeps its period: a later volume
           slide up resumes the same note, as MOD players do. */
        voice[i].volume=mod_volume_apply(voice[i].volume,cell[i].effect,cell[i].param);
    }
    /* Fold voices whose volume reached zero: release their channel. */
    for(i=0;i<MOD_VOICES;++i)
        if(!voice[i].volume&&voice[i].psg<MOD_PSG_CH){
            if(n<max){out[n].channel=voice[i].psg;out[n].volume=0u;out[n].period=0u;++n;}
            voice[i].psg=0xffu;
        }
    /* Give a channel to every sounding voice that has none. */
    for(i=0;i<MOD_VOICES;++i){
        if(!voice[i].volume||!voice[i].period)continue;
        if(voice[i].psg>=MOD_PSG_CH)voice[i].psg=(uint8_t)pick_channel(voice,i);
    }
    /* Emit the current state of each PSG channel (last owner wins). */
    for(ch=0;ch<MOD_PSG_CH;++ch){
        unsigned owner=MOD_VOICES;
        uint16_t per=0u;uint8_t vol=0u;
        for(i=0;i<MOD_VOICES;++i)
            if(voice[i].psg==(uint8_t)ch&&voice[i].volume&&voice[i].period){
                owner=i;per=mod_psg_period(voice[i].period);vol=(uint8_t)((voice[i].volume+3u)/4u);
                if(vol>15u)vol=15u;
            }
        if(owner==MOD_VOICES)continue;
        if(n>=max)break;
        out[n].channel=(uint8_t)ch;out[n].volume=vol;out[n].period=per;++n;
    }
    return n;
}
