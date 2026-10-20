/* ATOS audio core.  See include/kernel_audio.h for the rules and the scope. */
#include <stdint.h>
#include "kernel_audio.h"
#if !defined(PSG_HOST_TEST) || defined(KPCM_HOST_TEST)
#include "kernel_pcm.h"
#else
static int kpcm_owned(void){return 0;}
static void kpcm_release(uint32_t pid){(void)pid;}
#endif
#include "psg.h"
#include "app_abi.h"
#include "irq.h"

/* Short critical section for the queue and the owner state.  Producer is a
   syscall, consumer is the Timer C interrupt, so every mutation of q_head,
   q_tail, q_count, q_deadline, owner_pid and the Dosound pointer is wrapped.
   The same nesting is safe because psg.c's lock also saves/restores SR. */
#ifdef PSG_HOST_TEST
extern volatile uint32_t kaudio_host_locks;
volatile uint32_t kaudio_host_locked,kaudio_host_irq_points,kaudio_host_irq_locked_max;
volatile uint32_t kaudio_host_irq_unlocked,kaudio_host_irq_seen[2];
void kaudio_host_irq_point(unsigned where)
{
    ++kaudio_host_irq_points;
    if(where<32u)kaudio_host_irq_seen[where>>5]|=1u<<(where&31u);
    /* This records coverage of the critical sections at every instrumented point.
       It is NOT a simulated interrupt delivery: it proves that a real interrupt
       would be masked there, and that every point is reached with the lock held.
       A point taken outside the lock is counted separately, so a test can fail. */
    if(!kaudio_host_locked)++kaudio_host_irq_unlocked;
    if(kaudio_host_locked>kaudio_host_irq_locked_max)
        kaudio_host_irq_locked_max=kaudio_host_locked;
}
#define KAUDIO_IRQ_POINT(n) kaudio_host_irq_point(n)
static uint16_t kaudio_lock(void){++kaudio_host_locks;++kaudio_host_locked;return 0u;}
static void kaudio_unlock(uint16_t sr){(void)sr;--kaudio_host_locked;}
#else
static uint16_t kaudio_lock(void)
{
    uint16_t sr;
    __asm__ volatile("move.w %%sr,%0; ori.w #0x0700,%%sr":"=d"(sr)::"memory","cc");
    return sr;
}
static void kaudio_unlock(uint16_t sr){__asm__ volatile("move.w %0,%%sr"::"d"(sr):"memory","cc");}
#endif

/* The kernel's own event type and the app-visible one are declared separately so
   neither header drags the other in; these guards make the mirror enforced. */
_Static_assert(sizeof(kaudio_event_t)==sizeof(atos_audio_event_t),"audio event size");
_Static_assert(offsetof(kaudio_event_t,wait)==0u,"audio event wait offset");
_Static_assert(offsetof(kaudio_event_t,aux)==4u,"audio event aux offset");
_Static_assert(offsetof(kaudio_event_t,op)==6u,"audio event op offset");
_Static_assert(offsetof(kaudio_event_t,a)==7u,"audio event a offset");
_Static_assert(offsetof(kaudio_event_t,b)==8u,"audio event b offset");

/* The kernel's 12288 Hz counter (same unit as ATOS_CALL_HIRES_CLOCK). */
extern uint32_t kernel_user_hires_clock(void);

/* ATOS negative results, same numbering style as the other kernel services. */
#define KAUDIO_ERR_BUSY  (-0x3f0)
#define KAUDIO_ERR_ARG   (-0x3f1)
#define KAUDIO_ERR_FULL  (-0x3f2)
#define KAUDIO_ERR_AGAIN (-0x3f3)   /* the service could not be installed */

/* PSG volume registers and mixer bits.  On the YM2149 the mixer enables are
   ACTIVE LOW: a zero bit means "this source reaches the channel". */
