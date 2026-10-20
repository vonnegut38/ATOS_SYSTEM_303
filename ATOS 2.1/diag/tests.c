#include "diagnostics.h"
#include "hardware.h"
#include "system.h"
#include "boot_protocol.h"

uint8_t cpu_detect_type(void)
{
    if (cpu_probe_pcr()) return CPU_68060;
    if (cpu_probe_urp()) return CPU_68040;
    if (cpu_probe_tc()) return CPU_68030;
    if (cpu_probe_extb()) return cpu_probe_030_cacr()?CPU_68030:CPU_68020;
    if (cpu_probe_vbr()) return CPU_68010;
    return CPU_68000;
}
extern int cpu_probe_fpu(void);
uint8_t cpu_detect_fpu(uint8_t cpu_type)
{
    if (!cpu_probe_fpu()) return BOOT_FPU_NONE;
    return cpu_type >= CPU_68040 ? BOOT_FPU_INTEGRATED : BOOT_FPU_EXTERNAL;
}
const char *cpu_type_name(uint8_t type)
{
    if (type==CPU_68060) return "MC68060";
    if (type==CPU_68040) return "MC68040";
    if (type==CPU_68030) return "MC68030";
    if (type==CPU_68020) return "MC68020";
    if (type==CPU_68010) return "MC68010";
    return "MC68000";
}
const char *diag_result_name(enum diag_result r)
{
    if (r==DIAG_PASS) return "PASS";
    if (r==DIAG_FAIL) return "FAIL";
    if (r==DIAG_NOT_PRESENT) return "NOT PRESENT";
    if (r==DIAG_UNSUPPORTED) return "UNSUPPORTED";
    return "NOT RUN";
}
enum diag_result diag_cpu(void)
{
    volatile uint32_t a=0x55555555UL,b=0xaaaaaaaaUL;
    return ((a^b)==0xffffffffUL && (a+b)==0xffffffffUL) ? DIAG_PASS:DIAG_FAIL;
}
enum diag_result diag_stram(void)
{
    static const uint32_t pat[]={0x00000000UL,0xffffffffUL,0xaaaaaaaaUL,0x55555555UL,0x80000001UL};
    volatile uint32_t *p=(volatile uint32_t *)(uintptr_t)0x00020000UL;
    uint32_t old,i; old=*p;
    for(i=0;i<sizeof(pat)/sizeof(pat[0]);++i) { *p=pat[i]; if(*p!=pat[i]) { *p=old; return DIAG_FAIL; } }
    *p=old; return DIAG_PASS;
}
enum diag_result diag_ttram(void)
{
    if (g_system.cpu_type<CPU_68020) return DIAG_UNSUPPORTED;
    g_system.tt_ram_bytes=memory_detect_fast();
    if (!g_system.tt_ram_bytes) return DIAG_NOT_PRESENT;
    return DIAG_PASS;
}
enum diag_result diag_shifter(void) { return (REG8(SHIFTER_RES)&3u)<=2u?DIAG_PASS:DIAG_FAIL; }
enum diag_result diag_blitter(void) { return g_system.blitter_present?DIAG_PASS:DIAG_NOT_PRESENT; }
enum diag_result diag_mfp(void) { uint8_t v; return probe_read8(MFP_IERA,&v)?DIAG_PASS:DIAG_NOT_PRESENT; }
enum diag_result diag_ikbd(void) { uint8_t v; return probe_read8(IKBD_ACIA_CTRL,&v)?DIAG_PASS:DIAG_NOT_PRESENT; }
enum diag_result diag_rtc(void) { uint8_t v; return probe_read8(MEGA_RTC_SECONDS,&v)?DIAG_PASS:DIAG_NOT_PRESENT; }
/* Conservative RAM test: classic patterns plus a walking-bit address check,
   run over a dedicated scratch buffer. Non-destructive by design, and it uses
   only 68000-legal instructions so the ST bank can run it too. A full
   destructive test belongs in the diagnostics menu. */
enum diag_result diag_ram_pattern(void)
{
    static uint8_t scratch[256];
    static const uint8_t patterns[6]={0x00u,0xffu,0x55u,0xaau,0x01u,0x80u};
    uint8_t p,j;uint16_t i;uint8_t *ptr=scratch;
    for(p=0;p<6u;++p){
        for(i=0;i<8u;++i){uint8_t *base=ptr+(uint16_t)i*32u;for(j=0;j<32u;++j)base[j]=patterns[p];}
        for(i=0;i<8u;++i){const uint8_t *base=ptr+(uint16_t)i*32u;for(j=0;j<32u;++j)if(base[j]!=patterns[p])return DIAG_FAIL;}
    }
    for(i=0;i<256u;++i)ptr[i]=0u;
    for(i=0;i<8u;++i)ptr[(uint16_t)i*32u]=(uint8_t)(1u<<i);
    for(i=0;i<8u;++i)if(ptr[(uint16_t)i*32u]!=(uint8_t)(1u<<i))return DIAG_FAIL;
    for(i=0;i<256u;++i)if(ptr[i]!=(i%32u?0u:(uint8_t)(1u<<(i/32u))))return DIAG_FAIL;
    return DIAG_PASS;
}
/* Plan item 3b: 32-bit sum over the whole ROM window with the checksum field
   at header offset 0x14 treated as zero.  tools/patch_rom_checksum.py writes
   the matching value into the padded ROM after padrom; a zero (unpatched)
   field therefore reports FAIL. */
enum diag_result diag_rom_checksum(void)
{
    extern const uint8_t __rom_start[], __rom_end[];
    const uint8_t *p=__rom_start;
    uint32_t sum=0u;
    while(p<__rom_end){
        uint32_t word;
        if(p==__rom_start+0x14u)word=0u;
        else{
            /* Two word loads (ROM is 4-byte aligned) instead of four byte
               loads with shifts: the sum runs once per POST. */
            uint16_t hi,lo;
            __builtin_memcpy(&hi,p,2);
            __builtin_memcpy(&lo,p+2,2);
            word=((uint32_t)hi<<16)|lo;
        }
        sum+=word;p+=4;
    }
    return sum==(((uint32_t)__rom_start[0x14]<<24)|((uint32_t)__rom_start[0x15]<<16)|
                 ((uint32_t)__rom_start[0x16]<<8)|__rom_start[0x17])?DIAG_PASS:DIAG_FAIL;
}
