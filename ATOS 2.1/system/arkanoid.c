/* Native ATOS GUI backend. No SFML, TOS traps or direct framebuffer writes. */
#include "gui_app.h"
#include "game.h"
#include "colours.h"
#include "app_abi.h"

/* ------------------------------------------------------------------- sound
   The game logic raises events (system/game.c); this drains them once per
   simulation tick and hands them to the audio service in one batch.  It is
   deliberately NOT in the paint path: repainting is not another collision.
   When the kernel has no audio the capability word says so and we simply drop
   the events instead of pretending to play them. */
static int audio_ready=-1;              /* -1 = not asked, 0 = absent, 1 = present */
static uint32_t sound_dropped;

static void sound_map(unsigned kind,atos_audio_event_t *e)
{
    e->wait=0u;e->pad[0]=e->pad[1]=e->pad[2]=0u;
    switch(kind){
    case ARK_SND_BRICK_HIT:   e->op=ATOS_AUDIO_OP_TONE;e->a=0u;e->aux=180u;e->b=8u;break;
    case ARK_SND_BRICK_BREAK: e->op=ATOS_AUDIO_OP_TONE;e->a=0u;e->aux=120u;e->b=12u;break;
    case ARK_SND_PADDLE:      e->op=ATOS_AUDIO_OP_TONE;e->a=1u;e->aux=260u;e->b=10u;break;
    case ARK_SND_WALL:        e->op=ATOS_AUDIO_OP_TONE;e->a=1u;e->aux=90u;e->b=7u;break;
    case ARK_SND_BONUS_SPAWN: e->op=ATOS_AUDIO_OP_TONE;e->a=2u;e->aux=300u;e->b=6u;break;
    case ARK_SND_BONUS_TAKE:  e->op=ATOS_AUDIO_OP_TONE;e->a=2u;e->aux=60u;e->b=12u;break;
    case ARK_SND_LEVEL_UP:    e->op=ATOS_AUDIO_OP_TONE;e->a=2u;e->aux=45u;e->b=13u;break;
    case ARK_SND_LIFE_LOST:   e->op=ATOS_AUDIO_OP_NOISE;e->a=0u;e->aux=24u;e->b=12u;break;
    case ARK_SND_GAME_OVER:   e->op=ATOS_AUDIO_OP_NOISE;e->a=0u;e->aux=31u;e->b=14u;break;
    default:                  e->op=ATOS_AUDIO_OP_NONE;e->a=0u;e->aux=0u;e->b=0u;break;
    }
}

static void sound_flush(void)
{
    atos_audio_event_t batch[8];
    unsigned n=0,kind;
    if(audio_ready<0)
        audio_ready=(audio_caps()&ATOS_AUDIO_CAP_PSG_TONE)?1:0;
    while(n<8u&&(kind=ark_sound_poll())!=ARK_SND_NONE){
        if(!audio_ready){++sound_dropped;continue;}
        sound_map(kind,&batch[n]);
        if(batch[n].op==ATOS_AUDIO_OP_NONE){++sound_dropped;continue;}
        /* A PSG tone has no envelope: without a stop the last effect sounds
           forever - that is the continuous squeal heard in the demo.  Every
           effect becomes a short blip: the tone, then a stop ~50 ms later.
           Noise is mixed into channel C, so its stop targets that channel. */
        if(n+1u<8u){
            uint8_t mask=(batch[n].op==ATOS_AUDIO_OP_NOISE)
                       ? (uint8_t)(1u<<2u)
                       : (uint8_t)(1u<<(batch[n].a&3u));
            atos_audio_event_t *st=&batch[n+1u];
            st->wait=600u;st->aux=0u;st->op=ATOS_AUDIO_OP_STOP;st->a=mask;st->b=0u;
            st->pad[0]=st->pad[1]=st->pad[2]=0u;
            ++n;
        }
        ++n;
    }
    if(n&&audio_queue(batch,n)<0)++sound_dropped;
}
#define WIDTH 448
#define HEIGHT 536
#define KKEY 0x4b4b4559u
#define KMOU 0x4b4d4f55u
#define WMOV 0x574d4f56u
#define WCLS 0x57434c53u
static ark_game game;
typedef struct {int32_t x,y,paddle;} ark_pose_t;
static ark_pose_t previous_pose,draw_pose;
static uint8_t interpolated_draw;
static void remember_pose(void)
{previous_pose.x=game.x;previous_pose.y=game.y;previous_pose.paddle=game.paddle;}
/* Physics stays at 50 Hz. Present between the last two simulation poses so
 * a 40 Hz display does not alternate 12/24-pixel keyboard paddle jumps.
 * Only drawing uses this pose; collisions always use the real game state. */
