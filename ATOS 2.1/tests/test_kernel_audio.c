/* Unit test for the ATOS audio core (system/kernel_audio.c).
 *
 * Compiled for the host with -DPSG_HOST_TEST, so hw/psg.c writes into a test
 * double instead of the real chip and the register sequence can be asserted.
 * The kernel's 12288 Hz counter is replaced by a controllable fake, which is how
 * the queue is proven to be time-driven rather than frame-driven.
 *
 * What is NOT covered here: the real PSG timing and the emulator's sound output;
 * those are checked on the emulator with sound enabled.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "kernel_audio.h"
#include "psg.h"

/* Symbols required by hw/psg.c under PSG_HOST_TEST. */
uint8_t psg_host_regs[8];
volatile uint32_t psg_host_locks;
uint8_t psg_host_log[32][2];
volatile uint32_t psg_host_log_n;

/* The core asks for the hires counter (12288 ticks = 1 s) and takes short
   critical sections; on the host both are replaced, and the lock counter lets a
   test show that the sections really are taken. */
extern volatile uint32_t kaudio_host_irq_ok,kaudio_host_irq_points,kaudio_host_irq_locked_max,
                         kaudio_host_irq_unlocked,kaudio_host_irq_seen[2];
/* Stand-in for the persona's virtual ports: records what the interpreter tried
   to write to registers 14/15. */
static uint8_t vport_seen[8];
static unsigned vport_calls;
static int test_port_sink(uint8_t reg,uint8_t value)
{
    if(vport_calls<4u){vport_seen[vport_calls*2u]=reg;vport_seen[vport_calls*2u+1u]=value;}
    ++vport_calls;
    return 1;                       /* handled virtually */
}
static uint32_t fake_now;
volatile uint32_t kaudio_host_locks;
uint32_t kaudio_host_now(void){return fake_now;}
uint32_t kernel_user_hires_clock(void){return fake_now;}

static int failures;

static void check(int ok,const char *what)
{
    if(!ok){++failures;printf("FAIL: %s\n",what);}
    else printf("ok:   %s\n",what);
}

static void log_reset(void){psg_host_log_n=0u;}

/* Was (reg,value) written at some point in the log? */
static int wrote(uint8_t reg,uint8_t value)
{
    uint32_t i;
    for(i=0u;i<psg_host_log_n;++i)
        if(psg_host_log[i][0]==reg&&psg_host_log[i][1]==value)return 1;
    return 0;
}

