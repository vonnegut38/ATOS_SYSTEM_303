/* First real .PRG test for the TOS personality (tests/prg/README.md asked for
 * exactly this: a program that prints PASS/FAIL).
 *
 * It is a plain 0x601A TOS program: the tool prepends the header, _tos_start
 * calls _tos_main and then exits with Pterm(0).  Every check goes through the
 * GEMDOS trap so a failure tells us which part of the compatibility layer is
 * missing rather than just "it crashed".
 *
 * GEMDOS ABI: arguments are pushed left to right, the function number goes on
 * top as a WORD, then trap #1; the result is returned in d0.
 */
typedef signed long int32_t;
typedef unsigned long uint32_t;
typedef unsigned short uint16_t;
typedef short int16_t;

static int32_t gdos1(int16_t fn,int32_t a)
{
    register int32_t r __asm__("d0");
    __asm__ __volatile__("move.l %1,-(%%sp)\n\t"
                         "move.w %2,-(%%sp)\n\t"
                         "trap #1\n\t"
                         "addq.l #6,%%sp"
                         : "=r"(r)
                         : "r"(a), "r"((int32_t)fn)
                         : "d1","d2","a0","a1","a2","memory");
    return r;
}
static int32_t gdos3(int16_t fn,int32_t a,int32_t b,int32_t c) /* c first; a is a WORD */
{
    register int32_t r __asm__("d0");
    __asm__ __volatile__("move.l %1,-(%%sp)\n\t"   /* c (buf, LONG) */
                         "move.l %2,-(%%sp)\n\t"   /* b (count, LONG) */
                         "move.w %3,-(%%sp)\n\t"   /* a (handle, WORD) */
                         "move.w %4,-(%%sp)\n\t"   /* fn (WORD) */
                         "trap #1\n\t"
                         "add.l #12,%%sp"
                         : "=r"(r)
                         : "r"(c), "r"(b), "r"(a), "r"((int32_t)fn)
                         : "d1","d2","a0","a1","a2","memory");
    return r;
}
static int32_t gdos_fopen(const char *name,int16_t mode) /* [name LONG][mode WORD] */
{
    register int32_t r __asm__("d0");
    __asm__ __volatile__("move.w %1,-(%%sp)\n\t"   /* mode WORD */
                         "move.l %2,-(%%sp)\n\t"   /* name LONG */
                         "move.w %3,-(%%sp)\n\t"   /* fn WORD */
                         "trap #1\n\t"
                         "addq.l #8,%%sp"
                         : "=r"(r)
                         : "r"((int32_t)mode), "r"((int32_t)(uint32_t)name), "r"((int32_t)0x3d)
                         : "d1","d2","a0","a1","a2","memory");
    return r;
}
static int32_t gdos_fclose(int16_t handle) /* [handle WORD] */
{
    register int32_t r __asm__("d0");
    __asm__ __volatile__("move.w %1,-(%%sp)\n\t"   /* handle WORD */
                         "move.w %2,-(%%sp)\n\t"   /* fn WORD */
                         "trap #1\n\t"
                         "addq.l #4,%%sp"
                         : "=r"(r)
                         : "r"((int32_t)handle), "r"((int32_t)0x3e)
                         : "d1","d2","a0","a1","a2","memory");
    return r;
}
static void puts_(const char *s){ (void)gdos1(9,(int32_t)(uint32_t)s); }
static void num(int32_t v)
{
    /* No libgcc in a .PRG, so build the digits by repeated subtraction instead
       of / and % (which would pull in __udivsi3/__umodsi3). */
    char b[12];int i=0,j;uint32_t u;
    if(v<0){puts_("-");u=(uint32_t)(-v);}else u=(uint32_t)v;
    if(!u){puts_("0");return;}
    while(u&&i<11){
        uint32_t d=0;
        while(u>=10u){u-=10u;++d;}
        b[i++]=(char)('0'+u);u=d;
    }
    for(j=0;j<i/2;++j){char c=b[j];b[j]=b[i-1-j];b[i-1-j]=c;}
    b[i]=0;puts_(b);
}
/* A file the image always has: the kernel itself. */
static const char target[]="\\SYSTEM\\KERNEL.SYS";
static const char target2[]="SYSTEM\\KERNEL.SYS";

