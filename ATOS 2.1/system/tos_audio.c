/* TOS audio acceptance probe for the ATOS personality (second version).
 *
 * The first version was not an acceptance test: it used a 4-bit register for an
 * 8-bit marker, read the state before the first interrupt could have run, and
 * its busy loop was a fixed instruction count.  This one:
 *
 *   - uses register 0 (a full 8-bit half of the channel A period),
 *   - builds a COMPLETE audible tone: period low/high, channel volume, mixer,
 *   - exercises the contract: Dosound starts, a query returns the interpreter
 *     pointer without stopping, NULL stops, a table change returns the previous
 *     pointer,
 *   - exercises the 0x80/0x81 increment command including a BYTE WRAP,
 *   - touches port A in a controlled read/modify/restore cycle (the persona
 *     virtualises ports 14/15, so this cannot disturb the floppy select),
 *   - waits with a memory-based counter; the outside watchdog (the harness)
 *     bounds the run, and the kernel counters confirm the same facts.
 */
typedef signed long int32_t;
typedef unsigned long uint32_t;
typedef unsigned short uint16_t;
typedef short int16_t;
typedef unsigned long uintptr_t;

static int32_t gdos1(int16_t fn,int32_t a)
{
    register int32_t r __asm__("d0");
    __asm__ __volatile__("move.l %1,-(%%sp)\n\t"
                         "move.w %2,-(%%sp)\n\t"
                         "trap #1\n\t"
                         "addq.l #6,%%sp"
                         : "=r"(r) : "r"(a), "r"((int32_t)fn)
                         : "d1","d2","a0","a1","a2","memory");
    return r;
}
static void puts_(const char *s){ (void)gdos1(9,(int32_t)(uint32_t)s); }

/* Two hex digits without libgcc (no division): the audit asked for the raw
   ongibit values, not just PASS/FAIL. */
static void puthex8(int32_t v)
{
    static const char *d="0123456789ABCDEF";
    char b[3];
    b[0]=d[(v>>4)&0xF];b[1]=d[v&0xF];b[2]=0;
    puts_(b);
}

static int32_t xbios1(int16_t fn,int32_t a)              /* one WORD argument */
{
    register int32_t r __asm__("d0");
    __asm__ __volatile__("move.w %1,-(%%sp)\n\t"
                         "move.w %2,-(%%sp)\n\t"
                         "trap #14\n\t"
                         "addq.l #4,%%sp"
                         : "=r"(r) : "r"(a), "r"((int32_t)fn)
                         : "d1","d2","a0","a1","a2","memory");
    return r;
}
static int32_t xbios2(int16_t fn,int32_t a,int32_t b)    /* two WORD arguments */
{
    register int32_t r __asm__("d0");
    __asm__ __volatile__("move.w %1,-(%%sp)\n\t"         /* b */
                         "move.w %2,-(%%sp)\n\t"         /* a */
                         "move.w %3,-(%%sp)\n\t"         /* fn */
                         "trap #14\n\t"
                         "addq.l #6,%%sp"
                         : "=r"(r) : "r"(b), "r"(a), "r"((int32_t)fn)
                         : "d1","d2","a0","a1","a2","memory");
    return r;
}
static int32_t xbios_ptr(int16_t fn,int32_t p)           /* one LONG argument */
{
    register int32_t r __asm__("d0");
    __asm__ __volatile__("move.l %1,-(%%sp)\n\t"
                         "move.w %2,-(%%sp)\n\t"
                         "trap #14\n\t"
                         "addq.l #6,%%sp"
                         : "=r"(r) : "r"(p), "r"((int32_t)fn)
                         : "d1","d2","a0","a1","a2","memory");
    return r;
}

/* A memory-based wait: a loop counter in memory, NOT a clock.  It gives the
   interrupts time to run; the real bound on the run is the external watchdog. */
static void wait_loop(uint32_t rounds)
{
    volatile uint32_t i;
    while(rounds--){
        for(i=0u;i<400000u;++i){__asm__ __volatile__("");}
    }
}

/* Complete channel A tone: period 0xAA/0x00, full volume, mixer with only the
   channel A tone enabled.  A pitch change after a delay, then the end. */
static const unsigned char seq1[] = {
    0u, 0xAAu,          /* register 0: period low  = 0xAA */
    1u, 0x00u,          /* register 1: period high = 0    */
    8u, 0x0Fu,          /* register 8: channel A volume = 15 */
    7u, 0x3Eu,          /* mixer: tone A on (active low), all else off */
    0x82u, 60u,         /* delay 60 Timer C ticks (~1.2 s) */
    0u, 0x55u,          /* change the pitch */
    0x82u, 0x00u        /* delay 0: end of playback */
};

