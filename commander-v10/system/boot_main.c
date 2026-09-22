#include "boot_protocol.h"

static const char *machine_name(uint8_t type)
{
    static const char *const names[]={"ST","MEGA ST","STE","MEGA STE","TT","FALCON"};
    return type<6u?names[type]:"UNKNOWN";
}

static const char *cpu_name(uint8_t type)
{
    static const char *const names[]={"68000","68010","68020","68030","68040","68060"};
    return type<6u?names[type]:"UNKNOWN";
}

void boot_main(const boot_handoff_t *h)
{
    if(!h||h->magic!=BOOT_HANDOFF_MAGIC||h->version!=BOOT_HANDOFF_VERSION||
       h->size<sizeof(*h)||!h->console_clear||!h->console_puts||!h->console_put_u32||!h->start_kernel)
        for(;;)__asm__ volatile("stop #0x2700");
    h->console_clear();
    h->console_puts("ATARI SYSTEM LOADER\n\nBOOT.SYS LOADED SUCCESSFULLY\n\nMACHINE: ");
    h->console_puts(machine_name(h->machine_type));
    h->console_puts("\nCPU: ");h->console_puts(cpu_name(h->cpu_type));
    h->console_puts("\nST-RAM: ");h->console_put_u32(h->st_ram_bytes/1024u);h->console_puts(" KB");
    h->console_puts("\nTT-RAM: ");h->console_put_u32(h->tt_ram_bytes/1024u);h->console_puts(" KB");
    h->console_puts("\nBOOT DEVICE: ");h->console_puts((h->boot_acsi_id&0x80u)?"IDE ":"ACSI ");h->console_put_u32((uint32_t)(h->boot_acsi_id&0x7fu));
    h->console_puts("\nBOOT PARTITION: ");h->console_put_u32(h->boot_partition+1u);
    h->console_puts("\n\nLOADING SYSTEM/KERNEL.SYS...");
    h->start_kernel();
    h->console_puts("\nKERNEL LOAD FAILED\nPRESS RESET");
    for(;;)__asm__ volatile("stop #0x2700");
}
