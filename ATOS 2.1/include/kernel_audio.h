/* ATOS audio core: one sound owner, PSG tones and kernel-owned PCM stream.
 *
 * Scope: YM2149 tones, noise and software PCM using Timer A. DMA
 * devices may plug in later through the same core, which is why
 * every entry point here already reports capabilities and constraints instead
 * of promising hardware we do not have (no STE/TT DMA, no ISA, no USB yet).
 *
 * Rules this core enforces:
 *   - one owner (a pid) at a time; a second claimant is refused, not queued,
 *   - requests are applied immediately (a sound is not tied to a rendered
 *     frame), and queued effects are drained by the kernel loop, not by the
 *     application's drawing code,
 *   - the single-owner rule is deliberately simple: three PSG channels are NOT
 *     shared between applications,
 *   - nothing here allocates, opens files, or calls back into an application,
 *     so it is safe to run from the kernel loop,
 *   - releasing the owner (normal exit, error, kill) silences every channel and
 *     restores the mixer enables.
 */
#ifndef ATOS_KERNEL_AUDIO_H
#define ATOS_KERNEL_AUDIO_H

#include <stdint.h>

/* PCM capability is implemented by the kernel-owned Timer A/YM2149 stream. */
#define KAUDIO_CAP_PSG_TONE  (1u<<0)
#define KAUDIO_CAP_PSG_NOISE (1u<<1)
#define KAUDIO_CAP_ENVELOPE  (1u<<2)
#define KAUDIO_CAP_QUEUE     (1u<<3)
#define KAUDIO_CAP_PCM       (1u<<4)

/* Channels: the PSG has three. */
#define KAUDIO_CHANNELS 3u
/* Short queue of scheduled effects. */
#define KAUDIO_QUEUE_EVENTS 32u
/* A stalled system must not replay a whole sequence at once (~0.5 s). */
#define KAUDIO_CATCHUP_LIMIT 6144u
/* Noise effects use channel C (it has its own noise-enable bit). */
#define KAUDIO_NOISE_CHANNEL 2u

/* Queued effect opcodes. */
#define KAUDIO_OP_NONE  0u
#define KAUDIO_OP_TONE  1u   /* a=channel b=volume, aux=period        */
#define KAUDIO_OP_NOISE 2u   /* b=volume, aux=noise period            */
#define KAUDIO_OP_STOP  3u   /* a=channel bit mask                    */

typedef struct {
    uint32_t wait;   /* hires ticks (~81 us) to wait before applying */
    uint16_t aux;    /* tone/noise period */
    uint8_t op;
    uint8_t a;
    uint8_t b;
    uint8_t pad[3];  /* explicit: the ABI mirror must be exactly 12 bytes */
} kaudio_event_t;

void kaudio_init(void);
/* Called from the kernel main loop: applies due queued effects.  Does only PSG
   register writes - no allocation, no file access, no application code. */
void kaudio_pump(void);

uint32_t kaudio_caps(void);
/* Claim the single sound owner.  Returns 0 when this pid owns audio, or a
   negative ATOS error when another pid already does. */
int32_t kaudio_claim(uint32_t pid,unsigned session);
/* Called on any process exit: silences the channels when pid was the owner. */
void kaudio_release(uint32_t pid);

int32_t kaudio_tone(uint32_t pid,unsigned channel,uint16_t period,uint8_t volume);
int32_t kaudio_noise(uint32_t pid,uint16_t period,uint8_t volume);
int32_t kaudio_stop(uint32_t pid,uint8_t channel_mask);
int32_t kaudio_queue(uint32_t pid,const kaudio_event_t *events,uint32_t count);

/* ---- TOS adapter (XBIOS Giaccess/Dosound/Offgibit/Ongibit) ----
   The owner id used when a TOS program, not an ATOS application, drives the
   chip.  Dosound claims it; if an application already owns audio, Dosound
   refuses instead of stealing the channels. */
