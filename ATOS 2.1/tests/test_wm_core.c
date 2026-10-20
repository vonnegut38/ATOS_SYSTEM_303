/* Host tests for the compositor core: geometry, clip lists, damage and the
   Z-order coverage of a recompose pass. */
#include <stdio.h>
#include "wm_core.h"
#include "wm_chrome.h"

static int fails;
#define CHECK(c) do{if(!(c)){printf("FAIL %s:%d %s\n",__FILE__,__LINE__,#c);fails++;}}while(0)

/* paints the pid of every blit into a pid map, so we can verify who wins */
static uint8_t map[640*400];
static void record(void *ctx,uint32_t pid,const wm_rect_t *d,uint16_t sx,uint16_t sy)
{
    unsigned x,y;(void)ctx;(void)sx;(void)sy;
    for(y=0;y<d->h;++y)for(x=0;x<d->w;++x){int px=d->x+(int)x,py=d->y+(int)y;if(px>=0&&py>=0&&px<640&&py<400)map[py*640+px]=(uint8_t)pid;}
}

static unsigned area_of_clip(const wm_state_t *s,unsigned w)
{
    unsigned i,a=0;
    for(i=0;i<s->clip_count[w];++i)a+=(unsigned)s->clip[w][i].w*s->clip[w][i].h;
    return a;
}

