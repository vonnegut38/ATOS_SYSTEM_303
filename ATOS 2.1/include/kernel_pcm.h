#ifndef KERNEL_PCM_H
#define KERNEL_PCM_H
#include <stdint.h>
#define KPCM_RATE 12288u
#define KPCM_CAPACITY 8192u
#define KPCM_MAX_SUBMIT 512u
/* Unsigned 8-bit mono; fixed hardware rate. No guest pointers retained. */
int32_t kpcm_control(uint32_t pid,unsigned session,unsigned command);
int32_t kpcm_submit(uint32_t pid,const uint8_t *data,uint32_t count);
void kpcm_state(uint32_t *out);
void kpcm_release(uint32_t pid);
int kpcm_owned(void);
int32_t kpcm_clip_create(uint32_t pid,uint32_t bytes);
extern volatile uint32_t kpcm_owner,kpcm_active,kpcm_read,kpcm_write;
extern volatile uint32_t kpcm_played,kpcm_underruns;
extern uint32_t *kpcm_ring;   /* TT-RAM, alokowany przy otwarciu */
#endif