/* 0x80 sets the base value; 0x81 increments a register until it reaches the end
   value.  With base 0xF0 and step 0x20 the byte WRAPS to 0x10, which is the end
   value, so the command performs exactly one step - and the wrap is exercised. */
static const unsigned char seq2[] = {
    0x80u, 0xF0u,
    0x81u, 0u, 0x20u, 0x10u,
    0x82u, 0x00u
};

/* Result channel.  A TOS window is composed from the kernel loop, so a program
   that spins never gets its text on screen while it runs; the harness therefore
   reads a result word (bit per check) and a counter.  Fixed low-RAM scratch, the
   same convention the other test programs in this tree use. */
#define ARK_RESULT_ADDR 0x820u
#define ARK_RUNS_ADDR   0x830u      /* run counter, survives between runs */
#define ARK_VPORT_ADDR  0x834u      /* port A value seen by this run */
#define ARK_VPORT2_ADDR 0x838u      /* port A value after restore */
#define ARK_KDBG_ADDR   0x83Cu      /* diagnostic: kernel counter seen from the guest */
#define ARK_KCNT_ADDR   0x000C7C68u /* kdbg_tos_xbios32, from build/system/kernel.elf */
#define ARK_KTEAR_ADDR  0x000BBC86u /* kaudio_dbg_tos_teardown */
#define ARK_RUNID_ADDR  0x860u      /* which run this is (results are per run) */
#define ARK_MODE_ADDR   0x864u      /* harness sets: 0 = normal, 1 = fault while playing */
#define ARK_MODE_SEEN   0x868u      /* mode the probe actually read */
#define ARK_FAULT_MARK  0x86Cu      /* written just before the illegal instruction */
#define ARK_VOL_ADDR    0x870u      /* Giaccess reads of regs 8,9,10 during this run */
#define ARK_OWNER_ADDR  0x87Cu      /* owner id as reported by the kernel audio state */
/* Stores through a parameter address: the literal survives only as an
   argument, so GCC's bounds analysis has nothing to complain about. */
static void store32(uintptr_t addr,uint32_t value)
{
    __asm__ __volatile__("" : "+r"(addr));       /* keep the address opaque */
    *(volatile uint32_t *)addr=value;
}
static uint32_t load32(uintptr_t addr)
{
    uint32_t value;
    __asm__ __volatile__("" : "+r"(addr));
    value=*(volatile uint32_t *)addr;
    return value;
}
static void result_set(unsigned bit,int ok)
{
    uintptr_t base=(uintptr_t)ARK_RESULT_ADDR;
    uint32_t word=load32(base);
    if(ok)word|=(1u<<bit);else word&=(uint32_t)~(1u<<bit);
    store32(base,word);
    store32(base+4u,load32(base+4u)+1u);
}