#define PSG_VOL_BASE   8u
#define PSG_TONE_LO(ch) (uint8_t)((ch)*2u)
#define PSG_TONE_HI(ch) (uint8_t)((ch)*2u+1u)
#define MIX_TONE(ch)    (uint8_t)(1u<<(ch))
#define MIX_NOISE(ch)   (uint8_t)(1u<<(3u+(ch)))

#ifdef PSG_HOST_TEST
extern uint32_t kaudio_host_now(void);
static uint32_t kaudio_now(void){return kaudio_host_now();}
#else
static uint32_t kaudio_now(void){return kernel_user_hires_clock();}
#endif

static uint32_t owner_pid;
/* TOS sound daemon state (declared here because kaudio_release() also clears it). */
static const uint8_t *snd_table;
static uint32_t snd_delay;
static uint32_t tos_generation;
/* Last valid byte of the accepted table +1, and the generation the work belongs
   to.  The interpreter runs in an interrupt, so it must never read past the
   buffer the caller proved valid, and must never act on work from a session that
   is already over (the audit's P1.3 and generation-token points). */
static const uint8_t *snd_end;
static uint32_t snd_gen;
static uint32_t q_gen;          /* TOS session the queue belongs to (TOS owner only) */
static uint32_t owner_generation;  /* bumped whenever the owner changes */
static uint32_t q_owner_gen;       /* owner generation when the queue was armed */
static uint32_t q_owner;           /* pid the queue belongs to */

/* Persona hook for the hardware ports.  Registers 14/15 carry the floppy
   select, the printer strobe and the RS232 lines: in a TOS window they belong to
   the virtual port state, and the Dosound interpreter must not bypass that
   (the audit's P1.2).  Returns 1 when it handled the register. */
static int (*snd_port_sink)(uint8_t reg,uint8_t value);
void kaudio_set_port_sink(int (*fn)(uint8_t,uint8_t)){snd_port_sink=fn;}

#ifdef PSG_HOST_TEST
/* Controlled interrupt injection: the test runs code at these points and can
   assert that the critical section really is held there. */

#else
#define KAUDIO_IRQ_POINT(n) do{}while(0)
#endif
static uint8_t snd_tmp;
static unsigned owner_session;
static uint32_t q_deadline;
static uint8_t  channel_volume[KAUDIO_CHANNELS];
static uint8_t  channel_noise[KAUDIO_CHANNELS];
static uint8_t  ready;
static kaudio_event_t queue[KAUDIO_QUEUE_EVENTS];
static uint8_t  q_head,q_tail,q_count;
static uint32_t dbg_events,dbg_rejected,dbg_queued,dbg_underruns;

/* Non-static mirrors for the debugger and the emulator tests (the same pattern
   the rest of the kernel uses with kdbg_*).  The accessors are functions, so a
   symbol lookup by name would otherwise land on code. */
volatile uint32_t kaudio_state_owner,kaudio_state_events,kaudio_state_queued,
                  kaudio_state_rejected,kaudio_state_qcount;

/* Mixer enables are ACTIVE LOW: a zero bit means the source reaches the
   channel.  These wrappers exist so no call site has to remember the polarity. */
static void mixer_enable_tone(unsigned ch){psg_mixer_modify(MIX_TONE(ch),0u);}
static void mixer_disable_tone(unsigned ch){psg_mixer_modify(0u,MIX_TONE(ch));}
static void mixer_enable_noise(unsigned ch){psg_mixer_modify(MIX_NOISE(ch),0u);}
static void mixer_disable_noise(unsigned ch){psg_mixer_modify(0u,MIX_NOISE(ch));}
static void mixer_disable_all(void){psg_mixer_modify(0u,0x3fu);}

/* ---------------------------------------------------------------- silent off */

static void audio_channel_silence(unsigned ch)
{
    if(ch>=KAUDIO_CHANNELS)return;
    psg_write((uint8_t)(PSG_VOL_BASE+ch),0u);
    mixer_disable_tone(ch);
    mixer_disable_noise(ch);
    channel_volume[ch]=0u;
    channel_noise[ch]=0u;
}