static int32_t interpolate(int32_t before,int32_t after,uint32_t phase)
{return before+(after-before)*(int32_t)phase/12288;}

static unsigned frames,simsteps;
static char label[32]="256 COLOURS";
static uint8_t demo,hud_pending;
static uint32_t fps_since,fps_frames;
static int32_t gfx_pid;
static int16_t wx=160,wy=28;
static uint8_t quit,keys[128],left_button,repaint;
static gui_draw_t draws[3][GUI_DRAW_MAX];
static unsigned draw_bank,in_flight;
static gui_draw_t *frame_draws;
static uint16_t frame_groups[GUI_FRAME_GROUPS];
static unsigned frame_count,frame_group_count;
static uint8_t fast_frames,hidden_frame;
static void submit_frame(void)
{
#ifndef ARK_RENDER_TEST
    if(frame_count){
        int32_t rc=gui_call(ATOS_CALL_GUI_FRAME,frame_count,frame_group_count,
                            frame_draws,frame_groups);
        if(rc<0)quit=1;
        hidden_frame=(uint8_t)(rc==1);
        frame_count=frame_group_count=0;
    }
#endif
}
static gui_message_t pending[64];
static unsigned pending_head,pending_tail;
/* Keep input outside synchronous drawing: a service reply may arrive after
 * a key/mouse event, but a half-rendered frame must not mutate the game. */
static void copy_message(gui_message_t *d,const gui_message_t *s)
{
    d->sender=s->sender;d->v[0]=s->v[0];d->v[1]=s->v[1];
    d->v[2]=s->v[2];d->v[3]=s->v[3];
}
static void queue(const gui_message_t *m)
{
    if(m->sender)return;
    if(m->v[0]==KMOU&&pending_head!=pending_tail){
        gui_message_t *last=&pending[(pending_tail-1u)&63u];
        if(last->v[0]==KMOU&&last->v[2]==m->v[2]){copy_message(last,m);return;}
    }
    if(pending_tail-pending_head<64u)copy_message(&pending[pending_tail++&63u],m);
}
typedef struct {int x,y,w,h;} rect_t;
static rect_t damage[32],clip;
static unsigned damage_count;
static int clipping;
static unsigned count;
static uint8_t old_hp[ARK_CELLS];
static int old_x,old_y,old_p,old_half,old_drop_x,old_drop_y;
static unsigned old_drop,old_level=255,old_state=255,old_lives=255,old_power=255;
static uint32_t old_score=0xffffffffu;
static void event(const gui_message_t *m)
{
    if(m->sender)return;
    if(m->v[0]==WCLS){quit=1;return;}
    if(m->v[0]==WMOV){wx=(int16_t)(m->v[1]>>16);wy=(int16_t)m->v[1];return;}
    unsigned state_before=game.state,level_before=game.level;
    if(m->v[0]==KKEY){
        unsigned k=m->v[1]&127u,down=((m->v[1]>>24)&255u)==1u;
        if(down&&!keys[k]){
            if(k==1)quit=1;
            else if(k==57)ark_action(&game);
            else if(k==25)ark_pause(&game);
            else if(k==32){demo^=1u;if(demo)ark_action(&game);}
            else if(k==19){ark_new(&game);remember_pose();repaint=1;}
        }
        keys[k]=(uint8_t)down;
    }else if(m->v[0]==KMOU){
        int x=(int)(uint16_t)(m->v[1]>>16)-wx;
        int y=(int)(uint16_t)m->v[1]-wy-16;
        unsigned button=(m->v[2]>>24)&1u;
        if(x>=16&&x<432&&y>=48&&y<512){
            if((game.state==ARK_READY||game.state==ARK_PLAY)&&!keys[75]&&!keys[77]){
                ark_move(&game,x/2);
                /* Absolute mouse input must not blend from a stale keyboard pose. */
                previous_pose.paddle=game.paddle;
                if(game.state==ARK_READY){previous_pose.x=game.x;previous_pose.y=game.y;}
            }
            if(button&&!left_button)ark_action(&game);
        }
        left_button=(uint8_t)button;
    }
    if(game.state!=state_before||game.level!=level_before)remember_pose();
}
static int32_t gfx(uint32_t op,uint32_t a,uint32_t b,uint32_t c)
{
    gui_message_t m;
    if(send((uint32_t)gfx_pid,op,a,b,c)){quit=1;return -1;}
    for(;;){
        if(receive(&m,1)){quit=1;return -1;}
        if(m.sender==(uint32_t)gfx_pid)return (int32_t)m.v[1];
        queue(&m);
    }
}
/* Three immutable request buffers allow nearby frame regions to travel
 * through SYSTEM.GFX without a round-trip wait after each one. */