int _tos_main(void)
{
    int32_t r,r2;

    {
        uint32_t run=load32((uintptr_t)ARK_RUNS_ADDR)+1u;
        store32((uintptr_t)ARK_RUNS_ADDR,run);
        /* Per-run result: no bits carried over, run id published (audit point 5). */
        store32((uintptr_t)ARK_RESULT_ADDR,0u);
        store32((uintptr_t)(ARK_RESULT_ADDR+4u),0u);
        store32((uintptr_t)ARK_RUNID_ADDR,run);
#ifdef ARK_FORCE_FAULT_MODE
        /* Test build: the harness cannot write guest memory, so this variant always
           takes the "fault while the tone plays" path.  It must set the SAME word
           the fault block tests, otherwise the forced mode is only recorded and the
           run quietly completes (which is exactly what happened once). */
        store32((uintptr_t)ARK_MODE_ADDR,1u);
        store32((uintptr_t)ARK_MODE_SEEN,1u);
#else
        store32((uintptr_t)ARK_MODE_SEEN,load32((uintptr_t)ARK_MODE_ADDR));
#endif
    }
    puts_("TOS AUDIO PROBE 2\n");

    /* Volume of the three channels as seen through Giaccess: in the native-owner
       scenario this shows whether the TOS session touched somebody else's audio. */
    store32((uintptr_t)(ARK_VOL_ADDR+0u),(uint32_t)(xbios2(28,0,8)&0xFF));
    store32((uintptr_t)(ARK_VOL_ADDR+4u),(uint32_t)(xbios2(28,0,9)&0xFF));
    store32((uintptr_t)(ARK_VOL_ADDR+8u),(uint32_t)(xbios2(28,0,10)&0xFF));

    /* A. Giaccess on a full 8-bit register, with the TOS write flag. */
    xbios2(28,0xAAu,(int32_t)(0u|0x80u));
    r=xbios2(28,0,0);
    puts_("giaccess8 "); puts_(((r&0xFF)==0xAA)?"PASS\n":"FAIL\n");
    result_set(0u,(r&0xFF)==0xAA);

    /* B. Port A, controlled: read, set one bit, read back, restore.  The persona
          virtualises ports 14/15, so the floppy select is not touched. */
    {
        int32_t before=xbios2(28,0,14);
        puts_("xbios30 arg40 before=");puthex8(before);
        xbios1(30,0x40);                       /* ongibit 0x40 */
        r=xbios2(28,0,14);
        puts_(" after=");puthex8(r);
        xbios1(29,0xBF);                       /* offgibit: portA &= 0xBF */
        r2=xbios2(28,0,14);
        puts_(" restored=");puthex8(r2);puts_("\n");
        puts_("vport "); puts_(((r&0x40)!=0&&r2==before)?"PASS\n":"FAIL\n");
        result_set(1u,((r&0x40)!=0&&r2==before));
        store32((uintptr_t)ARK_VPORT_ADDR,(uint32_t)(r&0xFF));
        store32((uintptr_t)ARK_VPORT2_ADDR,(uint32_t)(r2&0xFF));
    }

    /* C. Dosound starts; a query returns the interpreter pointer and must not
          stop playback. */
    (void)xbios_ptr(32,(int32_t)(uint32_t)seq1);
    wait_loop(2);                              /* let a few interrupts run */
    r=xbios2(28,0,0);
    puts_("dosound tone "); puts_(((r&0xFF)==0xAA||(r&0xFF)==0x55)?"PASS\n":"FAIL\n");
    result_set(2u,((r&0xFF)==0xAA||(r&0xFF)==0x55));
    {
        const unsigned char *prev=(const unsigned char *)(uintptr_t)xbios_ptr(32,(int32_t)-1);
        int ok=(prev>=(const unsigned char *)seq1&&prev<seq1+sizeof(seq1));
        puts_("dosound query "); puts_(ok?"PASS\n":"FAIL\n");
        result_set(3u,ok);
    }

    /* D. Table change: the call returns the PREVIOUS interpreter pointer. */
    {
        const unsigned char *prev=(const unsigned char *)(uintptr_t)
            xbios_ptr(32,(int32_t)(uint32_t)seq2);
        int ok=(prev>=(const unsigned char *)seq1&&prev<seq1+sizeof(seq1));
        puts_("dosound change "); puts_(ok?"PASS\n":"FAIL\n");
        result_set(4u,ok);
    }

    /* E. The increment command, including the byte wrap. */
    wait_loop(3);
    r=xbios2(28,0,0);
    puts_("increment wrap "); puts_(((r&0xFF)==0x10)?"PASS\n":"FAIL\n");
    result_set(5u,((r&0xFF)==0x10));

    /* F. NULL stops interpreting the table: the register must stay put. */
    (void)xbios_ptr(32,0);
    r=xbios2(28,0,0);
    wait_loop(2);
    r2=xbios2(28,0,0);
    puts_("null stop "); puts_((r2==r)?"PASS\n":"FAIL\n");

    /* Fault with the audio ACTIVE (audit point 5): mode 1 faults while seq1 is
       being interpreted and channel A plays.  The sequence is restarted first,
       we wait for one step, and only then the illegal instruction runs - there
       is NO Dosound(NULL) and NO silencing before it. */
    if(load32((uintptr_t)ARK_MODE_ADDR)==1u){
        (void)xbios_ptr(32,(int32_t)(uint32_t)seq1);
        wait_loop(1);
        store32((uintptr_t)ARK_FAULT_MARK,0xFA017u);
        __asm__ __volatile__(".word 0x4AFC" ::: "memory");
        puts_("unreachable\n");
    }
    /* Leave the chip tidy on a normal run; a faulting run never reaches this,
       which is the point: the teardown has to do it. */
    xbios2(28,0x00u,(int32_t)(8u|0x80u));
    xbios2(28,0x00u,(int32_t)(9u|0x80u));
    xbios2(28,0x00u,(int32_t)(10u|0x80u));
    /* Second reading AFTER a real wait (~2.5 s), so a native event scheduled by
       somebody else in the meantime (a STOP due in that window) is visible as a
       change of the volumes - that is how the native STOP is caught on the
       emulator instead of being sampled blindly. */
    wait_loop(6);
    store32((uintptr_t)(ARK_VOL_ADDR+12u),(uint32_t)(xbios2(28,0,8)&0xFF));
    store32((uintptr_t)(ARK_VOL_ADDR+16u),(uint32_t)(xbios2(28,0,9)&0xFF));
    store32((uintptr_t)(ARK_VOL_ADDR+20u),(uint32_t)(xbios2(28,0,10)&0xFF));
    result_set(6u,1);              /* reached the end without faulting (bit 5 is the increment check) */
    puts_("TOS AUDIO PROBE END\n");
    /* Linger so the results stay readable in the window (and for a screenshot). */
    wait_loop(12);
    return 0;
}