/* Stop everything this core ever started.  Touches only registers 0..13 (never
   the port directions or the hardware ports) through hw/psg.c. */
static void audio_silence(void)
{
    unsigned ch;
    for(ch=0u;ch<KAUDIO_CHANNELS;++ch)audio_channel_silence(ch);
    mixer_disable_all();
    q_head=q_tail=q_count=0u;q_deadline=0u;
}

void kaudio_init(void)
{
    if(ready)return;
    ready=1u;
    audio_silence();
}

/* ------------------------------------------------------------------- effects */

static int32_t audio_apply_tone(unsigned ch,uint16_t period,uint8_t volume)
{
    if(ch>=KAUDIO_CHANNELS)return KAUDIO_ERR_ARG;
    if(period==0u||period>0xfffu)return KAUDIO_ERR_ARG;
    if(volume>15u)volume=15u;
    psg_write(PSG_TONE_LO(ch),(uint8_t)(period&0xffu));
    psg_write(PSG_TONE_HI(ch),(uint8_t)((period>>8)&0x0fu));
    psg_write((uint8_t)(PSG_VOL_BASE+ch),(uint8_t)(volume&0x0fu));
    mixer_disable_noise(ch);                     /* a tone effect has no noise */
    mixer_enable_tone(ch);
    channel_volume[ch]=volume;
    channel_noise[ch]=0u;
    ++dbg_events;
    kaudio_state_events=dbg_events;
    return 0;
}

static int32_t audio_apply_noise(uint16_t period,uint8_t volume)
{
    const unsigned ch=KAUDIO_NOISE_CHANNEL;
    if(period==0u||period>31u)return KAUDIO_ERR_ARG;
    if(volume>15u)volume=15u;
    psg_write(PSG_REG_NOISE,(uint8_t)(period&0x1fu));
    psg_write((uint8_t)(PSG_VOL_BASE+ch),(uint8_t)(volume&0x0fu));
    mixer_disable_tone(ch);                      /* noise replaces the tone */
    mixer_enable_noise(ch);
    channel_volume[ch]=volume;
    channel_noise[ch]=1u;
    ++dbg_events;
    kaudio_state_events=dbg_events;
    return 0;
}

static void audio_stop_mask(uint8_t mask)
{
    unsigned ch;
    for(ch=0u;ch<KAUDIO_CHANNELS;++ch)if(mask&(1u<<ch))audio_channel_silence(ch);
}

/* ------------------------------------------------------------------- queue */

static void audio_apply(const kaudio_event_t *e)
{
    switch(e->op){
    case KAUDIO_OP_TONE: audio_apply_tone(e->a,e->aux,e->b);break;
    case KAUDIO_OP_NOISE:audio_apply_noise(e->aux,e->b);break;
    case KAUDIO_OP_STOP: audio_stop_mask(e->a);break;
    default: ++dbg_rejected;break;
    }
}