static void finish(void)
{
#ifndef ARK_RENDER_TEST
    gui_message_t m;
    if(fast_frames){submit_frame();return;}
    while(in_flight){
        if(receive(&m,1)){quit=1;return;}
        if(m.sender==(uint32_t)gfx_pid&&m.v[0]==0x80000012u){
            --in_flight;if((int32_t)m.v[1]<0)quit=1;
        }else queue(&m);
    }
#else
    (void)in_flight;
#endif
}
static void flush(void)
{
    if(count){
#ifdef ARK_RENDER_TEST
        extern void render_test_batch(const gui_draw_t *,unsigned);
        render_test_batch(draws[draw_bank],count);
#else
        if(fast_frames){
            unsigned j;
            if(frame_count+count>GUI_FRAME_MAX||frame_group_count==GUI_FRAME_GROUPS)
                submit_frame(); /* bounded overflow, normally only initial scene */
            for(j=0;j<count;j++)frame_draws[frame_count+j]=draws[draw_bank][j];
            frame_groups[frame_group_count++]=(uint16_t)count;frame_count+=count;
            count=0;return;
        }
        if(send((uint32_t)gfx_pid,18,(uint32_t)(uintptr_t)draws[draw_bank],count,0)){
            quit=1;count=0;return;
        }
        ++in_flight;
#endif
        count=0;draw_bank=(draw_bank+1u)%3u;
        if(in_flight==3u)finish();
    }
}
static gui_draw_t *item(unsigned kind,int x,int y,int w,int h,unsigned color)
{
    gui_draw_t *d;if(count==GUI_DRAW_MAX)flush();
    d=&draws[draw_bank][count++];zero(d,sizeof(*d));d->kind=(uint16_t)kind;
    d->x=(int16_t)x;d->y=(int16_t)y;d->w=(uint16_t)w;d->h=(uint16_t)h;d->color=(uint16_t)color;
    return d;
}
static void fill(int x,int y,int w,int h,unsigned c)
{
    if(clipping){
        int r=x+w,b=y+h;
        if(x<clip.x)x=clip.x;
        if(y<clip.y)y=clip.y;
        if(r>clip.x+clip.w)r=clip.x+clip.w;
        if(b>clip.y+clip.h)b=clip.y+clip.h;
        w=r-x;h=b-y;
    }
    if(w>0&&h>0)(void)item(GUI_FILL,x,y,w,h,c);
}
static void text(int x,int y,const char *s,unsigned c,unsigned bg)
{gui_draw_t *d=item(GUI_TEXT,x,y,0,0,c);d->background=(uint16_t)bg;copytext(d->text,s,sizeof(d->text));}
static void append(char *d,const char *s)
{unsigned n=0;while(d[n]&&n<31)n++;copytext(d+n,s,32-n);}
static void brick(unsigned i)
{
    int x=16+(int)(i%13)*32,y=48+(int)(i/13)*16;
    unsigned t=game.type[i],c;if(!game.hp[i])return;
    c=brick_colour[t];
    fill(x+1,y+1,30,14,C_SHADOW);fill(x+1,y+1,29,12,c);
    fill(x+2,y+1,27,2,C_WHITE);fill(x+1,y+3,2,9,brick_light[t]);
    fill(x+3,y+11,27,2,brick_dark[t]);
    if(t==8||t==9){fill(x+14,y+4,3,5,C_WHITE);fill(x+12,y+6,7,1,C_WHITE);}
}
static int intersects(rect_t a,rect_t b)
{return a.x<b.x+b.w&&b.x<a.x+a.w&&a.y<b.y+b.h&&b.y<a.y+a.h;}
static rect_t united(rect_t a,rect_t b)
{
    int r=a.x+a.w>b.x+b.w?a.x+a.w:b.x+b.w;
    int d=a.y+a.h>b.y+b.h?a.y+a.h:b.y+b.h;
    rect_t c;c.x=a.x<b.x?a.x:b.x;c.y=a.y<b.y?a.y:b.y;
    c.w=r-c.x;c.h=d-c.y;return c;
}
static void dirty(rect_t r)
{
    unsigned i;
    /* Keep distant sprites separate: SYSTEM.GFX reports one bounding box
     * per batch. Joining ball, HUD and paddle repaints empty space. */
    for(i=0;i<damage_count;){
        rect_t u=united(r,damage[i]);
        if(intersects(r,damage[i])||u.w*u.h<=r.w*r.h+damage[i].w*damage[i].h+32){
            r=u;damage[i]=damage[--damage_count];i=0;
        }else ++i;
    }
    if(damage_count==32){
        /* Bounded fallback for mass changes (e.g. many skipped ticks). */
        r=(rect_t){16,48,416,464};damage_count=0;
    }
    damage[damage_count++]=r;
}
static void background(rect_t r)
{
    int row,col,c0=(r.x-16)/32,c1=(r.x+r.w-17)/32;
    int r0=(r.y-48)/16,r1=(r.y+r.h-49)/16;
    if(c0<0)c0=0;
    if(c1>12)c1=12;
    if(r0<0)r0=0;
    if(r1>=ARK_CELLS/13)r1=ARK_CELLS/13-1;
    fill(r.x,r.y,r.w,r.h,C_BG);
    /* Only visit grid cells intersecting the small damaged rectangle. */
    for(row=r0;row<=r1;row++)for(col=c0;col<=c1;col++)brick((unsigned)(row*13+col));
}
static void paddle(void)
{
    int h=ark_halfwidth(&game)*2,x=(int)((interpolated_draw?draw_pose.paddle:game.paddle)/(FP/2))-h,y=466;
    fill(x,y,h*2,14,C_RED);fill(x+8,y+1,h*2-16,12,C_SILVER);
    fill(x+9,y+1,h*2-18,3,C_WHITE);fill(x+10,y+10,h*2-20,3,C_BLUE);
    fill(x+2,y+2,4,8,C_PINK);fill(x+h*2-6,y+2,4,8,C_PINK);
}
static void ball(void)
{
    int x=(int)((interpolated_draw?draw_pose.x:game.x)/(FP/2))-4;
    int y=(int)((interpolated_draw?draw_pose.y:game.y)/(FP/2))-4;
    if(game.state==ARK_OVER||game.state==ARK_WON||y>504)return;
    fill(x+2,y,4,8,C_WHITE);fill(x,y+2,8,4,C_WHITE);fill(x+4,y+5,2,2,C_SILVER);
}
static void drop(void)
{
    static const char letters[4][2]={"E","S","C","L"};
    static const uint8_t colors[4]={C_GREEN,C_ORANGE,C_CYAN,C_PINK};
    int x=game.drop_x*2-16,y=game.drop_y*2-7;
    if(!game.drop_active||y>495)return;
    fill(x,y,32,15,colors[game.drop_type]);fill(x+2,y+1,28,2,C_WHITE);
    text(x+12,y,letters[game.drop_type],C_WHITE,colors[game.drop_type]);
}
static void hud(void)
{
    char s[32],v[16];
    fill(0,0,WIDTH,48,C_PANEL);text(12,3,"ARKANOID / ATOS",C_CYAN,C_PANEL);
    s[0]=0;append(s,"SCORE ");number(v,game.score);append(s,v);text(12,25,s,C_WHITE,C_PANEL);
    s[0]=0;append(s,"ROUND ");number(v,game.level+1);append(s,v);text(200,25,s,C_WHITE,C_PANEL);
    s[0]=0;append(s,"LIVES ");number(v,game.lives);append(s,v);text(340,25,s,C_WHITE,C_PANEL);
    text(280,3,label,C_WHITE,C_PANEL);
}
static void footer(void)
{
    fill(0,512,WIDTH,24,C_PANEL);
    text(8,516,"SPACE:GO P:PAUSE R:NEW D:DEMO",C_WHITE,C_PANEL);
    text(368,516,"ESC:END",C_WHITE,C_PANEL);
}
static void message(void)
{
    const char *s=game.state==ARK_READY?"SPACE OR CLICK TO SERVE":game.state==ARK_PAUSE?"PAUSED - P TO RESUME":game.state==ARK_OVER?"GAME OVER - SPACE TO RETRY":game.state==ARK_WON?"32 ROUNDS CLEAR! SPACE:NEW":0;
    if(s){fill(48,404,352,24,C_PANEL);text(56,408,s,C_WHITE,C_PANEL);}
}
static void paint(int full)
{
    unsigned i;
    int x=(interpolated_draw?draw_pose.x:game.x)/(FP/2);
    int y=(interpolated_draw?draw_pose.y:game.y)/(FP/2);
    int p=(interpolated_draw?draw_pose.paddle:game.paddle)/(FP/2),half=ark_halfwidth(&game)*2;
    int ball_visible=game.state!=ARK_OVER&&game.state!=ARK_WON&&y<=508;
    int was_ball=old_state!=ARK_OVER&&old_state!=ARK_WON&&old_y<=508;
    rect_t br={x-4,y-4,8,8},pr={p-half,466,half*2,14};
    rect_t dr={game.drop_x*2-16,game.drop_y*2-7,32,16};
    rect_t msg={48,404,352,24};
    damage_count=0;clipping=0;
    if(full||old_level!=game.level){
        fill(0,0,WIDTH,HEIGHT,C_BG);
        fill(0,48,16,464,C_BORDER);fill(432,48,16,464,C_BORDER);
        for(i=0;i<29;i++){
            fill(2,50+(int)i*16,12,2,C_SILVER);fill(434,50+(int)i*16,12,2,C_SILVER);
            fill(5,54+(int)i*16,5,8,C_PANEL);fill(437,54+(int)i*16,5,8,C_PANEL);
        }
        for(i=0;i<ARK_CELLS;i++)brick(i);
        hud();footer();paddle();ball();drop();message();flush();
    }else{
        if(x!=old_x||y!=old_y||ball_visible!=was_ball){
            if(was_ball)dirty((rect_t){old_x-4,old_y-4,8,8});
            if(ball_visible)dirty(br);
        }
        if(p!=old_p||half!=old_half){
            dirty((rect_t){old_p-old_half,466,old_half*2,14});dirty(pr);
        }
        if(old_drop||game.drop_active){
            if(old_drop)dirty((rect_t){old_drop_x*2-16,old_drop_y*2-7,32,16});
            if(game.drop_active)dirty(dr);
        }
        for(i=0;i<ARK_CELLS;i++)if(old_hp[i]!=game.hp[i])
            dirty((rect_t){16+(int)(i%13)*32,48+(int)(i/13)*16,32,16});
        if(old_state!=game.state)dirty(msg);
        /* Each batch is a complete local repaint. No intermediate erase-only
         * frame can be exposed while waiting for another service reply. */
        for(i=0;i<damage_count;i++){
            rect_t r=damage[i];int right=r.x+r.w,bottom=r.y+r.h;
            if(r.x<16)r.x=16;
            if(r.y<48)r.y=48;
            if(right>432)right=432;
            if(bottom>512)bottom=512;
            r.w=right-r.x;r.h=bottom-r.y;
            if(r.w<=0||r.h<=0)continue;
            clip=r;clipping=1;background(r);
            if(intersects(r,pr))paddle();
            if(ball_visible&&intersects(r,br))ball();
            if(game.drop_active&&intersects(r,dr))drop();
            /* Text has no per-command clip. Its small, complete extent can
             * widen this batch, never the ball-to-paddle empty rectangle. */
            if(game.state!=ARK_PLAY&&intersects(r,msg))message();
            flush();clipping=0;
        }
        /* Counters are separate damage groups, kept inside the client edges.
           Repainting the whole HUD also damaged the window chrome. */
        if(old_score!=game.score){
            char s[32],v[16];s[0]=0;append(s,"SCORE ");number(v,game.score);append(s,v);
            fill(12,25,176,16,C_PANEL);text(12,25,s,C_WHITE,C_PANEL);flush();
        }
        if(old_lives!=game.lives){
            char s[32],v[16];s[0]=0;append(s,"LIVES ");number(v,game.lives);append(s,v);
            fill(340,25,88,16,C_PANEL);text(340,25,s,C_WHITE,C_PANEL);flush();
        }
        if(hud_pending){fill(280,3,136,16,C_PANEL);text(280,3,label,C_WHITE,C_PANEL);hud_pending=0;flush();}
    }
    finish(); /* do not reuse buffers, or apply input, before all replies */
    for(i=0;i<ARK_CELLS;i++)old_hp[i]=game.hp[i];
    old_level=game.level;old_x=x;old_y=y;old_p=p;old_half=half;
    old_drop=game.drop_active;old_drop_x=game.drop_x;old_drop_y=game.drop_y;
    old_state=game.state;old_score=game.score;old_lives=game.lives;old_power=game.power;
}
static void paint_interpolated(int full,uint32_t phase)
{
    if(phase>12288u)phase=12288u;
    if(game.state!=ARK_READY&&game.state!=ARK_PLAY)phase=12288u;
    draw_pose.x=interpolate(previous_pose.x,game.x,phase);
    draw_pose.y=interpolate(previous_pose.y,game.y,phase);
    draw_pose.paddle=interpolate(previous_pose.paddle,game.paddle,phase);
    interpolated_draw=1;paint(full);interpolated_draw=0;
}
static void profile(uint32_t now)
{
    ++frames;
    if(!fps_since){fps_since=now;fps_frames=frames;}
    if(now-fps_since>=12288u){
        char num[16];uint32_t rate=(frames-fps_frames)*12288u/(now-fps_since);
        label[0]=0;number(num,rate);append(label,num);append(label," FPS");
        fps_since=now;fps_frames=frames;hud_pending=1;
    }
}

