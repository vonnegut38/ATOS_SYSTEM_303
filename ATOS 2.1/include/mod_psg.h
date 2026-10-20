/* MOD -> PSG conversion (pure module: no ATOS, no kernel headers, so it can be
 * unit tested on the host).
 *
 * Scope, stated honestly: a YM2149 has three tone channels and one noise
 * generator.  It cannot play four sampled voices.  This module therefore does
 * what trackers did on the ST: it reads the four MOD voices, converts the note
 * periods to YM2149 tone periods and STEALS a channel when more than three
 * voices sound at once.  The result is a chiptune rendition of the module with
 * the correct notes, volumes and timing - not PCM.
 */
#ifndef MOD_PSG_H
#define MOD_PSG_H

#include <stdint.h>

#define MOD_VOICES   4u
#define MOD_PSG_CH   3u

/* One pattern cell, already decoded from the 4 MOD bytes. */
typedef struct {
    uint16_t period;      /* 0 = no new note on this row          */
    uint8_t  instrument;  /* 0 = no instrument                    */
    uint8_t  effect;      /* 0 = none                             */
    uint8_t  param;
} mod_cell_t;

/* Running state of one MOD voice (kept by the player between rows). */
typedef struct {
    uint16_t period;      /* current Amiga period, 0 = silent     */
    uint8_t  volume;      /* 0..64, MOD scale                     */
    uint8_t  psg;         /* PSG channel it currently owns, 0xff = none */
} mod_voice_t;

/* What the player must apply to the audio service for one row. */
typedef struct {
    uint8_t  channel;     /* PSG channel 0..2                     */
    uint8_t  volume;      /* 0..15, PSG scale                     */
    uint16_t period;      /* YM2149 tone period                   */
} mod_psg_note_t;

/* Amiga period for a MOD note number (0 = C-1).  0 when out of range. */
uint16_t mod_note_period(uint8_t note);

/* Amiga period -> YM2149 tone period.  Uses the ST's 2 MHz PSG clock and the
 * Amiga's 3.546895 MHz PAL clock.  0 when the result cannot be represented. */
uint16_t mod_psg_period(uint16_t amiga_period);

/* Apply one row of cells to the voices and produce the PSG notes to play.
 * Returns how many notes were written to out[0..max).  Silenced voices are
 * reported as volume 0 so the player can stop them. */
unsigned mod_psg_row(const mod_cell_t cell[MOD_VOICES], mod_voice_t voice[MOD_VOICES],
                     mod_psg_note_t *out, unsigned max);

/* Volume slide (effect A) and set-volume (effect C) handling, exposed so the
 * player and the tests agree on the semantics. */
uint8_t mod_volume_apply(uint8_t volume, uint8_t effect, uint8_t param);

#endif