void kaudio_pump(void)
{
    uint32_t now;uint16_t sr;
#ifndef PSG_HOST_TEST
    uint32_t t0=0u;
#endif
    if(!ready)kaudio_init();
    sr=kaudio_lock();
    if(!q_count){kaudio_unlock(sr);return;}
#ifndef PSG_HOST_TEST
    t0=kernel_user_hires_clock();
#endif
    now=kaudio_now();
    if(q_count&&(q_owner_gen!=owner_generation||
                 (q_owner==KAUDIO_TOS_OWNER&&q_gen!=tos_generation))){
        /* Work whose owner is gone (or TOS work from a finished session) must
           never be applied.  Native work survives a TOS generation change. */
        q_head=q_tail=q_count=0u;q_deadline=0u;kaudio_state_qcount=0u;
    }
    while(q_count){
        const kaudio_event_t *e=&queue[q_head];
        if((int32_t)(now-q_deadline)<0)break;          /* not due yet */
        audio_apply(e);
        q_head=(uint8_t)((q_head+1u)%KAUDIO_QUEUE_EVENTS);
        --q_count;
        kaudio_state_qcount=q_count;
        if(q_count){
            /* Cumulative schedule: the next event keeps its place on the time
               axis, so a burst does not drift.  A long stall (a stopped kernel)
               must not make us replay a whole sequence at once, hence the cap. */
            q_deadline=q_deadline+queue[q_head].wait;
            if((int32_t)(now-q_deadline)>(int32_t)KAUDIO_CATCHUP_LIMIT)q_deadline=now;
        }
    }
#ifndef PSG_HOST_TEST
    {uint32_t spent=kernel_user_hires_clock()-t0;
     if(spent>kaudio_dbg_isr_max)kaudio_dbg_isr_max=spent;}
#endif
    kaudio_unlock(sr);
}

/* The kernel loop must not be a second consumer: it only drains when the Timer C
   hook could not be installed.  kaudio_irq_ok() tells which world we are in. */
void kaudio_pump_fallback(void){if(!kaudio_irq_ok())kaudio_pump();}

int32_t kaudio_queue(uint32_t pid,const kaudio_event_t *events,uint32_t count)
{
    uint32_t i;uint16_t sr;
    if(!count||count>KAUDIO_QUEUE_EVENTS)return KAUDIO_ERR_ARG;
    sr=kaudio_lock();
    KAUDIO_IRQ_POINT(9);
    if(pid!=owner_pid||kpcm_owned()){kaudio_unlock(sr);return KAUDIO_ERR_BUSY;}
    if((uint32_t)q_count+count>KAUDIO_QUEUE_EVENTS){kaudio_unlock(sr);return KAUDIO_ERR_FULL;}
    for(i=0u;i<count;++i){
        kaudio_event_t *dst=&queue[q_tail];
        dst->wait=events[i].wait;
        dst->aux=events[i].aux;
        dst->op=events[i].op;
        dst->a=events[i].a;
        dst->b=events[i].b;
        dst->pad[0]=events[i].pad[0];
        q_tail=(uint8_t)((q_tail+1u)%KAUDIO_QUEUE_EVENTS);
    }
    /* Appending to a non-empty queue must NOT touch the deadline that the head
       already has; only the empty -> non-empty transition arms it. */
    if(q_count==0u)q_deadline=kaudio_now()+events[0].wait;
    /* The queue belongs to its OWNER, not to the TOS session: a TOS teardown
       must not invalidate a native application's queued work (audit point 1).
       Only TOS-owned work also carries the TOS session generation. */
    q_owner=owner_pid;
    q_owner_gen=owner_generation;
    q_gen=tos_generation;
    q_count=(uint8_t)(q_count+count);
    kaudio_state_qcount=q_count;
    dbg_queued+=count;
    kaudio_state_queued=dbg_queued;
    kaudio_unlock(sr);
    return 0;
}

/* ----------------------------------------------------------- ownership/state */

uint32_t kaudio_caps(void)
{
#ifdef ARK_AUDIO_OFF
    /* Test-only build for the smoothness pair: the ATOS audio service reports
       "no audio", so a client (Arkanoid checks capabilities first) never enters
       the audio path.  Switching Hatari's own output off leaves the guest audio
       running, which is why that pair could not isolate the subsystem cost. */
    return 0;
#endif
    /* Only what this build really does.  No envelope shaping, no PCM, no
       hardware DMA - and saying so is the point of this function. */
    return KAUDIO_CAP_PSG_TONE|KAUDIO_CAP_PSG_NOISE|KAUDIO_CAP_QUEUE|KAUDIO_CAP_PCM;
}

