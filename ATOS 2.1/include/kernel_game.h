#ifndef KERNEL_GAME_H
#define KERNEL_GAME_H
#include <stdint.h>
#include "atos_game.h"
/* Exclusive full-screen game mode (GFX opcode 38). */
int kgame_active(void);
uint32_t kgame_owner(void);
/* True while the game's session is on screen: compositor, task bar and
   pointer must stay off the screen. */
int kgame_owns_screen(void);
int32_t kgame_begin(uint32_t pid,unsigned session,uint16_t width,uint16_t height);
int32_t kgame_end(uint32_t pid);
void kgame_release_pid(uint32_t pid);
/* f: kernel copy of the validated header+rectangles; pixels: validated. */
int32_t kgame_frame(uint32_t pid,const atos_game_frame_t *f,const uint8_t *pixels);
int32_t kgame_palette(uint32_t pid,const atos_game_palette_t *p);
#endif
