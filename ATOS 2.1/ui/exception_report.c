#include "console.h"
#include "diagnostics.h"
#include "system.h"
#include "exception_format.h"

/* Set by the assembly exception entry before calling us. */
volatile uint32_t exception_frame_ptr;

static void put_hex32(uint32_t value)
{
    static const char hx[]="0123456789ABCDEF";int i;
    for(i=7;i>=0;--i){char c[2];c[0]=hx[(value>>(i*4))&15u];c[1]='\0';console_puts(c);}
}

void bios_exception_report(void)
{
    const uint16_t *w=(const uint16_t *)(uintptr_t)exception_frame_ptr;
    exc_frame_t f;
    if(!w){console_puts("ATOS SYSTEM EXCEPTION\nNO FRAME\n");return;}
    exc_parse_frame(w,(g_system.cpu_type>=CPU_68010)?1:0,&f);
    console_clear();
    console_puts("ATOS SYSTEM EXCEPTION\n\n");
    console_puts("CPU:    ");console_puts(cpu_type_name(g_system.cpu_type));console_puts("\n");
    console_puts("VECTOR: ");put_hex32(f.vector);console_puts("\n");
    console_puts("FORMAT: ");put_hex32(f.format);console_puts("  ");console_puts(exc_format_name(f.format));console_puts("\n");
    console_puts("PC:     ");put_hex32(f.pc);console_puts("\n");
    console_puts("SR:     ");put_hex32(f.sr);console_puts("\n");
    console_puts("SP:     ");put_hex32((uint32_t)exception_frame_ptr);console_puts("\n");
    console_puts("FAULT:  ");if(f.fault_valid)put_hex32(f.fault);else console_puts("N/A");console_puts("\n\n");
    console_puts("SYSTEM HALTED");
}