/* Claim with the queue lock already held (used by the Dosound trap). */
static int32_t kaudio_claim_locked(uint32_t pid,unsigned session)
{
    if(!pid)return KAUDIO_ERR_ARG;
    if(!owner_pid){owner_pid=pid;owner_session=session;kaudio_state_owner=pid;++owner_generation;return 0;}
    return owner_pid==pid?0:KAUDIO_ERR_BUSY;
}

int32_t kaudio_claim(uint32_t pid,unsigned session)
{
    uint16_t sr;int32_t rc;
    if(!ready)kaudio_init();
    if(!pid)return KAUDIO_ERR_ARG;
    sr=kaudio_lock();
    KAUDIO_IRQ_POINT(1);
    if(!owner_pid){
        /* First owner: attach the Timer C hook from a syscall, never from an
           interrupt, and refuse to hand out a service that cannot be driven. */
        if(!kaudio_irq_ok()){kaudio_unlock(sr);return KAUDIO_ERR_AGAIN;}
        owner_pid=pid;owner_session=session;kaudio_state_owner=pid;++owner_generation;rc=0;
    }else rc=(owner_pid==pid)?0:KAUDIO_ERR_BUSY;
    kaudio_unlock(sr);
    return rc;
}

void kaudio_release(uint32_t pid)
{
    uint16_t sr;
    if(!pid)return;
    sr=kaudio_lock();
    KAUDIO_IRQ_POINT(2);
    if(pid!=owner_pid){kaudio_unlock(sr);return;}
    /* Order matters (audit P1.4): detach the work, change the state and only then
       silence - and all of it inside one critical section, so the interrupt can
       neither resume writing into a half-cleaned session nor see a stale owner. */
    kpcm_release(pid);
    snd_table=0;snd_delay=0u;snd_tmp=0u;snd_end=0;
    q_head=q_tail=q_count=0u;q_deadline=0u;kaudio_state_qcount=0u;
    owner_pid=0u;owner_session=0u;kaudio_state_owner=0u;++owner_generation;
    audio_silence();
    kaudio_unlock(sr);
}

int32_t kaudio_tone(uint32_t pid,unsigned channel,uint16_t period,uint8_t volume)
{
    uint16_t sr;int32_t rc;
    sr=kaudio_lock();
    KAUDIO_IRQ_POINT(3);
    rc=(pid==owner_pid&&!kpcm_owned())?audio_apply_tone(channel,period,volume):KAUDIO_ERR_BUSY;
    kaudio_unlock(sr);
    return rc;
}

int32_t kaudio_noise(uint32_t pid,uint16_t period,uint8_t volume)
{
    uint16_t sr;int32_t rc;
    sr=kaudio_lock();
    KAUDIO_IRQ_POINT(4);
    rc=(pid==owner_pid&&!kpcm_owned())?audio_apply_noise(period,volume):KAUDIO_ERR_BUSY;
    kaudio_unlock(sr);
    return rc;
}

int32_t kaudio_stop(uint32_t pid,uint8_t channel_mask)
{
    uint16_t sr;int32_t rc;
    sr=kaudio_lock();
    KAUDIO_IRQ_POINT(5);
    if(pid==owner_pid&&!kpcm_owned()){audio_stop_mask(channel_mask);rc=0;}else rc=KAUDIO_ERR_BUSY;
    kaudio_unlock(sr);
    return rc;
}

/* The legacy TOS entries in one atomic operation: check the owner rule and write.
   Splitting the check from the write (may_write + later write) was the audit's
   P1.4 point - between the two a native application could claim the chip. */
