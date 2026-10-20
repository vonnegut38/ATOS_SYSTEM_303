#ifndef MODPLAY_SOUND_H
#define MODPLAY_SOUND_H
#include <stdint.h>
int modsound_available(void);
int modsound_open(uint32_t *budget);
int modsound_begin(uint32_t frames, uint32_t rate);
int modsound_write(const uint8_t *u8, unsigned frames);
int modsound_commit(void);
int modsound_play(void);
int modsound_stop(void);
int modsound_close(void);
uint32_t modsound_seconds(void);
/* Tracker extension: byte upload, pause/resume, compact live telemetry. */
int modsound_module_begin(uint32_t bytes);
int modsound_module_data(const uint8_t *,unsigned);
int modsound_module_commit(void);
int modsound_module_control(unsigned action);
int modsound_module_status(uint8_t out[208]);

int modsound_module_can_seek(void);
int modsound_module_seek(uint32_t ms);
#endif
