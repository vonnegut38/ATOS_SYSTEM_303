/* MOD player state machine - pure module (no ATOS, no kernel), so the timing and
 * effect handling can be unit tested on the host.
 *
 * Design: the player never owns a timer.  mod_play_tick() produces the PSG notes
 * for one player tick and RETURNS how many hires ticks (12288/s) must pass
 * before the next tick. The windowed application advances a monotonic deadline
 * and sends notes through the native audio service when due. No timer vectors
 * or hardware registers are owned by the application.
 */
#ifndef MOD_PLAY_H
#define MOD_PLAY_H

#include <stdint.h>
#include "mod_psg.h"

#define MOD_MAX_PATTERNS 64u
#define MOD_HIRES_PER_SEC 12288u
#define MOD_END 0xffffffffu

typedef struct {
    const uint8_t *data;
    uint32_t size;
    /* header */
    uint8_t  song_len;
    uint8_t  restart;
    uint8_t  pattern_count;
    uint8_t  sample_volume[31];
    uint32_t sample_data;          /* offset of the sample data, for the player  */
    /* position */
    uint8_t  order;
    uint8_t  row;
    uint8_t  tick;
    uint16_t speed;                /* ticks per row            */
    uint16_t tempo;                /* BPM                      */
    uint32_t frac;                 /* tick length accumulator  */
    uint8_t  finished;
    uint8_t  jump_order;           /* pending Bxx/Dxx          */
    uint8_t  jump_row;
    uint8_t  jump;
    mod_voice_t voice[MOD_VOICES];
} mod_play_t;

/* Parse the header and start at the first order.  Returns 0 on success, <0 when
 * the image is too small or the signature is not a known 4-channel MOD. */
int mod_play_init(mod_play_t *p,const uint8_t *data,uint32_t size);

/* One player tick.  Writes up to `max` PSG notes, stores their count in *count
 * and returns the hires ticks until the next tick (MOD_END when the song is
 * over).  A zero-volume note means "silence this channel". */
uint32_t mod_play_tick(mod_play_t *p,mod_psg_note_t *out,unsigned max,unsigned *count);

#endif