int main(void)
{
    wm_state_t s;wm_rect_t r;unsigned i;
    /* Interior updates leave chrome alone; expose/title/border/sizer/scroll
       still redraw, including windows partly outside the screen. */
    {wm_rect_t f={-10,20,640,416},d={10,40,8,16},sb={610,36,16,398};
     CHECK(!wm_chrome_damaged(&f,0,&d));
     d=(wm_rect_t){610,40,8,16};CHECK(!wm_chrome_damaged(&f,0,&d));
     CHECK(wm_chrome_damaged(&f,&sb,&d));
     d=(wm_rect_t){20,20,4,16};CHECK(wm_chrome_damaged(&f,0,&d));
     d=(wm_rect_t){-9,100,2,2};CHECK(wm_chrome_damaged(&f,0,&d));
     d=(wm_rect_t){620,426,4,4};CHECK(wm_chrome_damaged(&f,0,&d));
     d=(wm_rect_t){-100,0,20,20};CHECK(!wm_chrome_damaged(&f,0,&d));
     CHECK(wm_chrome_damaged(&f,0,&f));}
    CHECK(wm_open_guard_active(100,100));
    CHECK(wm_open_guard_active(119,100));
    CHECK(!wm_open_guard_active(120,100));
    CHECK(!wm_open_guard_active(19,UINT32_MAX));
    /* geometry */
    {wm_rect_t a={0,0,10,10},b={5,5,10,10};
     CHECK(wm_rect_intersect(&a,&b,&r)&&r.x==5&&r.y==5&&r.w==5&&r.h==5);
     CHECK(wm_rect_union(&a,&b,&r)&&r.x==0&&r.y==0&&r.w==15&&r.h==15);
     {wm_rect_t c={20,20,5,5};CHECK(!wm_rect_intersect(&a,&c,&r));}}

    wm_init(&s,640,400);
    CHECK(s.count==0&&s.damage_count==0);

    /* two windows, B on top of A */
    s.win[0].frame.x=0;  s.win[0].frame.y=0;  s.win[0].frame.w=300;s.win[0].frame.h=200;
    s.win[0].client.x=2; s.win[0].client.y=2; s.win[0].client.w=296;s.win[0].client.h=196;
    s.win[0].pid=1;s.win[0].flags=1;
    s.win[1].frame.x=100;s.win[1].frame.y=50; s.win[1].frame.w=300;s.win[1].frame.h=200;
    s.win[1].client.x=102;s.win[1].client.y=52;s.win[1].client.w=296;s.win[1].client.h=196;
    s.win[1].pid=2;s.win[1].flags=1;
    s.count=2;
    wm_rebuild_clip(&s);
    CHECK(s.clip_count[1]==1);                                  /* top window is fully visible */
    CHECK(area_of_clip(&s,1)==296u*196u);
    /* A is covered wherever B's frame overlaps A's client: 198x146 */
    CHECK(area_of_clip(&s,0)==296u*196u-198u*148u);   /* overlap = 198 x 148 */
    CHECK(s.clip_count[0]>=2&&s.clip_count[0]<=4);

    /* damage merging */
    wm_damage_add(&s,(wm_rect_t){0,0,100,100});
    wm_damage_add(&s,(wm_rect_t){50,50,100,100});
    CHECK(s.damage_count==1);
    wm_damage_add(&s,(wm_rect_t){400,300,100,100});
    CHECK(s.damage_count==2);
    wm_damage_add(&s,(wm_rect_t){1000,1000,10,10});                 /* off screen: dropped */
    CHECK(s.damage_count==2);

    /* recompose coverage: background 0, A=1, B=2, topmost wins */
    wm_damage_clear(&s);
    for(i=0;i<640u*400u;++i)map[i]=0xEE;
    wm_damage_add(&s,(wm_rect_t){0,0,640,400});
    wm_recompose(&s,record,0);
    CHECK(s.damage_count==0);
    CHECK(map[10*640+10]==1);                 /* inside A only */
    CHECK(map[60*640+150]==2);                /* inside B */
    CHECK(map[399*640+639]==0);               /* background */
    CHECK(map[10*640+310]==0);                /* right of A, left of B's x? (background) */

    /* move with content: damage covers old+new, pixels follow, old area is background */
    s.win[1].flags=1;wm_rebuild_clip(&s);
    wm_damage_clear(&s);
    wm_move(&s,1,300,200);                        /* B from (100,50) to (300,200) */
    CHECK(s.damage_count>=1);
    {wm_rect_t u={0,0,0,0};unsigned k;for(k=0;k<s.damage_count;++k){if(!u.w)u=s.damage[k];else wm_rect_union(&u,&s.damage[k],&u);}
     CHECK(u.x<=100&&u.y<=50&&u.x+u.w>=300+300&&u.y+u.h>=200+200);}
    wm_recompose(&s,record,0);
    CHECK(map[250*640+350]==2);                   /* content is now at the new place */
    CHECK(map[60*640+150]==1);                    /* old B area shows A (it was A's client) */

    /* hit testing picks the topmost window and the right part */
    CHECK(wm_hit(&s,350,210)==1);                 /* B was moved to 300,200 */
    CHECK(wm_hit_part(&s,1,350,220)==WM_PART_CLIENT);
    /* fixture has client == frame, so the title band is part of the client */
    CHECK(wm_hit_part(&s,1,350,204)==WM_PART_CLIENT);
    CHECK(wm_hit(&s,5,5)==0);                     /* only A covers the corner */
    CHECK(wm_hit(&s,600,380)==WM_NO_WINDOW);      /* empty desktop */
    /* bit 2 (background canvas): composes but is never hit-tested, so a click
       cannot raise a full-screen application surface over the shell window. */
    s.win[0].flags=1|4;wm_rebuild_clip(&s);
    CHECK(wm_hit(&s,5,5)==WM_NO_WINDOW);
    CHECK(wm_hit(&s,350,210)==1);                 /* B stays hittable */
    s.win[0].flags=1;wm_rebuild_clip(&s);

    /* drag: grabbing the title moves, grabbing the body resizes */
    {wm_drag_t d;
     /* give B a real title band so the parts are distinguishable */
     s.win[1].client.x=s.win[1].frame.x;
     s.win[1].client.y=(int16_t)(s.win[1].frame.y+16);
     s.win[1].client.w=s.win[1].frame.w;
     s.win[1].client.h=(uint16_t)(s.win[1].frame.h-16);
     wm_drag_begin(&d,&s,350,204);                 /* B title (300,200) */
     CHECK(d.active&&d.part==WM_PART_TITLE);
     wm_drag_update(&d,&s,400,254);                /* +50,+50 */
     CHECK(s.win[1].frame.x==350&&s.win[1].frame.y==250);
     wm_drag_end(&d);
     CHECK(!d.active);
     wm_drag_begin(&d,&s,500,350);                 /* B body -> resize */
     CHECK(d.active&&d.part==WM_PART_CLIENT);
     wm_drag_update(&d,&s,540,380);                /* +40,+30 */
     CHECK(s.win[1].frame.w==340&&s.win[1].frame.h==230);
     wm_drag_end(&d);}

    /* resize: old+new damaged, client clamped inside the new frame */
    wm_damage_clear(&s);
    wm_resize(&s,1,200,120);
    CHECK(s.damage_count>=1);
    CHECK(s.win[1].frame.w==200&&s.win[1].frame.h==120);
    CHECK((uint32_t)s.win[1].client.x+s.win[1].client.w<=(uint32_t)s.win[1].frame.x+200);
    CHECK((uint32_t)s.win[1].client.y+s.win[1].client.h<=(uint32_t)s.win[1].frame.y+120);

    /* AES contract (wind_calc WC_WORK / XaAES frame->work delta): growing the
       frame must grow the WORK area too.  Before this the client kept its size,
       so an enlarged window still showed the old, smaller picture - exactly
       "the outline grows but the image stays small". */
    {uint16_t cw=s.win[1].client.w,ch=s.win[1].client.h;
     wm_resize(&s,1,340,230);
     wm_resize_client_to_frame(&s,1);
     CHECK(s.win[1].frame.w==340&&s.win[1].frame.h==230);
     CHECK(s.win[1].client.w==cw+140&&s.win[1].client.h==ch+110);
     CHECK((uint32_t)s.win[1].client.x+s.win[1].client.w<=(uint32_t)s.win[1].frame.x+340);
     CHECK((uint32_t)s.win[1].client.y+s.win[1].client.h<=(uint32_t)s.win[1].frame.y+230);}

    /* hidden window must not paint */
    s.win[1].flags=0;wm_rebuild_clip(&s);
    wm_damage_clear(&s);wm_damage_add(&s,(wm_rect_t){0,0,640,400});wm_recompose(&s,record,0);
    CHECK(map[60*640+150]==1);                /* now A shows through */

    /* click to focus: raising the lower window puts it on top (last), and the
       move must preserve the task-list title and the saved-minimize frame. */
    {unsigned top=s.count-1u;
     const char *want="WINDOW A";
     unsigned k;for(k=0;k<24;++k){s.win[0].title[k]=(char)(k<8?want[k]:0);}
     s.win[0].restore.x=7;s.win[0].restore.y=9;s.win[0].restore.w=33;s.win[0].restore.h=44;
     wm_raise(&s,0);
     CHECK(s.count==top+1u);
     CHECK(s.win[s.count-1u].frame.x==0&&s.win[s.count-1u].frame.y==0);
     CHECK(s.win[s.count-1u].title[0]=='W'&&s.win[s.count-1u].title[7]=='A');
     for(k=8;k<24;++k)CHECK(s.win[s.count-1u].title[k]==0);
     CHECK(s.win[s.count-1u].restore.x==7&&s.win[s.count-1u].restore.w==33);}

    /* task manager: the minimize gadget is the right 16px of a real title
       band, and a minimized window (flag 8, visible cleared) is not hit-tested */
    s.win[1].client.x=s.win[1].frame.x;
    s.win[1].client.y=(int16_t)(s.win[1].frame.y+16);
    s.win[1].client.w=s.win[1].frame.w;
    s.win[1].client.h=(uint16_t)(s.win[1].frame.h-16);
    CHECK(wm_hit_part(&s,1,(int16_t)(s.win[1].frame.x+s.win[1].frame.w-8),
                      (int16_t)(s.win[1].frame.y+4))==WM_PART_CLOSE);
    CHECK(wm_hit_part(&s,1,(int16_t)(s.win[1].frame.x+8),
                      (int16_t)(s.win[1].frame.y+4))==WM_PART_TITLE);
    CHECK(wm_hit_part(&s,1,(int16_t)(s.win[1].frame.x+s.win[1].frame.w-24),
                      (int16_t)(s.win[1].frame.y+4))==WM_PART_MINIMIZE);
    {uint8_t keep=s.win[1].flags;
     s.win[1].flags=8;wm_rebuild_clip(&s);
     CHECK(wm_hit(&s,(int16_t)(s.win[1].frame.x+8),(int16_t)(s.win[1].frame.y+4))==WM_NO_WINDOW);
     s.win[1].flags=(uint8_t)(keep|1u);wm_rebuild_clip(&s);
     CHECK(wm_hit(&s,(int16_t)(s.win[1].frame.x+8),(int16_t)(s.win[1].frame.y+4))==1);}

    printf(fails?"WM_CORE: %d FAIL\n":"WM_CORE: ALL PASS\n",fails);
    return fails?1:0;
}
