/* Directory-read probe: does the standard GEMDOS API list a directory?
 *
 * Uses Dsetpath, Dgetpath, Fsfirst and Fsnext exactly as a TOS program does
 * (function word on the stack, arguments after it), reads the results from the
 * DTA, and stores them at the fixed TPA address 0x04004100 for the host.
 */
typedef signed long int32_t;
typedef unsigned long uint32_t;
typedef unsigned short uint16_t;
typedef short int16_t;
typedef unsigned long uintptr_t;
typedef unsigned char uint8_t;

#define RES ((volatile uint32_t *)(uintptr_t)0x04004100UL)
/* Our own DTA, installed with Fsetdta(0x1A): the real DTA address is
   whatever the basepage says, so the program must set one it can read. */
static uint8_t my_dta[64];
#define DTA ((const uint8_t *)(uintptr_t)my_dta)

static int32_t gemdos(int16_t fn,int32_t a1,int16_t a2)
{
    register int32_t r __asm__("d0");
    __asm__ __volatile__("move.w %3,-(%%sp)\n\t"   /* attr (word)   */
                         "move.l %1,-(%%sp)\n\t"   /* path (long)   */
                         "move.w %2,-(%%sp)\n\t"   /* function word */
                         "trap #1\n\t"
                         "addq.l #8,%%sp\n\t"      /* caller pops 2+4+2 */
                         "move.l %%d0,%0"
                         : "=&r"(r)
                         : "r"(a1), "r"((int32_t)fn), "r"((int32_t)a2)
                         : "d1","d2","a0","a1","memory");
    return r;
}

static void copy_str(volatile uint32_t *dst,const char *src,int words)
{
    int i;
    for(i=0;i<words;i++){
        unsigned char c1=(unsigned char)src[i*2],c2=(unsigned char)src[i*2+1];
        dst[i]=((uint32_t)c1<<8)|c2;
    }
}

int _tos_main(void)
{
    static char path[96];
    static char pattern[96];
    int32_t rc;
    int count=0;
    unsigned i;

    for(i=0;i<48u;i++)RES[i]=0;

    /* 1) set the current directory through the standard API */
    {const char *p="\\SYSTEM\\TEST";
     for(i=0;i<80u&&p[i];++i)path[i]=p[i];
     path[i]=0;}
    RES[0]=1u;
    rc=gemdos(0x3b,(int32_t)(uintptr_t)path,0);          /* Dsetpath */
    RES[1]=(uint32_t)rc;

    /* 2) ask for it back */
    for(i=0;i<96u;i++)path[i]=0;
    rc=gemdos(0x47,(int32_t)(uintptr_t)path,0);          /* Dgetpath */
    RES[2]=(uint32_t)rc;
    copy_str(&RES[3],path,10);

    /* 3) install our own DTA, then list the directory */
    RES[11]=1u;
    rc=gemdos(0x1a,(int32_t)(uintptr_t)my_dta,0);        /* Fsetdta */
    RES[12]=(uint32_t)rc;

    {const char *p="\\SYSTEM\\TEST\\*.*";
     for(i=0;i<80u&&p[i];++i)pattern[i]=p[i];
     pattern[i]=0;}
    rc=gemdos(0x4e,(int32_t)(uintptr_t)pattern,0x17);    /* Fsfirst: all + dirs */
    RES[13]=(uint32_t)rc;
    while(rc==0&&count<64){
        /* DTA: 21 reserved, attr, time, date, size(4), name(14) */
        uint32_t name0=((uint32_t)DTA[30]<<24)|((uint32_t)DTA[31]<<16)|
                       ((uint32_t)DTA[32]<<8)|(uint32_t)DTA[33];
        uint32_t size=((uint32_t)DTA[26]<<24)|((uint32_t)DTA[27]<<16)|
                      ((uint32_t)DTA[28]<<8)|(uint32_t)DTA[29];
        if(count<8){
            RES[14+count*2]=(uint32_t)DTA[21];           /* attribute */
            RES[15+count*2]=size;
            RES[30+count]=name0;                          /* first 4 name bytes */
        }
        ++count;
        rc=gemdos(0x4f,0,0);                              /* Fsnext */
    }
    RES[10]=(uint32_t)count;
    RES[47]=0x600d600du;
    /* Exit through the standard call, so the round test can run this program
       several times and check for left-over vectors/locks/pointers. */
    gemdos(0x00,0,0);                                    /* Pterm0 */
    for(;;){}
    return 0;
}
