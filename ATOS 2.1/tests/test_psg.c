/* Unit test for the PSG access rules in hw/psg.c.
 *
 * What is testable on the host: which registers the audio path is allowed to
 * write, that port A is only ever changed with a read-modify-write (so the
 * direction bits and the other devices' hardware ports survive), and that the
 * whole select+data pair runs inside one locked section.
 *
 * What is NOT testable here: the real bus timing and the m68k status register,
 * so atomicity itself is structural (see psg_lock/psg_unlock) and is verified on
 * the emulator instead.
 *
 * The double maps the two PSG bus addresses onto psg_host_regs[0] (select
 * latch) and psg_host_regs[1] (data), which mirrors the real chip: the data port
 * carries the value of whichever register the latch points at.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "psg.h"

/* The test double that hw/psg.c expects when compiled with -DPSG_HOST_TEST. */
uint8_t psg_host_regs[8];
volatile uint32_t psg_host_locks;
uint8_t psg_host_log[32][2];
volatile uint32_t psg_host_log_n;

static int failures;

static void check(int ok,const char *what)
{
    if(!ok){++failures;printf("FAIL: %s\n",what);}
    else printf("ok:   %s\n",what);
}

int main(void)
{
    memset(psg_host_regs,0,sizeof(psg_host_regs));

    /* 1. A legal audio register write reaches the chip: latch = register,
          data port = value, one lock taken. */
    psg_write(PSG_REG_VOL_A,0x0fu);
    check(psg_host_regs[0]==PSG_REG_VOL_A,"vol A: register latch selected");
    check(psg_host_regs[1]==0x0fu,"vol A: value reached the data port");
    check(psg_dbg_writes==1u,"vol A: counted as one write");
    check(psg_host_locks==1u,"vol A: write ran inside one locked section");

    /* 2. Register 7 holds the port directions - the audio path must not write
          it.  The data port must stay untouched. */
    {
        uint8_t before=psg_host_regs[1];
        psg_write(PSG_REG_MIXER,0x38u);
        check(psg_dbg_rejected==1u,"register 7: refused and counted");
        check(psg_host_regs[1]==before,"register 7: data port untouched");
        check(psg_dbg_writes==1u,"register 7: not counted as a write");
    }

    /* 3. Registers 14 and 15 are the hardware ports (floppy select lives on
          port A) - also refused. */
    psg_write(PSG_REG_IO_A,0x00u);
    psg_write(PSG_REG_IO_B,0x00u);
    check(psg_dbg_rejected==3u,"registers 14/15: refused and counted");

    /* 4. Port A read-modify-write preserves every bit outside the mask.  The
          port starts as all ones (both drives deselected, side 1); the test
          reaches it through the public API, not by poking the model. */
    psg_port_a_modify(0x00u,0xffu);          /* port A = 0xff */
    {
        uint32_t locks_before=psg_host_locks;
        psg_port_a_modify(0x07u,0x02u);      /* drive A selected, side 0 */
        check(psg_host_locks==locks_before+1u,
              "port A: select+read+write in one locked section");
    }
    check(psg_host_regs[0]==PSG_REG_IO_A,"port A: latch selected register 14");
    check(psg_dbg_porta_value==0xfau,"port A: upper bits kept, low bits replaced");
    check(psg_host_regs[1]==0xfau,"port A: data port carried the new value");
    check(psg_dbg_porta==2u,"port A: counted as two modifies");

    /* 5. Selecting drive B must not disturb the upper bits either. */
    psg_port_a_modify(0x07u,0x04u);          /* drive B, side 0 */
    check(psg_dbg_porta_value==0xfcu,"port A: second modify keeps upper bits");

    /* 6. Registers 0..13 are write-only on the real chip, so the model keeps one
          byte per register: a value written to a register reads back from THAT
          register, not from whatever was written last. */
    psg_write(PSG_REG_VOL_B,0x42u);
    psg_write(PSG_REG_TONE_A_LO,0x11u);
    check(psg_read(PSG_REG_VOL_B)==0x42u,"read: returns the selected register's byte");
    check(psg_host_regs[0]==PSG_REG_VOL_B,"read: latch left on the read register");
    check(psg_dbg_reads==1u,"read: counted");

    /* 7. Interleaving - the point of this whole file.  The floppy (port A) and
          audio (mixer) share one chip, so hammer them alternately: neither may
          lose the other's bits, and the mixer's direction bits must never move.
          Both go through the same lock, which is what makes this hold. */
    {
        unsigned i;int ok=1;
        /* Seed the chip's mixer the way an ST boots it (port A out, port B in,
           all sources disabled).  With the real readback the RMW must start from
           what the hardware holds, not from a driver-side default. */
        psg_mixer_set_directions(0x40u);   /* the boot path owns bits 6..7 */
        psg_mixer_modify(0u,0x3fu);        /* all sources disabled */
        psg_port_a_modify(0x00u,0xffu);                 /* port A = 0xff */
        for(i=0u;i<64u;++i){
            psg_mixer_modify(0x01u,0u);                 /* audio: tone A on   */
            if((psg_mixer_shadow()&0xc0u)!=0x40u)ok=0;  /* directions intact  */
            psg_port_a_modify(0x07u,0x02u);             /* floppy: drive A    */
            if((psg_dbg_porta_value&0xf8u)!=0xf8u)ok=0; /* upper bits intact  */
            psg_mixer_modify(0u,0x01u);                 /* audio: tone A off  */
            psg_port_a_modify(0x07u,0x00u);             /* floppy: deselect   */
            if((psg_dbg_porta_value&0xf8u)!=0xf8u)ok=0;
        }
        check(ok,"interleave: audio and floppy keep each other's bits");
        check((psg_mixer_shadow()&0x3fu)==0x3fu,"interleave: all sources off at the end");
        check((psg_dbg_porta_value&0xf8u)==0xf8u,"interleave: port A upper bits survive 256 accesses");
    }

    /* 8. Raw access for the TOS contract (Giaccess).  Registers 0..15 are
          reachable, but a write to register 7 must NOT move the direction bits:
          the program owns the mixer enables, the driver owns bits 6..7. */
    {
        uint8_t dirs=psg_mixer_shadow()&0xc0u;
        psg_write_raw(PSG_REG_VOL_C,0x0du);
        check(psg_shadow(PSG_REG_VOL_C)==0x0du,"raw: value recorded for read-back");
        psg_write_raw(PSG_REG_MIXER,0x00u);        /* program enables everything */
        check((psg_mixer_shadow()&0x3fu)==0x00u,"raw: mixer enables taken from the program");
        check((psg_mixer_shadow()&0xc0u)==dirs,"raw: direction bits preserved");
        psg_write_raw(16u,0u);                     /* out of range */
        check(psg_dbg_rejected==4u,"raw: register > 15 refused and counted");
    }

    printf(failures?"PSG: %d FAILED\n":"PSG: all checks passed\n",failures);
    return failures?1:0;
}
