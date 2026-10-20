#ifndef ARK_GAME_H
#define ARK_GAME_H
#include <stdint.h>
#include "levels.h"
#define FP 256
enum {ARK_READY,ARK_PLAY,ARK_PAUSE,ARK_OVER,ARK_WON};
typedef struct {
    int32_t x,y,vx,vy,paddle;
    uint32_t score,seed,steps;
    uint16_t remaining;
    uint8_t level,lives,state,power;
    uint8_t type[ARK_CELLS],hp[ARK_CELLS];
    int16_t drop_x,drop_y;
    uint8_t drop_type,drop_active;
} ark_game;
void ark_new(ark_game *g);
void ark_level(ark_game *g,unsigned level);
void ark_action(ark_game *g);
void ark_pause(ark_game *g);
void ark_move(ark_game *g,int x);
void ark_step(ark_game *g,int direction);
int ark_halfwidth(const ark_game *g);

/* Sound events are raised by the LOGIC ONLY - never by the redraw path, because
   repainting a window is not another collision.  The application drains them
   with ark_sound_poll() and forwards them to the audio service in one batch. */
enum {
    ARK_SND_NONE=0,
    ARK_SND_BRICK_HIT,      /* brick took damage but survived */
    ARK_SND_BRICK_BREAK,    /* brick destroyed */
    ARK_SND_PADDLE,         /* ball bounced off the paddle */
    ARK_SND_WALL,           /* ball bounced off a wall or the ceiling */
    ARK_SND_BONUS_SPAWN,    /* a capsule appeared */
    ARK_SND_BONUS_TAKE,     /* a capsule was caught */
    ARK_SND_LIFE_LOST,      /* ball lost, lives left */
    ARK_SND_LEVEL_UP,       /* level finished */
    ARK_SND_GAME_OVER       /* no lives left */
};
void ark_sound_push(unsigned kind);
/* Returns the next pending event, or ARK_SND_NONE when the queue is empty. */
unsigned ark_sound_poll(void);
void ark_sound_reset(void);
#endif
