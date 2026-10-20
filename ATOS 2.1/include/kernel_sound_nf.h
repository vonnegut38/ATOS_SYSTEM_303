/* ATOS Sound 1.0 — usluga jadra dla transportu ASND (NF_ATOS_SOUND). */
#ifndef ATOS_KERNEL_SOUND_NF_H
#define ATOS_KERNEL_SOUND_NF_H
#include <stdint.h>
int32_t ksnd_claim(uint32_t pid);       /* wlasciciel URZADZENIA PCM (osobny od PSG) */
void ksnd_release(uint32_t pid);
uint32_t ksnd_owner_pid_get(void);
int32_t ksnd_exchange(uint32_t pid,const void *req,uint32_t req_len,void *reply,uint32_t reply_cap);
void ksnd_info(uint32_t *out);
uint32_t ksnd_available(void);
void ksnd_owner_exit(uint32_t pid);
#endif