int app_main(void)
{
    uint32_t last,acc=0,last_frame;
    ark_new(&game);remember_pose();gfx_pid=lookup("SYSTEM.GFX");if(gfx_pid<0)return 1;
    if(gfx(35,0,0,0)!=8)return 2;
    if(gfx(20,((uint32_t)(uint16_t)wx<<16)|(uint16_t)wy,
           ((uint32_t)WIDTH<<16)|(HEIGHT+16),0)<0)return 3;
    (void)gfx(21,(uint32_t)(uintptr_t)"ARKANOID - ATOS DEMO",0,0);
    (void)gfx(29,HEIGHT,0,0);
    if(atos_app_abi_version()>=0x00010004u){
        int32_t p=gui_call(16,GUI_FRAME_MAX*sizeof(gui_draw_t),1u,0,0);
        if(p>0){frame_draws=(gui_draw_t *)(uintptr_t)p;fast_frames=1;}
    }
    paint(1);
    /* A window exists only after its first draw, so activate it afterwards. */
    (void)gfx(32,0,0,(uint32_t)gui_call(18,0,0,0,0));
    last=last_frame=hires();
    while(!quit){
        gui_message_t m;unsigned n,steps=0;uint32_t now,elapsed;
        for(n=0;n<64;n++){
            if(pending_head!=pending_tail)copy_message(&m,&pending[pending_head++&63u]);
            else if(receive(&m,0))break;
            event(&m);
        }
        if(quit)break;
        /* Use the non-scheduling clock. ticks() is also a scheduler yield,
         * so it is not a cheap or independent animation time source. */
        now=hires();elapsed=now-last;last=now;
        if(elapsed>2457u)elapsed=2457u;
        acc+=elapsed*50u;
        while(acc>=12288u&&steps++<8u){
            int direction=(keys[77]!=0)-(keys[75]!=0);
            unsigned before_state=game.state,before_level=game.level;
            remember_pose();
            if(demo){if(game.state==ARK_READY)ark_action(&game);ark_move(&game,game.x/FP);}ark_step(&game,direction);++simsteps;acc-=12288u;
            if(game.state!=before_state||game.level!=before_level)remember_pose();
        }
        if(acc>=12288u)acc=0;
        sound_flush();     /* events from the step above, never from paint() */
        if((now-last_frame>=307u&&(game.state==ARK_PLAY||old_p!=game.paddle/(FP/2)))||
           (repaint&&now-last_frame>=307u)||old_state!=game.state||old_level!=game.level){
            paint_interpolated(repaint,acc);profile(hires());repaint=hidden_frame;
            /* Keep the deadline phase; late frames skip slots, not the next
               deadline. Resetting to now lost time at every scheduler delay. */
            if(now-last_frame>=307u)last_frame+=((now-last_frame)/307u)*307u;
            else last_frame=now;
            /* Drawing runs in supervisor mode, where timer IRQs cannot switch
               this task. Busy clock/RECV traps between frames can starve the
               input pump after serving. Route input once per completed frame,
               without the 20 ms delay imposed by pause_tick(). */
            gui_call(ATOS_CALL_SLEEP,0u,0u,0,0);
        }else if(game.state!=ARK_PLAY)pause_tick();
    }
    return 0;
}
