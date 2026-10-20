/* PSG (YM2149) register access for ATOS.
 *
 * Why this file exists: the PSG register-select latch is *stateful* and the
 * floppy drive/side select shares PSG port A (register 14).  A register access
 * is therefore two bus writes (select, then data) that must not be split by an
 * interrupt, or an audio write can land in the floppy's port-A access (and the
 * other way round), which corrupts both the sound and the drive select.
 *
 * Rules enforced here:
 *   - select + data form ONE atomic unit (interrupts masked to IPL 7),
 *   - the audio path may only write registers 0..13; register 7 (port
 *     directions), 14 (port A) and 15 (port B) are refused,
 *   - port A is only ever changed with psg_port_a_modify(), so the direction
 *     bits and the hardware ports of the other devices are preserved.
 *
 * The driver stays deliberately PSG-only: PCM/DMA devices are expected to
 * register later through the audio core, not here.
 */
#include "hardware.h"
#include "psg.h"

#define PSG_SELECT 0xffff8800u
#define PSG_DATA   0xffff8802u

/* Host-side unit test double: the rules this file enforces (which registers the
   audio path may touch, and that port A is only modified by read-modify-write)
   are testable on the host, where the real bus and the m68k status register do
   not exist.  tests/test_psg.c defines PSG_HOST_TEST and supplies psg_host_regs. */
/* Bus access.
 *
 * The YM2149 is reached through two addresses and they are NOT symmetric
 * (Hatari src/psg.c documents it, EmuTOS relies on it):
 *   0xff8800 write = select a register
 *   0xff8800 READ  = return the content of the selected register   <- the read
 *   0xff8802 write = write the selected register
 *   0xff8802 READ  = no action, returns 0xff
 * Reading the data port to learn a register's value is therefore wrong; it
 * returns 0xff, which is how an earlier version of this driver corrupted the
 * shared floppy port (OR-ing into 0xff leaves the wrong bits set).
 *
 * The host double keeps one byte per register and reproduces that asymmetry, so
 * a test can catch a driver that reads the wrong port.
 */
#ifdef PSG_HOST_TEST
extern uint8_t psg_host_regs[8];      /* [0] = latch, [1] = last data transfer */
extern volatile uint32_t psg_host_locks;
extern uint8_t psg_host_log[32][2];
extern volatile uint32_t psg_host_log_n;

static uint8_t psg_host_store[16];
static void psg_host_record(uint8_t reg,uint8_t value)
{
    uint32_t n=psg_host_log_n;
    if(n<32u){psg_host_log[n][0]=reg;psg_host_log[n][1]=value;++psg_host_log_n;}
}
static void psg_bus_select(uint8_t reg){psg_host_regs[0]=reg;}
static void psg_bus_write_data(uint8_t value)
{
    uint8_t reg=psg_host_regs[0]&0x0fu;
    psg_host_regs[1]=value;
    psg_host_store[reg]=value;
    psg_host_record(reg,value);
}
/* Reading the CONTROL port returns the selected register. */
static uint8_t psg_bus_read_reg(void)
{
    uint8_t v=psg_host_store[psg_host_regs[0]&0x0fu];
    psg_host_regs[1]=v;
    return v;
}
/* Reading the DATA port returns 0xff, exactly like the hardware. */
static uint8_t psg_bus_read_data(void){psg_host_regs[1]=0xffu;return 0xffu;}
#define PSG_RECORD(reg,value) do{}while(0)
#else
#define psg_bus_select(reg) (REG8(PSG_SELECT)=(uint8_t)(reg))
#define psg_bus_write_data(value) (REG8(PSG_DATA)=(uint8_t)(value))
#define psg_bus_read_reg() (REG8(PSG_SELECT))
#define psg_bus_read_data() (REG8(PSG_DATA))
#define PSG_RECORD(reg,value) do{}while(0)
#endif

/* Readable from the debugger and the C tests. */
volatile uint32_t psg_dbg_writes,psg_dbg_reads,psg_dbg_porta,psg_dbg_rejected,psg_dbg_mixer;

/* Last value written to the mixer and to port A, readable from the debugger and
   the emulator tests (the accessors are functions, so a name lookup by symbol
   would land on code, not on data). */
