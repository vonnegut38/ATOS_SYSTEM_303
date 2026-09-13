#include "hardware.h"
#include "system.h"
uint32_t early_ram_bytes;

/* The assembly bootstrap determines both banks before normal C code starts. */
uint32_t memory_detect(void)
{
    return early_ram_bytes;
}

uint32_t memory_detect_fast(void)
{
    volatile uint32_t *const scratch=(volatile uint32_t *)(uintptr_t)0x00020000UL;
    const uint32_t fast_base=0x01000000UL;
    uint32_t old,value,size;

    if (g_system.cpu_type<CPU_68020)
        return 0;

    /* A 24-bit accelerator wraps 0x01020000 onto ST-RAM at 0x00020000.
     * Detect that alias using two patterns before attempting any Fast-RAM
     * write. The known ST-RAM longword is restored in every path. */
    old=*scratch;
    *scratch=0x13579bdfUL;
    if (probe_read32(fast_base+0x00020000UL,&value) && value==0x13579bdfUL) {
        *scratch=0x2468ace0UL;
        if (probe_read32(fast_base+0x00020000UL,&value) && value==0x2468ace0UL) {
            *scratch=old;
            return 0;
        }
    }
    *scratch=old;

    /* Size the conventional contiguous Fast/TT-RAM window in 1 MiB steps.
     * Each probe preserves the tested longword. Limit the scan to 256 MiB. */
    for (size=0;size<0x10000000UL;size+=0x00100000UL) {
        if (!probe_rw32(fast_base+size))
            break;
    }
    return size;
}
