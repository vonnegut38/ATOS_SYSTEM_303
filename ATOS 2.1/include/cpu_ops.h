#ifndef ASF_CPU_OPS_H
#define ASF_CPU_OPS_H
#include <stdint.h>
#include "system.h"   /* CPU_68000..CPU_68060 */
/* Phase 20 (v10): per-family CPU capability layer.  It does NOT execute MOVEC
 * or touch the MMU; it tells callers what is safe (VBR on 68010+, CACR on
 * 68020+, data cache/PMMU on 68030+).  The MOVEC primitives stay in the
 * already-tested boot/exception.S asm.  Host-tested in tests/test_cpu_ops.c. */
typedef enum {
    CPU_FAM_68000=0,CPU_FAM_68010,CPU_FAM_68020,CPU_FAM_68030,CPU_FAM_68040,CPU_FAM_68060
} cpu_family_t;

typedef struct {
    const char *name;
    cpu_family_t family;
    uint8_t has_vbr;         /* 68010+ */
    uint8_t has_cacr;        /* 68020+ */
    uint8_t has_data_cache;  /* 68030+ */
    uint8_t has_pmmu;        /* 68030+ on-chip MMU */
    uint8_t has_fpu_onchip;  /* 68040/68060 */
} cpu_ops_t;

static inline cpu_family_t cpu_ops_family_for(uint8_t cpu_type)
{
    switch(cpu_type){
    case CPU_68010:return CPU_FAM_68010;
    case CPU_68020:return CPU_FAM_68020;
    case CPU_68030:return CPU_FAM_68030;
    case CPU_68040:return CPU_FAM_68040;
    case CPU_68060:return CPU_FAM_68060;
    default:return CPU_FAM_68000;
    }
}
const cpu_ops_t *cpu_ops_for(uint8_t cpu_type);
#endif