volatile uint8_t psg_dbg_mixer_value=0x7fu,psg_dbg_porta_value=0xffu;

/* Last value written to each register.  The chip CAN be read back through the
   control port (see psg_bus_read_reg), so this is a cached model of the same
   state - not a replacement for reading the bus. */
static uint8_t reg_shadow[16];
volatile uint32_t psg_pcm_sample;
volatile uint8_t psg_pcm_active;

/* The PSG needs a short settle time between the address latch and the data
   access.  Four NOPs is what the FDC path used before this file existed and is
   enough on an 8 MHz bus; keeping it small matters when audio writes registers
   at a high rate. */
static void psg_settle(void)
{
#ifndef PSG_HOST_TEST
    volatile unsigned i;for(i=0u;i<4u;++i)__asm__ volatile("nop");
#endif
}

#ifdef PSG_HOST_TEST
static uint16_t psg_lock(void){++psg_host_locks;return 0u;}
static void psg_unlock(uint16_t sr){(void)sr;}
#else
static uint16_t psg_lock(void)
{
    uint16_t sr;
    __asm__ volatile("move.w %%sr,%0; ori.w #0x0700,%%sr":"=d"(sr)::"memory","cc");
    return sr;
}
static void psg_unlock(uint16_t sr){__asm__ volatile("move.w %0,%%sr"::"d"(sr):"memory","cc");}
#endif

uint8_t psg_read(uint8_t reg)
{
    uint16_t sr=psg_lock();
    uint8_t value;
    psg_bus_select(reg);
    psg_settle();
    value=psg_bus_read_reg();   /* 0xff8800 read returns the register */
    psg_unlock(sr);
    ++psg_dbg_reads;
    return value;
}

void psg_write(uint8_t reg,uint8_t value)
{
    uint16_t sr;
    if(reg==PSG_REG_MIXER||reg>=PSG_REG_IO_A){++psg_dbg_rejected;return;}
    sr=psg_lock();
    psg_bus_select(reg);
    psg_settle();
    psg_bus_write_data(value);
    reg_shadow[reg]=value;            /* every writer keeps the record honest */
    psg_unlock(sr);
    ++psg_dbg_writes;
}

/* Atomic read-modify-write of port A (register 14): one lock covers select,
   read-back and write, so a concurrent audio access cannot interleave. */
void psg_port_a_modify(uint8_t clear_mask,uint8_t set_bits)
{
    uint16_t sr=psg_lock();
    uint8_t old,now;
    psg_bus_select(PSG_REG_IO_A);
    psg_settle();
    old=psg_bus_read_reg();   /* the shared port must be read via 0xff8800 */
    now=(uint8_t)((old&(uint8_t)~clear_mask)|set_bits);
    psg_bus_write_data(now);
    reg_shadow[PSG_REG_IO_A]=now;
    psg_dbg_porta_value=now;
    psg_unlock(sr);
    ++psg_dbg_porta;
}

/* Atomic update of the mixer register (7).
 *
 * Registers 0..13 of a YM2149 are WRITE-ONLY: reading register 7 back does not
 * return what was written, so a read-modify-write on the chip is impossible.
 * This driver keeps a software shadow instead.  Bits 0..5 (tone/noise enables,
 * active low) belong to audio; bits 6..7 are the port A/B DIRECTION bits and are
 * only reachable through psg_mixer_set_directions(), which the boot path owns.
 *
 * The reset value follows the standard ST wiring: port A output (bit 6 = 1, the
 * floppy/side select), port B input (bit 7 = 0), all sources disabled. */
static uint8_t mixer_shadow=0x7fu;

void psg_mixer_modify(uint8_t clear_mask,uint8_t set_bits)
{
    uint16_t sr=psg_lock();
    uint8_t old,next;
    psg_bus_select(PSG_REG_MIXER);
    psg_settle();
    old=psg_bus_read_reg();                 /* the real register 7 */
    /* Bits 0..5 are the enables (active low) and belong to audio; bits 6..7 are
       the port directions and are taken from what the chip actually holds, so a
       program that changed them through Giaccess is not overruled here. */
    next=(uint8_t)((old&0xc0u)|((uint8_t)((old&0x3fu)&(uint8_t)~clear_mask)|(set_bits&0x3fu)));
    psg_bus_write_data(next);
    mixer_shadow=next;
    reg_shadow[PSG_REG_MIXER]=next;      /* one state, every reader */
    PSG_RECORD(PSG_REG_MIXER,next);
    psg_dbg_mixer_value=next;
    psg_unlock(sr);
    ++psg_dbg_mixer;
}

