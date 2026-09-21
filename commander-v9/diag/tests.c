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
