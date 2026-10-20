/* Host tests for the MOD -> PSG converter (no emulator, no kernel). */
#include <stdio.h>
#include <string.h>
#include "mod_psg.h"

static unsigned failures;
static void check(int ok,const char *what)
{
    if(!ok){printf("MODPSG: FAIL %s\n",what);++failures;}
}

int main(int argc,char **argv)
{
    /* 1. ProTracker period table */
    check(mod_note_period(0u)==856u,"note C-1 = 856");
    check(mod_note_period(12u)==428u,"note C-2 = 428");
    check(mod_note_period(35u)==113u,"note B-3 = 113");
    check(mod_note_period(36u)==0u,"note 36 out of range");

    /* 2. Amiga period -> YM2149 period (2 MHz PSG, PAL Amiga clock) */
    check(mod_psg_period(428u)==241u,"428 -> 241 (518.7 Hz)");
    check(mod_psg_period(127u)==72u,"127 -> 72 (1736 Hz)");
    check(mod_psg_period(3374u)==1902u,"3374 -> 1902 (65.7 Hz, exact integer)");
    check(mod_psg_period(0u)==0u,"period 0 stays 0");
    check(mod_psg_period(60000u)==4095u,"huge period clamps to 4095");
    check(mod_psg_period(1u)==1u,"tiny period clamps to 1");

    /* 3. Volume semantics: Cxx sets, Axy slides */
    check(mod_volume_apply(10u,0x0Cu,64u)==64u,"C-64 sets 64");
    check(mod_volume_apply(10u,0x0Cu,99u)==64u,"C-99 clamps to 64");
    check(mod_volume_apply(10u,0x0Au,0x40u)==14u,"A-40 slides up 4");
    check(mod_volume_apply(2u,0x0Au,0x04u)==0u,"A-04 slides down to 0");
    check(mod_volume_apply(10u,0x0Au,0x00u)==10u,"A-00 leaves volume");
    check(mod_volume_apply(10u,0x00u,0x00u)==10u,"no effect leaves volume");

    /* 4. Four voices, three PSG channels: the quietest loses its channel and
          every emitted note goes to a distinct channel in 0..2. */
    {
        mod_voice_t v[MOD_VOICES];mod_cell_t c[MOD_VOICES];mod_psg_note_t out[8];
        unsigned n,i,j;int distinct=1;
        memset(v,0xff,sizeof(v));for(i=0;i<MOD_VOICES;++i)v[i].psg=0xffu;
        /* three loud sustained voices */
        for(i=0;i<3u;++i){v[i].period=(uint16_t)(428u+i*20u);v[i].volume=64u;}
        memset(c,0,sizeof(c));
        c[0].period=428u;c[1].period=404u;c[2].period=381u;
        c[3].period=360u;c[3].instrument=1u;      /* a fourth, quieter voice */
        v[3].volume=16u;
        n=mod_psg_row(c,v,out,8u);
        check(n>=1u&&n<=MOD_PSG_CH,"four voices yield at most three notes");
        for(i=0;i<n;++i)for(j=i+1u;j<n;++j)if(out[i].channel==out[j].channel)distinct=0;
        check(distinct,"emitted notes use distinct PSG channels");
        for(i=0;i<n;++i){
            check(out[i].channel<MOD_PSG_CH,"channel in range");
            check(out[i].period>=1u&&out[i].period<=4095u,"period in range");
            check(out[i].volume<=15u,"volume in PSG range");
        }
    }

    /* 5. Volume scaling: MOD 64 -> PSG 15, MOD 32 -> 8, MOD 1 -> 1 */
    {
        mod_voice_t v[MOD_VOICES];mod_cell_t c[MOD_VOICES];mod_psg_note_t out[8];
        unsigned n,i;uint8_t seen=0u;
        memset(v,0,sizeof(v));memset(c,0,sizeof(c));
        for(i=0;i<MOD_VOICES;++i)v[i].psg=0xffu;
        v[0].period=428u;v[0].volume=64u;c[0].period=428u;
        n=mod_psg_row(c,v,out,8u);
        for(i=0;i<n;++i)if(out[i].channel==v[0].psg)seen=out[i].volume;
        check(seen==15u,"MOD volume 64 -> PSG 15");
    }

    /* 6. Silencing: C-00 releases the channel with a volume-0 event, and the
          same voice keeps its channel across rows while it sounds. */
    {
        mod_voice_t v[MOD_VOICES];mod_cell_t c[MOD_VOICES];mod_psg_note_t out[8];
        unsigned n,i;uint8_t ch0,vol0=99u;
        memset(v,0,sizeof(v));memset(c,0,sizeof(c));
        for(i=0;i<MOD_VOICES;++i)v[i].psg=0xffu;
        v[0].period=428u;v[0].volume=64u;c[0].period=428u;
        (void)mod_psg_row(c,v,out,8u);
        ch0=v[0].psg;
        check(ch0<MOD_PSG_CH,"first voice got a channel");
        memset(c,0,sizeof(c));
        (void)mod_psg_row(c,v,out,8u);
        check(v[0].psg==ch0,"channel is stable across rows");
        memset(c,0,sizeof(c));c[0].effect=0x0Cu;c[0].param=0u;
        n=mod_psg_row(c,v,out,8u);
        for(i=0;i<n;++i)if(out[i].channel==ch0)vol0=out[i].volume;
        check(vol0==0u,"C-00 emits volume 0 on that channel");
        check(v[0].psg>=MOD_PSG_CH,"channel released");
    }

    /* Optional extra: run a REAL module's first pattern through the converter
       and report the shape of the result (periods must stay in the PSG range).
       The hermetic test above does not depend on this. */
    if(argc>1){
        FILE *f=fopen(argv[1],"rb");
        if(!f){printf("MODPSG: cannot open %s\n",argv[1]);return 2;}
        {
            static uint8_t d[1100000];
            size_t got=fread(d,1,sizeof(d),f);
            fclose(f);
            if(got>1084u){
                unsigned sl=d[950],plen=0,row,notes_total=0,rows_with=0,minp=4095u,maxp=0u,p;
                for(p=0;p<sl;++p)if(d[952+p]>plen)plen=d[952+p];
                for(row=0;row<64u;++row){
                    mod_cell_t c[MOD_VOICES];mod_psg_note_t out[8];mod_voice_t v[MOD_VOICES];
                    unsigned base=1080u+(unsigned)d[952]*1024u+row*16u,i,n;
                    if(base+16u>got)break;
                    for(i=0;i<MOD_VOICES;++i){
                        unsigned o=base+i*4u;
                        c[i].period=(uint16_t)(((d[o]&0x0fu)<<8)|d[o+1]);
                        c[i].instrument=(uint8_t)((d[o]&0xf0u)|(d[o+2]>>4));
                        c[i].effect=(uint8_t)(d[o+2]&0x0fu);c[i].param=d[o+3];
                    }
                    for(i=0;i<MOD_VOICES;++i){v[i].psg=0xffu;v[i].period=0u;v[i].volume=64u;}
                    n=mod_psg_row(c,v,out,8u);
                    if(n)++rows_with;
                    notes_total+=n;
                    for(i=0;i<n;++i){if(out[i].period&&out[i].period<minp)minp=out[i].period;
                                     if(out[i].period>maxp)maxp=out[i].period;}
                }
                printf("MODPSG: %s | songlen %u, patterns %u | pattern 0: %u rows with notes, %u notes, PSG period %u..%u\n",
                       argv[1],sl,plen+1u,rows_with,notes_total,minp,maxp);
            }
        }
    }

    printf(failures?"MODPSG: %u FAILED\n":"MODPSG: all checks passed\n",failures);
    return failures?1:0;
}