/* Reserved domain id: no native pid can collide with it (pids are small). */
#define KAUDIO_TOS_OWNER 0xFFFFFFF0u

/* Hook the effect queue and the TOS sound daemon into Timer C (~50 Hz).  This
   is what makes a Dosound sequence progress while a TOS program computes in a
   tight loop and never calls the OS.  Idempotent. */
int kaudio_irq_attach(void);
int kaudio_irq_ok(void);
/* Kernel loop hook: drains only when the Timer C consumer is not installed. */
void kaudio_pump_fallback(void);
/* Timer C side: step the TOS sequence and apply due queued effects.  Only PSG
   register writes happen here - no allocation, no file access, no app code. */
void kaudio_timer_tick(void);

/* TOS Dosound contract: returns the PREVIOUS table pointer; a negative pointer
   stops playback; a delay byte of zero in the table ends it. */
const uint8_t *kaudio_dosound(const uint8_t *table);
uint32_t kaudio_dosound_state(void);   /* 1 = a sequence is playing */
uint32_t kaudio_dosound_delay(void);

/* TOS port-bit contracts (EmuTOS semantics): ongibit ORs the mask into PSG port
   A, offgibit ANDs the mask.  Both keep the direction bits alone because they
   go through psg_port_a_modify(). */
void kaudio_ongibit(uint8_t mask);
void kaudio_offgibit(uint8_t mask);

/* Debugger/emulator-visible state (the accessors are functions, so a symbol
   lookup by name would land on code). */
/* Called by the TOS launcher BEFORE the program's memory is released: detaches
   the Dosound table (an interrupt must never read a freed TPA), drops anything
   still queued and releases the TOS domain owner.  Atomic with respect to the
   Timer C consumer.  Returns the generation that was torn down. */
uint32_t kaudio_tos_teardown(void);
/* Generation counter of TOS audio sessions (diagnostics: a stale owner from a
   previous run can be told apart from the current one). */
uint32_t kaudio_tos_generation(void);
/* Policy for the legacy TOS audio entries: a TOS window may touch the sound
   registers only while no native application owns the chip.  Kept in the core
   (not in the persona) so it can be tested without an emulator. */
int kaudio_tos_may_write(void);
/* Atomic "check the owner rule and write" for the legacy TOS entries: the check
   and the register write must be one operation, or a native application can
   claim the chip in between. */
int32_t kaudio_tos_reg_write(uint8_t reg,uint8_t value);
/* Dosound with an explicit valid range: the interpreter runs in an interrupt and
   must never read past the bytes the caller proved valid. */
const uint8_t *kaudio_dosound_span(const uint8_t *table,const uint8_t *end);
/* Called before a buffer is freed or shrunk: stop the interpreter or clamp its
   valid end, so the Timer C consumer never reads released or reused memory. */
void kaudio_dosound_revalidate(const uint8_t *old_base,const uint8_t *old_end,
                               const uint8_t *new_end);
/* Persona hook for PSG ports 14/15 (virtualised in a TOS window). */
void kaudio_set_port_sink(int (*fn)(uint8_t reg,uint8_t value));
extern volatile uint32_t kaudio_dbg_tos_teardown,kaudio_dbg_isr_max;
extern volatile uint32_t kaudio_dbg_tos_teardown;

extern volatile uint32_t kaudio_state_owner,kaudio_state_events,kaudio_state_queued,
                         kaudio_state_rejected,kaudio_state_qcount;
extern volatile uint32_t kaudio_dbg_dosound_starts,kaudio_dbg_dosound_ends,
                         kaudio_dbg_dosound_rejected,kaudio_dbg_ticks,kaudio_dbg_isr_max;

uint32_t kaudio_owner(void);
uint32_t kaudio_dbg_events(void);
uint32_t kaudio_dbg_rejected(void);
uint32_t kaudio_dbg_queued(void);
uint32_t kaudio_dbg_underruns(void);

#endif