int _tos_main(void)
{
    int32_t r;int fails=0,mask=0;
    /* Fresh-run protocol for the harness.  Order matters:
         1. DONE (0x818) = 0        - a stale PASS/FAIL can never be read as the
                                      result of a run that has not finished
         2. run counter (0x81C)++   - proves THIS entry happened at all
         3. START sentinel (0x814)  - written before any check
       The harness requires a new counter value AND a new DONE afterwards. */
    {void *d=(void *)0x818u; __asm__ volatile("move.l %1,%0@" : : "a"(d), "d"(0u) : "memory");}
    /* Constant-address casts trip -Werror=array-bounds, so the counter also goes
       through inline asm (same as the markers below). */
    {uint32_t n;void *c=(void *)0x81Cu;
     __asm__ volatile("move.l %1@,%0" : "=d"(n) : "a"(c) : "memory");
     n=(n==0xFFFFFFFFu)?1u:n+1u;
     __asm__ volatile("move.l %1,%0@" : : "a"(c), "d"(n) : "memory");}
    {void *d=(void *)0x814u; __asm__ volatile("move.l %1,%0@" : : "a"(d), "d"((uint32_t)0x5A5AA5A5u) : "memory");}

    puts_("TOSPROBE: GEMDOS compatibility\r\n");

    /* 1. Sversion (0x30): major in the low byte, minor in the high byte */
    r=gdos1(0x30,0);
    puts_("  Sversion = 0x");num(r&0xff);puts_(".");num((r>>8)&0xff);
    if((r&0xff)==0||(r&0xff)==0xff){puts_("  FAIL\r\n");++fails;mask|=1;}
    else puts_("  PASS\r\n");

    /* 2. Malloc (0x48) / Mfree (0x49) - the GEMDOS heap */
    r=gdos1(0x48,4096);
    puts_("  Malloc(4096) = ");num(r);
    if(r>0){
        int32_t f=gdos1(0x49,r);
        puts_("  Mfree=");num(f);
        if(f==0){puts_("  PASS\r\n");}else{puts_("  FAIL\r\n");++fails;mask|=2;}
    }else{puts_("  FAIL\r\n");++fails;mask|=2;}

    /* 3. Fopen (0x3D) / Fread (0x3F) / Fclose (0x3E) */
    r=gdos_fopen(target,0);
    if(r<0)r=gdos_fopen(target2,0);   /* path form may differ */
    {void *d=(void *)0x820u; __asm__ volatile("move.l %1,%0@" : : "a"(d), "d"((uint32_t)r) : "memory");}
    puts_("  Fopen(KERNEL.SYS) = ");num(r);
    if(r>=0){
        char buf[16];int32_t got=gdos3(0x3f,r,16,(int32_t)(uint32_t)buf);
        int32_t cl=gdos_fclose((int16_t)r);
        puts_("  Fread=");num(got);puts_("  Fclose=");num(cl);
        if(got>0&&cl==0){puts_("  PASS\r\n");}else{puts_("  FAIL\r\n");++fails;mask|=4;}
    }else{puts_("  FAIL\r\n");++fails;mask|=4;}

    /* 4. Fattrib/current dir sanity via Dgetdrv (0x19) */
    r=gdos1(0x19,0);
    puts_("  Dgetdrv = ");num(r);
    if(r>=0&&r<26){puts_("  PASS\r\n");}else{puts_("  FAIL\r\n");++fails;mask|=8;}

    if(!fails)puts_("TOSPROBE: ALL PASS\r\n");
    else{puts_("TOSPROBE: FAILURES = ");num(fails);puts_("\r\n");}
    /* Publish the result to fixed scratch for the harness:
       0x810 = failure bitmask (0 = all pass; 1=Sversion 2=Malloc 4=Fopen 8=Dgetdrv)
       0x818 = DONE: 0x900D PASS / 0xBAAD FAIL (written ONLY on completion) */
    {void *m=(void *)0x810u;void *s=(void *)0x818u;
     __asm__ volatile("move.l %1,%0@" : : "a"(m), "d"((uint32_t)mask) : "memory");
     __asm__ volatile("move.l %1,%0@" : : "a"(s), "d"((uint32_t)(fails?0xBAADu:0x900Du)) : "memory");}
    return fails;
}