int main(void)
{
    kaudio_event_t events[2];

    memset(psg_host_regs,0,sizeof(psg_host_regs));
    fake_now=1000u;
    kaudio_init();                       /* silence whatever the chip held */

    /* 1. Capabilities are honest: PSG tone/noise/queue yes, PCM and envelope no. */
    {
        uint32_t caps=kaudio_caps();
        check((caps&KAUDIO_CAP_PSG_TONE)!=0u,"caps: tone advertised");
        check((caps&KAUDIO_CAP_PSG_NOISE)!=0u,"caps: noise advertised");
        check((caps&KAUDIO_CAP_QUEUE)!=0u,"caps: effect queue advertised");
        check((caps&KAUDIO_CAP_PCM)!=0u,"caps: PCM stream advertised");
        check((caps&KAUDIO_CAP_ENVELOPE)==0u,"caps: envelope NOT advertised (not implemented)");
    }

    /* 2. One owner only. */
    check(kaudio_claim(1u,0u)==0,"claim: first process owns audio");
    check(kaudio_claim(1u,0u)==0,"claim: the same process may claim again");
    check(kaudio_claim(2u,0u)<0,"claim: a second process is refused");
    check(kaudio_owner()==1u,"claim: owner is pid 1");

    /* 3. A tone writes period, volume and enables the channel in the mixer.
          Seed register 7 like an ST boot (A out, B in, sources off): the RMW now
          reads the real register, so the test must model real hardware. */
    psg_mixer_set_directions(0x40u);   /* A out, B in - the boot path owns it */
    log_reset();
    check(kaudio_tone(1u,0u,400u,12u)==0,"tone: accepted from the owner");
    check(wrote((uint8_t)PSG_REG_TONE_A_LO,(uint8_t)(400u&0xffu)),"tone: period low written");
    check(wrote((uint8_t)PSG_REG_TONE_A_HI,(uint8_t)((400u>>8)&0x0fu)),"tone: period high written");
    check(wrote((uint8_t)PSG_REG_VOL_A,12u),"tone: volume written");
    check(psg_host_log_n>=1u&&psg_host_log[psg_host_log_n-1u][0]==PSG_REG_MIXER,
          "tone: mixer updated last");
    check((psg_mixer_shadow()&0x01u)==0u,"tone: channel A tone enabled (bit is active low)");
    check((psg_mixer_shadow()&0x08u)!=0u,"tone: channel A noise disabled");
    check((psg_mixer_shadow()&0xc0u)==0x40u,
          "tone: port direction bits kept from the boot shadow (A out, B in)");

    /* 4. A non-owner cannot make a sound. */
    check(kaudio_tone(2u,1u,200u,15u)<0,"tone: refused for a non-owner");
    check(kaudio_stop(2u,0x07u)<0,"stop: refused for a non-owner");

    /* 5. Noise uses channel C and its own enable bit. */
    log_reset();
    check(kaudio_noise(1u,17u,10u)==0,"noise: accepted");
    check(wrote((uint8_t)PSG_REG_NOISE,17u),"noise: period written");
    check(wrote((uint8_t)PSG_REG_VOL_C,10u),"noise: channel C volume written");
    check((psg_mixer_shadow()&0x20u)==0u,"noise: channel C noise enabled");
    check((psg_mixer_shadow()&0x04u)!=0u,"noise: channel C tone disabled");

    /* 6. Stop silences the requested channels only. */
    check(kaudio_stop(1u,0x01u)==0,"stop: accepted from the owner");
    check((psg_mixer_shadow()&0x01u)!=0u,"stop: channel A tone disabled");
    check((psg_mixer_shadow()&0x20u)==0u,"stop: channel C noise untouched");

    /* 7. Queued effects run from the kernel loop and are TIME driven: an event
          with a wait is not applied until enough ticks have passed, which is
          exactly what "sound independent of rendered frames" means. */
    events[0].wait=0u;   events[0].op=KAUDIO_OP_TONE;  events[0].a=1u;
    events[0].aux=300u;  events[0].b=9u;               events[0].pad[0]=0u;
    events[1].wait=12288u; /* one second later */
    events[1].op=KAUDIO_OP_STOP; events[1].a=0x02u; events[1].aux=0u;
    events[1].b=0u; events[1].pad[0]=0u;
    log_reset();
    check(kaudio_queue(1u,events,2u)==0,"queue: two events accepted");
    kaudio_pump();
    check(wrote((uint8_t)PSG_REG_VOL_B,9u),"queue: first event applied immediately");
    log_reset();
    fake_now+=6000u;                     /* ~0.5 s: still too early */
    kaudio_pump();
    check(psg_host_log_n==0u,"queue: second event not applied before its time");
    fake_now+=7000u;                     /* now past the 1 s wait */
    kaudio_pump();
    check(psg_host_log_n>0u,"queue: second event applied when due");

    /* 8. A queue that cannot fit is refused instead of silently dropped. */
    {
        kaudio_event_t many[KAUDIO_QUEUE_EVENTS];
        unsigned i;
        for(i=0u;i<KAUDIO_QUEUE_EVENTS;++i){
            many[i].wait=0u;many[i].op=KAUDIO_OP_TONE;many[i].a=0u;
            many[i].aux=100u;many[i].b=1u;many[i].pad[0]=0u;
        }
        check(kaudio_queue(1u,many,KAUDIO_QUEUE_EVENTS)==0,"queue: fills completely");
        check(kaudio_queue(1u,many,1u)<0,"queue: overflow refused");
        kaudio_stop(1u,0x07u);
        /* Drain it so the release test starts from an empty queue. */
        fake_now+=100000u;kaudio_pump();
    }

    /* 9. Release (exit, error or kill) silences everything and frees the owner. */
    kaudio_tone(1u,0u,500u,15u);
    check((psg_mixer_shadow()&0x01u)==0u,"release: channel A sounding before release");
    kaudio_release(1u);
    check(kaudio_owner()==0u,"release: owner cleared");
    check((psg_mixer_shadow()&0x3fu)==0x3fu,"release: all sources disabled");
    check((psg_mixer_shadow()&0xc0u)==0x40u,"release: port directions untouched");
    check(kaudio_claim(3u,0u)==0,"release: another process can take over");

    /* 10. Releasing a pid that never owned audio must not disturb the owner. */
    kaudio_release(99u);
    check(kaudio_owner()==3u,"release: unrelated pid ignored");

    /* 11. Dosound: a plain table of (instruction,data) pairs followed by a
           delay command.  A delay byte of zero ends playback. */
    {
        static const uint8_t seq[]={ 0u,0x34u, 1u,0x00u, 0x82u,0x00u };
        kaudio_release(3u);kaudio_release(KAUDIO_TOS_OWNER);   /* no owner at all */
        check(kaudio_dosound(seq)==0,"dosound: first call returns no previous table");
        check(kaudio_dosound_state()==1u,"dosound: sequence is playing");
        kaudio_timer_tick();                      /* nothing else drives it */
        check(psg_shadow(0u)==0x34u,"dosound: register 0 written from the table");
        check(psg_shadow(1u)==0x00u,"dosound: register 1 written from the table");
        check(kaudio_dosound_state()==0u,"dosound: delay 0 ended playback");
        check(kaudio_dbg_dosound_starts==1u,"dosound: start counted");
        check(kaudio_dbg_dosound_ends==1u,"dosound: end counted");
    }

    /* 12. The point of the Timer C hook: a sequence with delays advances one
           step per interrupt, with NO application call in between.  This is what
           lets a TOS program compute in a tight loop and still hear sound. */
    {
        static const uint8_t seq[]={ 0u,0x10u, 0x82u,3u, 0u,0x20u, 0x82u,0x00u };
        uint32_t ticks_before=kaudio_dbg_ticks;
        kaudio_dosound(seq);
        kaudio_timer_tick();                      /* writes 0x10, delay = 3 */
        check(psg_shadow(0u)==0x10u,"dosound: first step writes 0x10");
        check(kaudio_dosound_delay()==3u,"dosound: delay armed to 3 ticks");
        kaudio_timer_tick();
        kaudio_timer_tick();
        check(psg_shadow(0u)==0x10u,"dosound: delay ticks do not write");
        /* A delay of 3 costs three ticks and returns; the write happens on the
           tick AFTER them - the same off-by-one EmuTOS has. */
        kaudio_timer_tick();                      /* 3rd tick: delay reaches 0 */
        check(psg_shadow(0u)==0x10u,"dosound: the 3rd delay tick still does not write");
        check(kaudio_dosound_delay()==0u,"dosound: delay exhausted after three ticks");
        kaudio_timer_tick();                      /* continue -> write, then end */
        check(psg_shadow(0u)==0x20u,"dosound: sequence continued on the next tick");
        check(kaudio_dosound_state()==0u,"dosound: second delay 0 ended playback");
        check(kaudio_dbg_ticks==ticks_before+5u,"dosound: five Timer C ticks counted");
    }

    /* 13. Contract from EmuTOS dosound(): NULL stops interpreting the table, a
           NEGATIVE pointer is only a query - it must neither stop playback nor
           reset the pending delay.  Stopping the table is not "silence". */
    {
        static const uint8_t seq[]={ 0u,0x11u, 0x82u,5u, 0u,0x22u, 0x82u,0x00u };
        const uint8_t *cur;
        kaudio_dosound(seq);
        kaudio_timer_tick();                      /* writes 0x11, arms delay 5 */
        check(psg_shadow(0u)==0x11u,"dosound: first step wrote 0x11");
        check(kaudio_dosound_delay()==5u,"dosound: delay armed to 5 ticks");
        cur=kaudio_dosound((const uint8_t *)(intptr_t)-1);          /* query */
        check(cur!=0,"dosound: query returns the current pointer");
        check(kaudio_dosound_state()==1u,"dosound: query does not stop playback");
        check(kaudio_dosound_delay()==5u,"dosound: query does not reset the delay");
        check(kaudio_dosound(0)==cur,"dosound: NULL returns the previous pointer");
        check(kaudio_dosound_state()==0u,"dosound: NULL stopped interpreting the table");
        check(psg_shadow(0u)==0x11u,"dosound: stopping the table leaves the chip alone");
        kaudio_timer_tick();
        check(psg_shadow(0u)==0x11u,"dosound: a stopped table does not advance");
    }

    /* 13b. Appending to a non-empty queue must not pull the head's deadline
            forward, and the schedule must stay on the time axis (no drift). */
    {
        kaudio_event_t ev[2];
        kaudio_release(KAUDIO_TOS_OWNER);
        kaudio_claim(9u,0u);
        fake_now=10000u;
        ev[0].wait=12288u;ev[0].op=KAUDIO_OP_TONE;ev[0].a=0u;ev[0].aux=200u;ev[0].b=5u;ev[0].pad[0]=0u;
        check(kaudio_queue(9u,ev,1u)==0,"queue: head scheduled one second ahead");
        fake_now=20000u;                              /* 10000 ticks later */
        ev[0].wait=0u;ev[0].aux=210u;
        check(kaudio_queue(9u,ev,1u)==0,"queue: appended while the head is pending");
        {uint8_t before0=psg_shadow(0u);
         kaudio_pump();                          /* the head is NOT due yet */
         check(psg_shadow(0u)==before0,"queue: appended event did not steal the slot");}
        /* The head was due at 22288; run time forward and it must fire there. */
        fake_now=22288u;
        kaudio_pump();
        /* Both events are due in that tick: the head at 22288, and the appended
           one at head_deadline + its own wait (0) - cumulative, so the last one
           applied is the appended one.  This is the schedule staying on the time
           axis instead of drifting. */
        check(psg_shadow(0u)==210u,"queue: cumulative schedule fired both on time");
        kaudio_stop(9u,0x07u);
        kaudio_release(9u);
    }

    /* 14. Port bits (Offgibit/Ongibit) touch PSG port A only, and the mixer's
           direction bits are never disturbed by them. */
    {
        uint8_t dirs_before=psg_mixer_shadow();
        psg_port_a_modify(0x00u,0xffu);            /* port A = 0xff */
        kaudio_offgibit(0xDFu);                    /* TOS: portA &= 0xDF */
        check(psg_dbg_porta_value==0xDFu,"gibit: offgibit ANDs the mask (TOS semantics)");
        kaudio_ongibit(0x20u);                     /* portA |= 0x20 */
        check(psg_dbg_porta_value==0xFFu,"gibit: ongibit ORs the mask");
        check(psg_mixer_shadow()==dirs_before,"gibit: port writes leave directions alone");
    }

    /* 14b. The port bit set by ongibit must be visible through the register
           shadow, because that is what a TOS Giaccess read returns. */
    {
        psg_port_a_modify(0x00u,0x00u);           /* clear port A */
        kaudio_ongibit(0x20u);
        check((psg_shadow(14u)&0x20u)!=0u,"ongibit: port A bit visible via shadow");
        kaudio_offgibit(0xDFu);                   /* portA &= 0xDF */
        check((psg_shadow(14u)&0x20u)==0u,"offgibit: bit cleared in the shadow");
    }

    /* 15. A running ATOS application keeps the chip: Dosound must refuse rather
           than steal the channels from it. */
    {
        static const uint8_t seq[]={ 0u,0x33u, 0x82u,0x00u };
        kaudio_release(KAUDIO_TOS_OWNER);
        kaudio_claim(7u,0u);                       /* the "application" owns it */
        check(kaudio_dosound(seq)==0,"dosound: refused when an application owns audio");
        check(kaudio_dosound_state()==0u,"dosound: nothing started");
        check(kaudio_dbg_dosound_rejected==1u,"dosound: refusal counted");
        kaudio_release(7u);
    }

    /* 16. Tearing the TOS session down must detach the table, drop the queue and
           release the domain owner BEFORE the launcher releases the program's
           TPA.  Otherwise the Timer C interpreter walks freed memory. */
    {
        static const uint8_t seq[]={ 0u,0x44u, 0x82u,10u, 0u,0x55u, 0x82u,0x00u };
        kaudio_event_t ev;
        uint32_t gen,before_gen;
        kaudio_release(KAUDIO_TOS_OWNER);
        kaudio_dosound(seq);
        check(kaudio_dosound_state()==1u,"teardown: sequence playing before");
        ev.wait=0u;ev.op=KAUDIO_OP_TONE;ev.a=0u;ev.aux=300u;ev.b=4u;ev.pad[0]=0u;
        (void)kaudio_queue(KAUDIO_TOS_OWNER,&ev,1u);
        before_gen=kaudio_tos_generation();
        gen=kaudio_tos_teardown();
        check(gen==before_gen+1u,"teardown: generation advanced");
        check(kaudio_dosound_state()==0u,"teardown: table detached");
        check(kaudio_owner()==0u,"teardown: domain owner released");
        check(kaudio_state_qcount==0u,"teardown: queue dropped");
        check(psg_shadow(8u)==0u,"teardown: channel A silenced");
        kaudio_timer_tick();                  /* an interrupt after the teardown */
        check(psg_shadow(0u)!=0x55u,"teardown: no stale table is walked");
    }

    /* 17. Owner policy for the legacy TOS entries: while a NATIVE application
           owns the chip, TOS must not be allowed to write sound registers and a
           Dosound start must be refused (counted, not silently accepted). */
    {
        static const uint8_t seq[]={ 0u,0x33u, 0x82u,0x00u };
        uint32_t starts_before;
        kaudio_release(KAUDIO_TOS_OWNER);
        kaudio_claim(7u,0u);                      /* a native application owns it */
        check(kaudio_tos_may_write()==0,"tos policy: native owner blocks TOS writes");
        starts_before=kaudio_dbg_dosound_starts;
        (void)kaudio_dosound(seq);
        check(kaudio_dbg_dosound_starts==starts_before,"tos policy: Dosound refused");
        check(kaudio_dbg_dosound_rejected>0u,"tos policy: refusal is counted");
        check(kaudio_dosound_state()==0u,"tos policy: nothing started");
        kaudio_release(7u);
        check(kaudio_tos_may_write()==1,"tos policy: free chip allows TOS writes");
        (void)kaudio_dosound(seq);
        check(kaudio_dosound_state()==1u,"tos policy: TOS start accepted when free");
        kaudio_release(KAUDIO_TOS_OWNER);
    }

    /* 18. A Dosound start must NOT be accepted when the Timer C hook cannot be
           installed: nothing would ever step the sequence, and reporting success
           would be a fake. */
    {
        static const uint8_t seq[]={ 0u,0x11u, 0x82u,0x00u };
        uint32_t before;
        kaudio_release(KAUDIO_TOS_OWNER);
        kaudio_host_irq_ok=0u;
        before=kaudio_dbg_dosound_starts;
        (void)kaudio_dosound(seq);
        check(kaudio_dbg_dosound_starts==before,"irq: Dosound refused without a timer hook");
        check(kaudio_dosound_state()==0u,"irq: nothing started");
        check(kaudio_owner()==0u,"irq: the refused request did not keep the owner");
        kaudio_host_irq_ok=1u;                     /* the service is back */
        check(kaudio_claim(7u,0u)==0,"irq: another client can still take the chip");
        kaudio_release(7u);
        kaudio_host_irq_ok=1u;
        (void)kaudio_dosound(seq);
        check(kaudio_dosound_state()==1u,"irq: accepted once the hook is available");
        kaudio_release(KAUDIO_TOS_OWNER);
    }

    /* 19. P1.1: a TOS program must not destroy a native owner's queued work -
           and the queued event must actually be EXECUTED afterwards.  The audit
           showed the earlier version of this test passed on a channel that was
           already silent, so it proves nothing: here the channel is made audible
           first and the stop has to bring it down to zero. */
    {
        kaudio_event_t ev;uint32_t q_before;
        kaudio_release(KAUDIO_TOS_OWNER);kaudio_release(7u);
        check(kaudio_claim(7u,0u)==0,"p1.1: native application owns the chip");
        check(kaudio_tone(7u,0u,0x100u,12u)==0,"p1.1: native tone accepted");
        check(psg_shadow(8u)==12u,"p1.1: channel A is audible before the stop");
        fake_now=1000u;
        ev.wait=12288u;ev.op=KAUDIO_OP_STOP;ev.a=7u;ev.aux=0u;ev.b=0u;ev.pad[0]=0u;
        check(kaudio_queue(7u,&ev,1u)==0,"p1.1: native queued work accepted");
        q_before=kaudio_state_qcount;
        (void)kaudio_tos_teardown();               /* a TOS program ran and ended */
        check(kaudio_owner()==7u,"p1.1: native owner survives a TOS teardown");
        check(kaudio_state_qcount==q_before,"p1.1: native queue survives a TOS teardown");
        kaudio_pump();                             /* a tick before the deadline */
        check(psg_shadow(8u)==12u,"p1.1: the stop has not fired early");
        check(kaudio_state_qcount==q_before,"p1.1: the queue still holds the event");
        fake_now=1000u+12288u;                     /* the deadline arrives */
        kaudio_pump();
        check(psg_shadow(8u)==0u,"p1.1: the native scheduled stop really executed");
        check(kaudio_state_qcount==0u,"p1.1: the queue drained when it should");
        kaudio_release(7u);
    }

    /* 20. P1.2: the Dosound interpreter must not touch the physical ports. */
    {
        static const uint8_t seqA[]={ 14u,0x5Au, 0x82u,0x00u };
        static const uint8_t seqB[]={ 15u,0x11u, 0x82u,0x00u };
        static const uint8_t seqInc[]={ 0x80u,0x00u, 0x81u,14u,1u,1u, 0x82u,0x00u };
        kaudio_release(KAUDIO_TOS_OWNER);
        psg_port_a_modify(0xFFu,0xA7u);            /* a real value on the hardware port */
        vport_calls=0u;kaudio_set_port_sink(test_port_sink);
        (void)kaudio_dosound(seqA);kaudio_timer_tick();kaudio_timer_tick();
        check((psg_dbg_porta_value&0xFFu)==0xA7u,"p1.2: port A untouched by the interpreter");
        check(vport_calls>0u,"p1.2: register 14 went to the virtual sink");
        check(vport_seen[0]==14u&&vport_seen[1]==0x5Au,"p1.2: sink got register and value");
        (void)kaudio_dosound(seqB);kaudio_timer_tick();kaudio_timer_tick();
        check((psg_dbg_porta_value&0xFFu)==0xA7u,"p1.2: port B path cannot touch port A");
        check(vport_seen[2]==15u,"p1.2: register 15 went to the virtual sink");
        (void)kaudio_dosound(seqInc);
        kaudio_timer_tick();kaudio_timer_tick();kaudio_timer_tick();
        check((psg_dbg_porta_value&0xFFu)==0xA7u,"p1.2: 0x81 increment path virtualised too");
        kaudio_set_port_sink(0);
        /* Without a sink the ports are refused, still not touched. */
        (void)kaudio_dosound(seqA);kaudio_timer_tick();kaudio_timer_tick();
        check((psg_dbg_porta_value&0xFFu)==0xA7u,"p1.2: no sink means refusal, not a write");
        kaudio_release(KAUDIO_TOS_OWNER);
    }

    /* 21. P1.3: the interpreter must not read past the range it was given. */
    {
        static const uint8_t seq[]={ 0u,0x11u, 1u,0x22u, 2u,0x33u, 0x82u,0x00u };
        uint32_t ends_before=kaudio_dbg_dosound_ends;
        kaudio_release(KAUDIO_TOS_OWNER);
        (void)kaudio_dosound_span(seq,seq+3u);     /* only the first pair is valid */
        kaudio_timer_tick();kaudio_timer_tick();
        check(kaudio_dosound_state()==0u,"p1.3: stopped at the proven end of the table");
        check(kaudio_dbg_dosound_ends>ends_before,"p1.3: stopping at the range end is counted");
        check(psg_shadow(1u)!=0x22u,"p1.3: no byte past the range was interpreted");
    }

    /* 22. P1.4: coverage of the critical sections.  Every instrumented point must
           be reached WITH the lock held, and every point that the sequence touches
           must appear.  This is a measurement of section coverage, not a simulated
           interrupt delivery - the report asked for that distinction to be honest. */
    {
        kaudio_event_t ev;
        kaudio_host_irq_points=0u;kaudio_host_irq_locked_max=0u;
        kaudio_host_irq_unlocked=0u;kaudio_host_irq_seen[0]=0u;kaudio_host_irq_seen[1]=0u;
        kaudio_release(5u);kaudio_release(KAUDIO_TOS_OWNER);
        (void)kaudio_claim(5u,0u);                 /* point 1 */
        (void)kaudio_tone(5u,0u,0x100u,10u);       /* point 3 */
        ev.wait=0u;ev.op=KAUDIO_OP_TONE;ev.a=1u;ev.aux=0x120u;ev.b=8u;ev.pad[0]=0u;
        (void)kaudio_queue(5u,&ev,1u);             /* point 9 */
        (void)kaudio_stop(5u,0x07u);               /* point 5 */
        kaudio_release(5u);                        /* point 2 */
        check(kaudio_host_irq_unlocked==0u,"p1.4: no instrumented point outside the lock");
        check((kaudio_host_irq_seen[0]&(1u<<1))!=0u,"p1.4: claim covered");
        check((kaudio_host_irq_seen[0]&(1u<<2))!=0u,"p1.4: release covered");
        check((kaudio_host_irq_seen[0]&(1u<<3))!=0u,"p1.4: tone covered");
        check((kaudio_host_irq_seen[0]&(1u<<5))!=0u,"p1.4: stop covered");
        check((kaudio_host_irq_seen[0]&(1u<<9))!=0u,"p1.4: queue covered");
    }

    /* 23. P2.5: one register state, and the widths the chip really stores. */
    {
        psg_mixer_modify(0x00u,0x00u);
        check(psg_shadow(7u)==psg_mixer_shadow(),"p2.5: mixer read and cache agree");
        psg_write_raw(1u,0xFFu);
        check(psg_shadow(1u)==0x0Fu,"p2.5: coarse period is four bits on read");
        psg_write_raw(6u,0xFFu);
        check(psg_shadow(6u)==0x1Fu,"p2.5: noise period is five bits");
    }

    /* 24. Freeing or shrinking a buffer must affect the interpreter ONLY when the
           table really lives in that buffer (audit R13 point 2: checking just
           "table >= lo" stopped a table in an unrelated block above).  The ranges
           below are address arithmetic on a real table; only the numbers matter. */
    {
        static const uint8_t seq[]={ 0u,0x77u, 0x82u,255u, 0u,0x66u, 0x82u,0x00u };
        const uint8_t *lo=seq,*hi=seq+sizeof(seq);
        kaudio_release(KAUDIO_TOS_OWNER);
        /* (a) free of a block BELOW the table: must not touch it */
        (void)kaudio_dosound(seq);
        kaudio_timer_tick();
        check(psg_shadow(0u)==0x77u,"revalidate: sequence running before (a)");
        kaudio_dosound_revalidate(lo-32u,lo-16u,lo-32u);
        check(kaudio_dosound_state()==1u,"revalidate: free of a LOWER block keeps it");
        /* (b) free of a block ABOVE the table: must not touch it either */
        kaudio_dosound_revalidate(hi,hi+16u,hi);
        check(kaudio_dosound_state()==1u,"revalidate: free of a HIGHER block keeps it");
        /* (c) shrink of the own block, pointer still inside the new range */
        kaudio_dosound_revalidate(lo,hi,hi);
        check(kaudio_dosound_state()==1u,"revalidate: own shrink that keeps the table running");
        kaudio_timer_tick();
        check(psg_shadow(0u)==0x77u,"revalidate: still playing after the clamps");
        /* (d) shrink of the own block below the interpreter pointer: stops */
        kaudio_dosound_revalidate(lo,hi,lo+4u);
        check(kaudio_dosound_state()==0u,"revalidate: own shrink past the pointer stops it");
        /* (e) free of the own block: stops */
        (void)kaudio_dosound(seq);
        kaudio_timer_tick();
        check(kaudio_dosound_state()==1u,"revalidate: re-armed for the free case");
        kaudio_dosound_revalidate(lo,hi,lo);
        check(kaudio_dosound_state()==0u,"revalidate: freeing the own buffer stops it");
        kaudio_timer_tick();
        check(psg_shadow(0u)==0x77u,"revalidate: a stopped table does not advance");
    }

    printf(failures?"AUDIO: %d FAILED\n":"AUDIO: all checks passed\n",failures);
    return failures?1:0;
}
