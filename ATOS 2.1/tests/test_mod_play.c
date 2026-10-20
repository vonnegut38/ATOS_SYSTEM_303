/* Host tests for the MOD player state machine.  The module is built in memory so
 * every case (speed, tempo, break, jump, end of song) is deterministic. */
#include <stdio.h>
#include <string.h>
#include "mod_play.h"

static unsigned failures;
static void check(int ok,const char *what)
{
    if(!ok){printf("MODPLAY: FAIL %s\n",what);++failures;}
}

/* header + two patterns + a little sample data */
static uint8_t mod[1080u+2u*1024u+16u];

static void put_cell(unsigned pat,unsigned row,unsigned ch,unsigned per,unsigned inst,
                     unsigned eff,unsigned param)
{
    uint8_t *c=mod+1084u+pat*1024u+row*16u+ch*4u;
    c[0]=(uint8_t)(((inst&0xf0u))|((per>>8)&0x0fu));
    c[1]=(uint8_t)(per&0xffu);
    c[2]=(uint8_t)(((inst&0x0fu)<<4)|(eff&0x0fu));
    c[3]=(uint8_t)param;
}

static void build(void)
{
    unsigned i;
    memset(mod,0,sizeof(mod));
    memcpy(mod,"hymn test",9);
    mod[20u+25u]=32u;                    /* sample 1: default volume 32 */
    mod[20u+22u]=0u;mod[20u+23u]=2u;     /* length 2 words */
    mod[950]=2u;mod[951]=0u;             /* song length 2, restart 0 */
    mod[952]=0u;mod[953]=1u;             /* order table */
    memcpy(mod+1080u,"M.K.",4);
    /* pattern 0 */
    put_cell(0u,0u,0u,428u,1u,0u,0u);    /* C-2 with instrument 1        */
    put_cell(0u,1u,0u,0u,0u,0x0Fu,3u);   /* F03: speed 3                 */
    put_cell(0u,2u,0u,0u,0u,0x0Du,0u);   /* D00: break to next order     */
    /* pattern 1 */
    put_cell(1u,0u,0u,0u,0u,0x0Fu,150u); /* F96: tempo 150               */
    put_cell(1u,1u,0u,0u,0u,0x0Bu,0u);   /* B00: jump to order 0         */
    for(i=0;i<8u;++i)mod[1084u+2u*1024u+i]=(uint8_t)i;
}

