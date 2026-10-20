#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "tos_damage.h"

static tos_damage_t d;
static uint16_t screen_words[16001],shadow_words[16001];
static unsigned char covered[400][640];
static void take_all(void)
{
    tos_damage_rect_t r;
    while(tos_damage_take(&d,&r))
        for(int y=r.y;y<r.y+r.h;y++)for(int x=r.x;x<r.x+r.w;x++)covered[y][x]=1;
}
static void reset(void)
{
    memset(screen_words,0,sizeof screen_words);memset(shadow_words,0,sizeof shadow_words);
    memset(covered,0,sizeof covered);tos_damage_reset(&d,640,400);
}
int main(void)
{
    unsigned char *s=(unsigned char *)screen_words,*p=(unsigned char *)shadow_words;
    tos_damage_rect_t r;
    /* Two distant raw writes must BOTH survive acknowledgement by the scan. */
    reset();s[80*80+2]=0x80;s[80*320+61]=0x01;
    tos_damage_scan(&d,s,p,80,1);take_all();
    assert(covered[80][16]&&covered[320][495]);
    assert(!covered[80][400]&&!covered[200][200]);
    tos_damage_scan(&d,s,p,80,1);assert(!tos_damage_take(&d,&r));
    assert(!memcmp(s,p,32000));
    /* API drawing cannot starve detection of concurrent direct screen writes. */
    reset();
    for(int i=0;i<100;i++){
        tos_damage_note(&d,2,2,10,18);s[80*350+50]^=1;
        tos_damage_scan(&d,s,p,80,1);take_all();
        assert(covered[350][407]);
    }
    /* Retain all pixels of overlapping updates; clip off-screen rectangles. */
    reset();tos_damage_note(&d,-10,-10,8,4);tos_damage_note(&d,630,398,900,600);
    tos_damage_note(&d,10,2,30,3);take_all();
    assert(covered[0][0]&&covered[2][29]&&covered[399][639]);
    assert(!covered[20][20]);
    /* Colour uses whole rows because source words contain interleaved planes. */
    reset();tos_damage_reset(&d,320,200);s[160*70+100]=1;
    tos_damage_scan(&d,s,p,160,0);assert(tos_damage_take(&d,&r));
    assert(r.x==0&&r.w==320&&r.y==70&&r.h==1);
    /* Odd source pointers take the byte path, without unaligned word reads. */
    reset();s[80*3+4]=1;tos_damage_scan(&d,s+1,p+1,80,1);
    assert(tos_damage_take(&d,&r)&&r.y==3&&r.x==24&&r.w==8);
    /* Calls do not consume time. Deadlines also survive a 32-bit tick wrap. */
    for(int i=0;i<10000;i++)assert(!tos_present_due(100,100,2));
    assert(!tos_present_due(101,100,2)&&tos_present_due(102,100,2));
    assert(tos_present_due(1,UINT32_MAX,2));
    puts("tos_damage: PASS (lost bands, mixed writers, clipping, spans, colour, alignment, deadlines)");
    return 0;
}