int32_t kaudio_tos_reg_write(uint8_t reg,uint8_t value)
{
    uint16_t sr;int32_t rc;
    sr=kaudio_lock();
    KAUDIO_IRQ_POINT(6);
    if(owner_pid==0u)kaudio_claim_locked(KAUDIO_TOS_OWNER,0u);
    if(owner_pid!=KAUDIO_TOS_OWNER)rc=KAUDIO_ERR_BUSY;
    else{
        if(reg>=14u)rc=KAUDIO_ERR_ARG;                 /* ports are virtualised */
        else if(reg==PSG_REG_MIXER){psg_mixer_modify(0x3fu,(uint8_t)(value&0x3fu));rc=0;}
        else{psg_write_raw(reg,value);rc=0;}
    }
    kaudio_unlock(sr);
    return rc;
}

/* --------------------------------------------------------- TOS sound daemon
   Mirrors the Atari/EmuTOS Dosound contract (bios/sound.c in EmuTOS):
   the table is a run of (instruction, data) byte pairs;
     0x00..0x0f  write that PSG register,
     0x80        set the base value used by the increment command,
     0x81        increment command: register, increment, end value; it rewinds
                 itself while the value has not reached the end,
     >0x81       ends the run and the NEXT byte is a delay in Timer C ticks;
                 a delay of zero ends playback.
   Stepping runs in the Timer C interrupt, so a TOS program that never calls the
   OS still hears the sequence progress. */

volatile uint32_t kaudio_dbg_tos_teardown;
volatile uint32_t kaudio_dbg_dosound_starts,kaudio_dbg_dosound_ends,
                  kaudio_dbg_dosound_rejected,kaudio_dbg_ticks,kaudio_dbg_isr_max;

/* One register write for the interpreter.  Registers 14/15 go to the persona's
   virtual ports (or are refused when there is no sink), 0..13 respect the owner
   rule, and register 7 keeps the port direction bits. */
static void snd_write_reg(uint8_t reg,uint8_t value)
{
    if(reg>=14u){
        if(snd_port_sink&&snd_port_sink(reg,value))return;   /* virtualised */
        ++kaudio_dbg_dosound_rejected;                       /* or refused */
        return;
    }
    if(owner_pid!=0u&&owner_pid!=KAUDIO_TOS_OWNER){
        /* A native application took the chip: stop the TOS sequence instead of
           writing over it. */
        snd_table=0;snd_delay=0u;++kaudio_dbg_dosound_ends;
        return;
    }
    if(reg==PSG_REG_MIXER)psg_mixer_modify(0x3fu,(uint8_t)(value&0x3fu));
    else psg_write_raw(reg,value);
}

static void snd_step(void)
{
    const uint8_t *code=snd_table;
    uint8_t instr=0u,data;
    if(!code)return;
    /* Work from a finished session must not run at all. */
    if(snd_gen!=tos_generation){snd_table=0;snd_delay=0u;return;}
    if(snd_delay){--snd_delay;return;}
    /* A bounded run, and every byte is checked against the accepted range BEFORE
       it is read: an interrupt must not fault on a bad table, and must not walk
       off the end of the buffer (the audit's P1.3). */
    {unsigned guard=0u;
     while(guard++<256u){
        if(code+2>snd_end){snd_table=0;snd_delay=0u;++kaudio_dbg_dosound_ends;return;}
        instr=*code++;
        data=*code++;
        if(instr==0x80u){snd_tmp=data;continue;}
        if(instr>0x80u){--code;break;}       /* only the data byte was extra: TOS
                                                leaves the pointer just after the
                                                instruction, and the delay byte
                                                (or the 0x81 operands) follows it */
        snd_write_reg(instr,data);
        if(snd_table==0)return;                              /* refused: stopped */
     }
     if(guard>256u){snd_table=0;snd_delay=0u;++kaudio_dbg_dosound_ends;return;}}
    if(instr==0x81u){
        uint8_t reg,inc,end;
        if(code+3>snd_end){snd_table=0;snd_delay=0u;++kaudio_dbg_dosound_ends;return;}
        reg=*code++;inc=*code++;end=*code++;
        snd_tmp=(uint8_t)(snd_tmp+inc);
        snd_write_reg(reg,snd_tmp);
        if(snd_table==0)return;
        if(snd_tmp!=end)code-=4u;      /* rewind, exactly like TOS */
    }else{
        if(code+1>snd_end){snd_table=0;snd_delay=0u;++kaudio_dbg_dosound_ends;return;}
        snd_delay=*code++;
        if(!snd_delay){code=0;++kaudio_dbg_dosound_ends;}   /* end of playback */
    }
    snd_table=code;
}