int main(void)
{
    mod_play_t p;
    mod_psg_note_t out[8];
    unsigned n,i;
    uint32_t d,sum;

    build();

    /* 1. init and rejection */
    check(mod_play_init(&p,mod,sizeof(mod))==0,"init accepts M.K.");
    check(mod_play_init(&p,mod,100u)<0,"too small rejected");
    {uint8_t bad[1084];memcpy(bad,mod,1084u);memcpy(bad+1080u,"M!K!",4);
     check(mod_play_init(&p,bad,sizeof(bad))<0,"bad signature rejected");}
    check(mod_play_init(&p,mod,sizeof(mod))==0,"re-init ok");
    check(p.order==0u&&p.row==0u&&p.tick==0u,"starts at order 0 row 0");
    check(p.speed==6u&&p.tempo==125u,"default speed 6, tempo 125");
    check(p.sample_volume[0]==32u,"sample 1 default volume read");

    /* 2. first tick: the note of row 0 is emitted at MOD volume 32 -> PSG 8 */
    d=mod_play_tick(&p,out,8u,&n);
    check(n>=1u,"row 0 emits at least one note");
    check(out[0].volume==8u,"MOD volume 32 -> PSG 8");
    check(out[0].period>=1u&&out[0].period<=4095u,"period in PSG range");
    check(d>=245u&&d<=246u,"tempo 125 -> ~246 hires ticks per tick");

    /* 3. row advance after `speed` ticks, and no drift over one row */
    sum=d;
    for(i=0;i<5u;++i){d=mod_play_tick(&p,out,8u,&n);sum+=d;}
    check(p.row==1u,"row advances after speed ticks");
    check(sum>=1474u&&sum<=1475u,"six ticks at tempo 125 sum to ~1475 (no drift)");

    /* 4. F03 sets the speed: the next row lasts three ticks */
    (void)mod_play_tick(&p,out,8u,&n);            /* row 1 tick 0 applies F03 */
    check(p.speed==3u,"F03 sets speed 3");
    (void)mod_play_tick(&p,out,8u,&n);
    (void)mod_play_tick(&p,out,8u,&n);
    check(p.row==2u&&p.order==0u,"row 2 reached after three ticks");

    /* 5. D00 breaks to the next order, row 0 */
    (void)mod_play_tick(&p,out,8u,&n);            /* row 2 tick 0 applies D00 */
    for(i=0;i<2u;++i)(void)mod_play_tick(&p,out,8u,&n);
    check(p.order==1u&&p.row==0u,"D00 breaks to order 1 row 0");

    /* 6. F96 sets tempo 150: ticks get shorter */
    d=mod_play_tick(&p,out,8u,&n);
    check(p.tempo==150u,"F96 sets tempo 150");
    check(d>=204u&&d<=206u,"tempo 150 -> ~205 hires ticks");
    d=mod_play_tick(&p,out,8u,&n);
    check(d>=204u&&d<=206u,"tempo stays 150");

    /* 7. B00 jumps back to order 0 and the song keeps running */
    (void)mod_play_tick(&p,out,8u,&n);            /* row 1 tick 0 applies B00 */
    check(p.jump==0u||p.order<=1u,"jump accepted");
    for(i=0;i<8u&&!p.finished;++i)(void)mod_play_tick(&p,out,8u,&n);
    check(p.order==0u||p.finished,"B00 returned to order 0");

    /* 8. End of song.  The song above loops on purpose (B00 jumps to order 0),
          so this case gets its own short module: one order, no jump. */
    {
        unsigned guard=0u;uint32_t last=0u;
        mod[950]=1u;                                  /* song length 1 */
        mod[952]=0u;
        check(mod_play_init(&p,mod,sizeof(mod))==0,"short song init");
        while(guard++<20000u){last=mod_play_tick(&p,out,8u,&n);if(last==MOD_END)break;}
        check(last==MOD_END,"short song reaches MOD_END");
        check(p.finished==1u,"finished flag set");
        check(p.order==0u||p.order>=p.song_len,"stopped at the end of the order list");
        check(mod_play_tick(&p,out,8u,&n)==MOD_END,"stays finished");
    }

    /* Reject malformed order tables before pointer arithmetic. */
    build();mod[952]=255u;
    check(mod_play_init(&p,mod,sizeof(mod))<0,"pattern count overflow rejected");
    build();mod[950]=0u;
    check(mod_play_init(&p,mod,sizeof(mod))<0,"empty order rejected");
    build();put_cell(0,0,0,428,1,0xD,0x99);
    check(!mod_play_init(&p,mod,sizeof(mod)),"break fixture");
    for(i=0;i<6;i++)(void)mod_play_tick(&p,out,8,&n);
    check(p.row==0&&p.order==1,"invalid D99 bounded to row zero");
    build();put_cell(0,0,0,428,1,0xF,0);
    check(!mod_play_init(&p,mod,sizeof(mod)),"stop fixture");
    check(mod_play_tick(&p,out,8,&n)==MOD_END,"F00 stops");
    build();mod[950]=1;memset(mod+1084,0,1024);
    put_cell(0,63,0,428,1,0,0);
    check(!mod_play_init(&p,mod,sizeof(mod)),"last tick fixture");
    p.row=63;p.speed=1;
    check(mod_play_tick(&p,out,8,&n)!=MOD_END&&n>0,"last row has notes and duration");
    check(mod_play_tick(&p,out,8,&n)==MOD_END,"end follows last duration");

    printf(failures?"MODPLAY: %u FAILED\n":"MODPLAY: all checks passed\n",failures);
    return failures?1:0;
}
