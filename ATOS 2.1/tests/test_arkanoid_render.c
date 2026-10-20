#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define ARK_RENDER_TEST
#define app_main unused_app_main
#include "../system/arkanoid.c"
#undef app_main
static unsigned char pixels[HEIGHT][WIDTH],expected[HEIGHT][WIDTH];
static unsigned long area,commands,batches;
int32_t gui_call(uint32_t a,uint32_t b,uint32_t c,void *d,void *e)
{(void)a;(void)b;(void)c;(void)d;(void)e;return -1;}
static void pixel(int x,int y,unsigned color)
{if(x>=0&&x<WIDTH&&y>=0&&y<HEIGHT)pixels[y][x]=(unsigned char)color;}
void render_test_batch(const gui_draw_t *ds,unsigned n)
{
    unsigned i;int l=WIDTH,t=HEIGHT,r=0,b=0;
    ++batches;commands+=n;
    for(i=0;i<n;i++){
        const gui_draw_t *d=ds+i;int x,y,w=d->w,h=d->h;
        if(d->kind==GUI_TEXT){w=(int)strlen(d->text)*8;h=16;}
        if(d->x<l)l=d->x;
        if(d->y<t)t=d->y;
        if(d->x+w>r)r=d->x+w;
        if(d->y+h>b)b=d->y+h;
        if(d->kind==GUI_FILL){
            for(y=0;y<h;y++)for(x=0;x<w;x++)pixel(d->x+x,d->y+y,d->color);
        }else if(d->kind==GUI_TEXT){
            /* Deterministic opaque glyph surrogate, with the real 8x16 size.
             * Checks erasure/clipping/layer order, not the system font. */
            for(y=0;y<16;y++)for(x=0;x<w;x++){
                unsigned char ch=(unsigned char)d->text[x/8];
                unsigned on=y>1&&y<14&&((ch>>(x&7))&1u);
                pixel(d->x+x,d->y+y,on?d->color:d->background);
            }
        }else assert(!"unexpected drawing command");
    }
    if(r>l&&b>t)area+=(unsigned long)(r-l)*(b-t);
}
static void check(const char *phase,unsigned frame)
{
    int x,y;
    paint(0);memcpy(expected,pixels,sizeof(pixels));
    paint(1);
    for(y=0;y<HEIGHT;y++)for(x=0;x<WIDTH;x++)if(expected[y][x]!=pixels[y][x]){
        fprintf(stderr,"render mismatch %s frame %u at %d,%d: %u vs %u\n",phase,frame,x,y,expected[y][x],pixels[y][x]);exit(1);
    }
}
int main(void)
{
    unsigned i;unsigned long steady_area,steady_commands;
    ark_new(&game);paint(1);
    for(i=0;i<20;i++){ark_move(&game,24+(int)i*8);check("ready",i);}
    ark_action(&game);
    for(i=0;i<600;i++){
        if(game.state==ARK_READY)ark_action(&game);
        ark_move(&game,game.x/FP);ark_step(&game,0);check("play",i);
        if(i==50){ark_pause(&game);check("pause",i);ark_pause(&game);check("resume",i);}
    }
    copytext(label,"40 FPS",sizeof label);hud_pending=1;check("FPS counter",0);
    game.score=999999;check("score grows",0);game.score=0;check("score shrinks",0);
    /* Deliberate dynamic overlaps and transitions. */
    for(i=0;i<4;i++){
        game.state=ARK_PLAY;game.drop_active=1;game.drop_type=(uint8_t)i;
        game.drop_x=112;game.drop_y=230;game.x=112*FP;game.y=231*FP;
        check("drop overlap",i);ark_step(&game,0);check("catch bonus",i);
    }
    game.state=ARK_OVER;check("game over",0);ark_action(&game);check("restart",0);
    ark_level(&game,7);check("new level",0);
    for(i=0;i<ARK_CELLS;i++)game.hp[i]=0;
    game.remaining=1;game.state=ARK_PLAY;game.x=112*FP;game.y=100*FP;
    game.drop_active=0;paint(1);area=commands=batches=0;
    for(i=0;i<20;i++){game.x+=FP;paint(0);}
    steady_area=area;steady_commands=commands;
    assert(steady_area/20<256); /* ball only, no distant stationary paddle */
    assert(batches==20);
    printf("PASS: incremental pixels equal full repaint; ball-only average %lu pixels, %lu commands/frame\n",steady_area/20,steady_commands/20);
    area=commands=batches=0;paint(0);assert(!commands&&!batches); /* no idle redraw */
    /* 50 Hz simulation / 40 Hz presentation: a held arrow used to advance
       12,12,12,24 px. Interpolation must produce uniform 15 px frames. */
    {uint32_t phase=0;int prev=-1;ark_game saved;
     ark_new(&game);ark_move(&game,40);remember_pose();paint(1);
     for(i=0;i<16;i++){
        phase+=15360u; /* 25 ms * 50 * 12288, exact rational test clock */
        while(phase>=12288u){remember_pose();ark_step(&game,1);phase-=12288u;}
        saved=game;
        paint_interpolated(0,phase);memcpy(expected,pixels,sizeof(pixels));
        assert(!memcmp(&saved,&game,sizeof game)); /* render never mutates physics */
        if(prev>=0)assert(old_p-prev==15);
        prev=old_p;
        paint_interpolated(1,phase);assert(!memcmp(expected,pixels,sizeof pixels));
     }}
    /* Interpolated moving ball/paddle still erase exactly their old pixels. */
    ark_action(&game);remember_pose();
    for(i=0;i<150;i++){
        unsigned state=game.state,level=game.level;
        remember_pose();ark_move(&game,game.x/FP);ark_step(&game,0);
        if(state!=game.state||level!=game.level)remember_pose();
        paint_interpolated(0,(i*997u)%12288u);memcpy(expected,pixels,sizeof pixels);
        paint_interpolated(1,(i*997u)%12288u);assert(!memcmp(expected,pixels,sizeof pixels));
    }
    /* A mouse packet must not snap a keyboard-controlled paddle elsewhere. */
    {gui_message_t m={0};int32_t p;
     ark_new(&game);remember_pose();keys[75]=1;p=game.paddle;
     m.v[0]=KMOU;m.v[1]=((uint32_t)(wx+80)<<16)|(uint16_t)(wy+116);
     event(&m);assert(game.paddle==p);
     keys[75]=0;event(&m);assert(game.paddle==40*FP);
     assert(previous_pose.paddle==game.paddle);
     m.v[0]=KKEY;m.v[1]=0x01000000u|19u;event(&m);
     assert(game.state==ARK_READY&&previous_pose.paddle==game.paddle);
    }
    puts("PASS: uniform interpolated motion, unchanged physics, clean pixels and input ownership");
    return 0;
}
