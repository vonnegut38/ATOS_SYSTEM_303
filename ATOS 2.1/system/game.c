/* ATOS demonstration adaptation of dawid-wolinski/arkanoid-game (MIT).
 * Level layouts, brick values/durability, paddle segments and basic rules
 * derive from that project. SFML/float rendering replaced by fixed-point C.
 * Physics: 50 Hz, 8 fractional bits, two collision substeps per tick.
 */
#include "game.h"
static int clamp(int v,int lo,int hi){return v<lo?lo:v>hi?hi:v;}
static int absolute(int v){return v<0?-v:v;}
static unsigned random_value(ark_game *g)
{g->seed=g->seed*1664525u+1013904223u;return g->seed>>16;}

/* --------------------------------------------------------------- sound queue
   A short ring filled by the game logic and drained by the application.  The
   redraw path never calls ark_sound_push(), so repainting cannot repeat an
   effect.  Fixed size, no allocation. */
#define ARK_SND_QUEUE 16u
static uint8_t snd_queue[ARK_SND_QUEUE];
static uint8_t snd_head,snd_count;
static uint32_t snd_dropped;

void ark_sound_reset(void){snd_head=0u;snd_count=0u;snd_dropped=0u;}
void ark_sound_push(unsigned kind)
{
    if(kind==ARK_SND_NONE)return;
    if(snd_count>=ARK_SND_QUEUE){++snd_dropped;return;}   /* keep the game running */
    snd_queue[(uint8_t)((snd_head+snd_count)%ARK_SND_QUEUE)]=(uint8_t)kind;
    ++snd_count;
}
unsigned ark_sound_poll(void)
{
    unsigned kind;
    if(!snd_count)return ARK_SND_NONE;
    kind=snd_queue[snd_head];
    snd_head=(uint8_t)((snd_head+1u)%ARK_SND_QUEUE);
    --snd_count;
    return kind;
}
uint32_t ark_sound_dropped(void){return snd_dropped;}
int ark_halfwidth(const ark_game *g){return g->power==1?24:16;}
void ark_move(ark_game *g,int x)
{
    g->paddle=clamp(x,8+ark_halfwidth(g),216-ark_halfwidth(g))*FP;
    if(g->state==ARK_READY){g->x=g->paddle;g->y=231*FP;}
}
static void ready(ark_game *g)
{
    g->power=0;g->drop_active=0;g->state=ARK_READY;
    g->vx=320;g->vy=-704;ark_move(g,112);
}
void ark_level(ark_game *g,unsigned level)
{
    unsigned i;
    if(level>=ARK_LEVELS){g->state=ARK_WON;return;}
    g->level=(uint8_t)level;g->remaining=0;
    for(i=0;i<ARK_CELLS;i++){
        unsigned t=ark_layouts[level][i];g->type[i]=(uint8_t)t;
        g->hp[i]=(uint8_t)(t==255?0:t==9?255:t==8?2+(level+1)/8:1);
        if(g->hp[i]&&t!=9)++g->remaining;
    }
    ready(g);
}
void ark_new(ark_game *g)
{
    unsigned i;uint8_t *p=(uint8_t *)g;
    for(i=0;i<sizeof(*g);i++)p[i]=0;
    g->seed=0x41544f53u;g->lives=3;ark_level(g,0);
    /* A restarted game must not replay effects queued before it. */
    ark_sound_reset();
}
void ark_action(ark_game *g)
{
    if(g->state==ARK_READY)g->state=ARK_PLAY;
    else if(g->state==ARK_OVER||g->state==ARK_WON)ark_new(g);
}
void ark_pause(ark_game *g)
{if(g->state==ARK_PLAY)g->state=ARK_PAUSE;else if(g->state==ARK_PAUSE)g->state=ARK_PLAY;}
static void hit(ark_game *g,unsigned i)
{
    unsigned t=g->type[i];
    if(t==9)return;
    if(--g->hp[i]){ark_sound_push(ARK_SND_BRICK_HIT);return;}
    --g->remaining;g->score+=(t==8?50u*(g->level+1u):50u+10u*t);
    ark_sound_push(ARK_SND_BRICK_BREAK);
    if(!g->drop_active&&random_value(g)%7u==0u){
        g->drop_x=(int16_t)(16+(i%13)*16);g->drop_y=(int16_t)(28+(i/13)*8);
        g->drop_type=(uint8_t)(random_value(g)%4u);g->drop_active=1;
        ark_sound_push(ARK_SND_BONUS_SPAWN);
    }
}
static void bounce_paddle(ark_game *g)
{
    int offset=(g->x-g->paddle)/FP,half=ark_halfwidth(g);
    int magnitude=absolute(offset)*16/half;
    /* Original 30/45/65-degree paddle zones, precomputed without libm. */
    int vx=magnitude>=11?665:magnitude>=6?543:325;
    int vy=magnitude>=11?384:magnitude>=6?543:696;
    if(offset<0)vx=-vx;else if(!offset&&g->vx<0)vx=-vx;
    if(g->power==2){vx=vx*3/4;vy=vy*3/4;}
    g->vx=vx;g->vy=-vy;
    ark_sound_push(ARK_SND_PADDLE);            /* the bounce, not the redraw */
    if(g->power==3){g->state=ARK_READY;g->x=g->paddle;}
}
/* Candidate cells in row-major order, matching the original first-hit rule.
 * ark_step has already clamped x >= 10 and y >= 26. Exclusive upper edges
 * use one fixed-point unit less, preserving strict overlap at tile borders. */