void kaudio_timer_tick(void)
{
#ifdef ARK_AUDIO_OFF
    return;                      /* no periodic audio work at all */
#endif
    /* The whole callback is measured, including the Dosound interpreter, which
       can perform up to 256 register writes under IPL 7 (the audit: measuring
       only the queue hid that cost). */
#ifndef PSG_HOST_TEST
    uint32_t t0=kernel_user_hires_clock();
#endif
    ++kaudio_dbg_ticks;
    if(snd_table){uint16_t sr=kaudio_lock();snd_step();kaudio_unlock(sr);}
    kaudio_pump();          /* queued effects are time driven, not frame driven */
#ifndef PSG_HOST_TEST
    {uint32_t spent=kernel_user_hires_clock()-t0;
     if(spent>kaudio_dbg_isr_max)kaudio_dbg_isr_max=spent;}
#endif
}

#ifndef PSG_HOST_TEST
static void kaudio_irq_handler(irq_source_t source,void *context)
{
    (void)source;(void)context;
    kaudio_timer_tick();
}
#endif

#ifdef PSG_HOST_TEST
/* The host has no interrupt controller: the test drives kaudio_timer_tick()
   itself, and can force a failure to check that Dosound is then refused. */
volatile uint32_t kaudio_host_irq_ok=1u;
int kaudio_irq_attach(void){return (int)kaudio_host_irq_ok;}
int kaudio_irq_ok(void){return (int)kaudio_host_irq_ok;}
#else
int kaudio_irq_attach(void)
{
    static int8_t attached=-1;      /* -1 = not tried, 0 = failed, 1 = installed */
    if(attached<0)
        attached=irq_register(IRQ_SRC_MFP_C,kaudio_irq_handler,0)?1:0;
    return attached;
}
int kaudio_irq_ok(void){return kaudio_irq_attach()==1;}
#endif
void kaudio_init_audio(void){ (void)kaudio_irq_attach(); }

const uint8_t *kaudio_dosound_span(const uint8_t *table,const uint8_t *end)
{
    const uint8_t *old;uint16_t sr;
    sr=kaudio_lock();
    KAUDIO_IRQ_POINT(8);
    old=snd_table;
    if(!table){
        /* NULL stops interpreting the table.  It does NOT silence the PSG: the
           registers keep whatever they hold, which is what TOS does too. */
        snd_table=0;snd_delay=0u;snd_end=0;
    }else if((int32_t)(intptr_t)table<0){
        /* Negative pointer is a QUERY: return the current interpreter pointer and
           change nothing - not the table, not the delay (EmuTOS dosound()). */
    }else{
        if(!kaudio_irq_ok()){
            /* Check availability BEFORE taking the chip: refusing and still
               holding the owner would lock out the next native client (audit
               point 3). */
            ++kaudio_dbg_dosound_rejected;
        }else if(kaudio_claim_locked(KAUDIO_TOS_OWNER,0u)<0){
            ++kaudio_dbg_dosound_rejected;
        }else{
            snd_table=table;snd_end=end?end:(const uint8_t *)(uintptr_t)~0ul;
            snd_delay=0u;snd_tmp=0u;snd_gen=tos_generation;
            ++kaudio_dbg_dosound_starts;
        }
    }
    kaudio_unlock(sr);
    return old;
}

/* A caller is about to free or shrink a buffer it owns.  If the interpreter is
   reading inside that block, either clamp its end (shrink) or stop it (free):
   an interrupt must never walk memory that is being returned or handed to
   somebody else.  Atomic with respect to the Timer C consumer. */