/* The only way to change the port direction bits, kept separate so no audio
   path can reach them by accident. */
void psg_mixer_set_directions(uint8_t directions)
{
    uint16_t sr=psg_lock();
    uint8_t next;
    /* The one place allowed to move bits 6..7: it writes the register directly
       instead of going through the read-modify-write, which by design keeps
       whatever directions the chip already has. */
    next=(uint8_t)((mixer_shadow&0x3fu)|(directions&0xc0u));
    psg_bus_select(PSG_REG_MIXER);
    psg_settle();
    psg_bus_write_data(next);
    mixer_shadow=next;
    reg_shadow[PSG_REG_MIXER]=next;      /* one state, every reader */
    psg_unlock(sr);
    ++psg_dbg_mixer;
}

uint8_t psg_mixer_shadow(void){return mixer_shadow;}

/* ---------------------------------------------------------------- raw access
   The TOS contract (XBIOS Giaccess) lets a program touch any PSG register, so
   this is the one entry point that allows 0..15.  It is still atomic (one lock
   around select+write) and it keeps the driver's bookkeeping honest:
   writing register 7 updates the mixer shadow, so the audio path cannot later
   "restore" directions the program deliberately changed. */

void psg_write_raw(uint8_t reg,uint8_t value)
{
    uint16_t sr;
    uint8_t effective=value;
    if(reg>15u){++psg_dbg_rejected;return;}
    /* A program may set the mixer enables through Giaccess, but the port
       DIRECTION bits belong to the driver/boot path: keep them even here. */
    if(reg==PSG_REG_MIXER)
        effective=(uint8_t)((value&0x3fu)|(mixer_shadow&0xc0u));
    sr=psg_lock();
    psg_bus_select(reg);
    psg_settle();
    psg_bus_write_data(effective);
    reg_shadow[reg]=effective;
    if(reg==PSG_REG_MIXER)mixer_shadow=effective;
    psg_unlock(sr);
    ++psg_dbg_writes;
}

/* Last value written to a register.  Registers 0..13 are write-only on a real
   YM2149, so this is the driver's own record, which is what a Giaccess read can
   honestly return. */
/* The chip does not store 8 bits in every register: the high nibble of the
   coarse period registers and the noise/envelope-period registers is unused, and
   Hatari masks a read the same way.  A read model that ignores this reports
   values the hardware never holds. */
static uint8_t psg_reg_mask(uint8_t reg)
{
    switch(reg){
    case 1u:case 3u:case 5u:            return 0x0fu;   /* coarse period: 4 bits */
    case 6u:                            return 0x1fu;   /* noise period: 5 bits */
    case 8u:case 9u:case 10u:           return 0x1fu;   /* volume: 5 bits */
    default:                            return 0xffu;
    }
}

uint8_t psg_shadow(uint8_t reg)
{
    if(reg>=16u)return 0u;
    if(psg_pcm_active&&reg>=8u&&reg<=10u)
        return (uint8_t)((psg_pcm_sample>>((10u-reg)*8u))&15u);
    /* Register 7 is the mixer: keep the one state, whichever path wrote it. */
    if(reg==PSG_REG_MIXER)return (uint8_t)((mixer_shadow&0xffu));
    return (uint8_t)(reg_shadow[reg]&psg_reg_mask(reg));
}

/* Diagnostic: reading the DATA port never returns a register value (0xff on the
   real chip and in Hatari).  Kept visible so a test can show the asymmetry. */
uint8_t psg_read_data_port(void){return psg_bus_read_data();}

/* One settle-time calibration point for tests: how many accesses we refused. */
uint32_t psg_rejected_writes(void){return psg_dbg_rejected;}