static unsigned brick_at_ball(const ark_game *g)
{
    int c0=(g->x-10*FP)/(16*FP),c1=(g->x-6*FP-1)/(16*FP);
    int r0=(g->y-26*FP)/(8*FP),r1=(g->y-22*FP-1)/(8*FP);
    int r,c;
    if(c1>12)c1=12;
    if(r1>=ARK_CELLS/13)r1=ARK_CELLS/13-1;
    for(r=r0;r<=r1;++r)for(c=c0;c<=c1;++c){
        unsigned i=(unsigned)(r*13+c);if(g->hp[i])return i;
    }
    return ARK_CELLS;
}
void ark_step(ark_game *g,int direction)
{
    unsigned sub,i;
    if(g->state!=ARK_READY&&g->state!=ARK_PLAY)return;
    if(direction)ark_move(g,g->paddle/FP+direction*6);
    if(g->state==ARK_READY)return;
    ++g->steps;
    for(sub=0;sub<2;sub++){
        int32_t ox=g->x,oy=g->y;
        g->x+=g->vx/2;g->y+=g->vy/2;
        if(g->x<10*FP){g->x=10*FP;g->vx=absolute(g->vx);ark_sound_push(ARK_SND_WALL);}
        if(g->x>214*FP){g->x=214*FP;g->vx=-absolute(g->vx);ark_sound_push(ARK_SND_WALL);}
        if(g->y<26*FP){g->y=26*FP;g->vy=absolute(g->vy);ark_sound_push(ARK_SND_WALL);}
        if(g->vy>0&&oy<=231*FP&&g->y>=231*FP&&
           absolute(g->x-g->paddle)<=(ark_halfwidth(g)+2)*FP){
            g->y=231*FP;bounce_paddle(g);
            if(g->state==ARK_READY)break;
        }
        i=brick_at_ball(g);if(i<ARK_CELLS){
            int l=(8+(int)(i%13)*16)*FP,r=l+16*FP;
            int t=(24+(int)(i/13)*8)*FP,b=t+8*FP;
            if(g->x+2*FP<=l||g->x-2*FP>=r||g->y+2*FP<=t||g->y-2*FP>=b)continue;
            /* Resolve against the previous position so adjacent bricks do not
             * reverse the same axis twice in a single substep. */
            if(oy+2*FP<=t&&g->vy>0){g->y=t-2*FP;g->vy=-absolute(g->vy);}
            else if(oy-2*FP>=b&&g->vy<0){g->y=b+2*FP;g->vy=absolute(g->vy);}
            else if(ox+2*FP<=l&&g->vx>0){g->x=l-2*FP;g->vx=-absolute(g->vx);}
            else if(ox-2*FP>=r&&g->vx<0){g->x=r+2*FP;g->vx=absolute(g->vx);}
            else {g->x=ox;g->y=oy;g->vy=-g->vy;}
            hit(g,i);
        }
        if(!g->remaining){ark_sound_push(ARK_SND_LEVEL_UP);ark_level(g,g->level+1u);return;}
        if(g->y>258*FP){
            if(--g->lives){ark_sound_push(ARK_SND_LIFE_LOST);ready(g);}
            else {ark_sound_push(ARK_SND_GAME_OVER);g->state=ARK_OVER;}
            return;
        }
    }
    if(g->drop_active){
        ++g->drop_y;
        if(g->drop_y>=229&&g->drop_y<=243&&absolute(g->drop_x-g->paddle/FP)<ark_halfwidth(g)+8){
            unsigned t=g->drop_type;g->score+=1000;g->drop_active=0;
            ark_sound_push(ARK_SND_BONUS_TAKE);
            if(t==3){if(g->lives<9)++g->lives;}
            else {g->power=(uint8_t)(t+1);ark_move(g,g->paddle/FP);
                if(t==1){g->vx=g->vx*3/4;g->vy=g->vy*3/4;}}
        }else if(g->drop_y>260)g->drop_active=0;
    }
}