void kaudio_dosound_revalidate(const uint8_t *old_base,const uint8_t *old_end,
                               const uint8_t *new_end)
{
    uintptr_t lo=(uintptr_t)old_base,hi=(uintptr_t)old_end,ne=(uintptr_t)new_end;
    uint16_t sr;
    sr=kaudio_lock();
    KAUDIO_IRQ_POINT(10);
    /* Only the allocation that is being changed may be touched.  Checking just
       "snd_table >= lo" stopped a table living in an UNRELATED block above the
       changed one (audit R13 point 2).  A failed operation never reaches here:
       the callers return before this on the error paths. */
    if(snd_table&&lo&&hi>lo&&(uintptr_t)snd_table>=lo&&(uintptr_t)snd_table<hi){
        if(ne<=lo||(uintptr_t)snd_table>=ne){
            /* The interpreter itself, or the bytes ahead of it, are gone. */
            snd_table=0;snd_delay=0u;snd_end=0;
            ++kaudio_dbg_dosound_ends;
        }else if((uintptr_t)snd_end>ne){
            snd_end=(const uint8_t *)ne;           /* truncate the walk range */
        }
    }
    kaudio_unlock(sr);
}

const uint8_t *kaudio_dosound(const uint8_t *table)
{
    return kaudio_dosound_span(table,0);       /* unbounded: host tests, legacy */
}

/* Tear the TOS audio session down.  Order matters: the table pointer must stop
   being visible to the Timer C consumer BEFORE the launcher releases the TPA,
   otherwise the ISR would walk freed or reused memory.  The lock makes that
   transition atomic against the consumer. */
uint32_t kaudio_tos_teardown(void)
{
    uint32_t gen;uint16_t sr;
    sr=kaudio_lock();
    KAUDIO_IRQ_POINT(7);
    /* The interpreter state is TOS's own: always detach it, and always detach it
       before the launcher releases the TPA. */
    snd_table=0;snd_delay=0u;snd_tmp=0u;snd_end=0;
    if(owner_pid==KAUDIO_TOS_OWNER){
        /* Queue and channels belong to the owner of the chip, so only the TOS
           domain is released here.  A native application's queued work - say
           Arkanoid's scheduled stop - must survive a TOS program (audit P1.1). */
        unsigned ch;
        q_head=q_tail=q_count=0u;q_deadline=0u;kaudio_state_qcount=0u;
        for(ch=0u;ch<KAUDIO_CHANNELS;++ch){
            psg_write_raw((uint8_t)(8u+ch),0u);
            psg_mixer_modify(0u,(uint8_t)((1u<<ch)|(1u<<(3u+ch))));
        }
        owner_pid=0u;owner_session=0u;kaudio_state_owner=0u;++owner_generation;
    }
    gen=++tos_generation;      /* invalidates any work left from this session */
    kaudio_unlock(sr);
    ++kaudio_dbg_tos_teardown;
    return gen;
}

uint32_t kaudio_tos_generation(void){return tos_generation;}

int kaudio_tos_may_write(void)
{
    return owner_pid==0u||owner_pid==KAUDIO_TOS_OWNER;
}

uint32_t kaudio_dosound_state(void){return snd_table?1u:0u;}
uint32_t kaudio_dosound_delay(void){return snd_delay;}

void kaudio_ongibit(uint8_t mask){psg_port_a_modify(0u,mask);}
void kaudio_offgibit(uint8_t mask){psg_port_a_modify((uint8_t)~mask,0u);}

uint32_t kaudio_owner(void){return owner_pid;}
uint32_t kaudio_dbg_events(void){return dbg_events;}
uint32_t kaudio_dbg_rejected(void){return dbg_rejected;}
uint32_t kaudio_dbg_queued(void){return dbg_queued;}
uint32_t kaudio_dbg_underruns(void){return dbg_underruns;}
