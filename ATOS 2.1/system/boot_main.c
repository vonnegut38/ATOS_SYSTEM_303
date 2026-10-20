#include "boot_protocol.h"

void boot_main(const boot_handoff_t *h)
{
    if(!h||h->magic!=BOOT_HANDOFF_MAGIC||h->version!=BOOT_HANDOFF_VERSION||
       h->size<sizeof(*h)||!h->console_clear||!h->console_puts||!h->console_put_u32||!h->start_kernel)
        for(;;)__asm__ volatile("stop #0x2700");
    /* The ROM leaves the cursor on its fixed boot status row. Hardware is
       already listed by POST (and again by the kernel), so repeating that
       table here scrolls the entire VRAM once per line, particularly costly
       at 800x600x8. A short acknowledgement fits the existing row; the ROM
       start_kernel callback owns the subsequent load/error status. */
    h->start_kernel();
    h->console_puts("\nKERNEL LOAD FAILED\nPRESS RESET");
    for(;;)__asm__ volatile("stop #0x2700");
}
