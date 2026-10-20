/* PSG (YM2149) access.  See hw/psg.c for why every access is atomic. */
#ifndef ATOS_PSG_H
#define ATOS_PSG_H

#include <stdint.h>

/* Audio-usable registers 0..13. */
#define PSG_REG_TONE_A_LO  0u
#define PSG_REG_TONE_A_HI  1u
#define PSG_REG_TONE_B_LO  2u
#define PSG_REG_TONE_B_HI  3u
#define PSG_REG_TONE_C_LO  4u
#define PSG_REG_TONE_C_HI  5u
#define PSG_REG_NOISE      6u
#define PSG_REG_MIXER      7u   /* port A/B directions - audio must NOT write it */
#define PSG_REG_VOL_A      8u
#define PSG_REG_VOL_B      9u
#define PSG_REG_VOL_C     10u
#define PSG_REG_ENV_LO    11u
#define PSG_REG_ENV_HI    12u
#define PSG_REG_ENV_SHAPE 13u
#define PSG_REG_IO_A      14u   /* port A: floppy drive/side select */
#define PSG_REG_IO_B      15u   /* port B */

/* Register access with interrupts masked across select+data. */
uint8_t psg_read(uint8_t reg);
/* Refuses register 7, 14 and 15 (counts them in psg_dbg_rejected). */
void psg_write(uint8_t reg,uint8_t value);
/* Atomic read-modify-write of port A; used by the floppy drive/side select. */
void psg_port_a_modify(uint8_t clear_mask,uint8_t set_bits);
/* Atomic update of the mixer (register 7).  Only bits 0..5 (the tone/noise
   enables, active low) can change; the port direction bits 6..7 are preserved
   from the driver's shadow, because registers 0..13 cannot be read back. */
void psg_mixer_modify(uint8_t clear_mask,uint8_t set_bits);
/* The only way to change the port A/B direction bits (boot path). */
void psg_mixer_set_directions(uint8_t directions);
/* For tests and diagnostics: the driver's shadow of register 7. */
uint8_t psg_mixer_shadow(void);
/* Raw access for the TOS contract (XBIOS Giaccess): permits 0..15, still
   atomic, and keeps the mixer shadow in step when register 7 is written. */
void psg_write_raw(uint8_t reg,uint8_t value);
/* Cached model of the register file (the chip itself is read through the
   control port; see psg_read()). */
uint8_t psg_shadow(uint8_t reg);
/* Reading the DATA port (0xff8802) returns 0xff, never a register value: the
   selected register is read through the CONTROL port (0xff8800). */
uint8_t psg_read_data_port(void);

uint32_t psg_rejected_writes(void);

extern volatile uint32_t psg_dbg_writes,psg_dbg_reads,psg_dbg_porta,psg_dbg_rejected,psg_dbg_mixer;
/* Last value written to the mixer and to port A: the two registers audio and
   the floppy must never corrupt for each other. */
extern volatile uint8_t psg_dbg_mixer_value,psg_dbg_porta_value;

#endif
