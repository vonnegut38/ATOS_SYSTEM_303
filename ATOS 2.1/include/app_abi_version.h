#ifndef ATOS_APP_ABI_VERSION_H
#define ATOS_APP_ABI_VERSION_H
#include <stdint.h>

/* ATOS native application ABI version — single source of truth.
   1.0 frozen 2026-09-26.  1.1 adds only the HIRES_CLOCK opcode (67); the 1.0
   surface is unchanged.  1.2 adds the exclusive game mode (GFX opcode 38,
   include/atos_game.h), joystick virtual scan codes 0x78..0x7d and a 16 MB
   fast-RAM budget for ALLOC with prefer_tt=1.  1.3 adds the PSG audio opcodes
   75..80 (ATOS_CALL_AUDIO_*); the capability word is the contract, and this
   build advertises tones, noise and the effect queue only - no PCM, no DMA.
   1.4 adds GUI_FRAME (81): bounded synchronous window frames with disjoint
   damage groups; no broker round trip.
   1.5 adds PCM_CONTROL/SUBMIT/STATE (82..85, including pre-rendered clips), U8 mono through YM2149.
   See docs/APP-ABI.md.  Kept dependency-free so both the
   kernel and host tests can include it.  Bump MAJOR for any layout/meaning
   change, MINOR for additive changes (new opcode / appended field). */
#define APP_ABI_VERSION_MAJOR 1u
#define APP_ABI_VERSION_MINOR 5u
#define APP_ABI_VERSION (((uint32_t)APP_ABI_VERSION_MAJOR << 16) | APP_ABI_VERSION_MINOR)
/* Syscall number that returns APP_ABI_VERSION. */
#define APP_ABI_SYSCALL_VERSION 66u
#endif
