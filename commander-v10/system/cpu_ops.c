#include "cpu_ops.h"
static const cpu_ops_t ops_68000={"68000",CPU_FAM_68000,0u,0u,0u,0u,0u};
static const cpu_ops_t ops_68010={"68010",CPU_FAM_68010,1u,0u,0u,0u,0u};
static const cpu_ops_t ops_68020={"68020",CPU_FAM_68020,1u,1u,0u,0u,0u};
static const cpu_ops_t ops_68030={"68030",CPU_FAM_68030,1u,1u,1u,1u,0u};
static const cpu_ops_t ops_68040={"68040",CPU_FAM_68040,1u,1u,1u,1u,1u};
static const cpu_ops_t ops_68060={"68060",CPU_FAM_68060,1u,1u,1u,1u,1u};
const cpu_ops_t *cpu_ops_for(uint8_t cpu_type)
{
    switch(cpu_ops_family_for(cpu_type)){
    case CPU_FAM_68010:return &ops_68010;
    case CPU_FAM_68020:return &ops_68020;
    case CPU_FAM_68030:return &ops_68030;
    case CPU_FAM_68040:return &ops_68040;
    case CPU_FAM_68060:return &ops_68060;
    default:return &ops_68000;
    }
}
