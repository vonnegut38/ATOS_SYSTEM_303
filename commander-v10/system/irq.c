#include "irq.h"

#define IRQ_SLOTS 4

typedef struct { irq_handler_t fn; void *context; } irq_slot_t;

static irq_slot_t irq_slots[IRQ_SRC_COUNT][IRQ_SLOTS];
static uint32_t irq_dispatches[IRQ_SRC_COUNT];

int irq_register(irq_source_t source,irq_handler_t handler,void *context)
{
    int i;
    if(source<0||source>=IRQ_SRC_COUNT||!handler)return 0;
    for(i=0;i<IRQ_SLOTS;++i){
        if(irq_slots[source][i].fn==handler&&irq_slots[source][i].context==context)return 0;
    }
    for(i=0;i<IRQ_SLOTS;++i){
        if(!irq_slots[source][i].fn){
            irq_slots[source][i].fn=handler;
            irq_slots[source][i].context=context;
            return 1;
        }
    }
    return 0;
}

int irq_unregister(irq_source_t source,irq_handler_t handler)
{
    int i;
    if(source<0||source>=IRQ_SRC_COUNT||!handler)return 0;
    for(i=0;i<IRQ_SLOTS;++i){
        if(irq_slots[source][i].fn==handler){
            irq_slots[source][i].fn=0;
            irq_slots[source][i].context=0;
            return 1;
        }
    }
    return 0;
}

void irq_dispatch(irq_source_t source)
{
    int i;
    if(source<0||source>=IRQ_SRC_COUNT)return;
    ++irq_dispatches[source];
    for(i=0;i<IRQ_SLOTS;++i){
        irq_handler_t fn=irq_slots[source][i].fn;
        if(fn)fn(source,irq_slots[source][i].context);
    }
}

int irq_handler_count(irq_source_t source)
{
    int i,n=0;
    if(source<0||source>=IRQ_SRC_COUNT)return 0;
    for(i=0;i<IRQ_SLOTS;++i){if(irq_slots[source][i].fn)++n;}
    return n;
}
int irq_dispatch_count(irq_source_t source){if(source<0||source>=IRQ_SRC_COUNT)return 0;return (int)irq_dispatches[source];}
void irq_reset(void){int s,i;for(s=0;s<IRQ_SRC_COUNT;++s){irq_dispatches[s]=0u;for(i=0;i<IRQ_SLOTS;++i){irq_slots[s][i].fn=0;irq_slots[s][i].context=0;}}}
const char *irq_source_name(irq_source_t source)
{
    switch(source){
    case IRQ_SRC_VBL:return "VBL";
    case IRQ_SRC_HBL:return "HBL";
    case IRQ_SRC_MFP_A:return "MFP-A";
    case IRQ_SRC_MFP_B:return "MFP-B";
    case IRQ_SRC_MFP_C:return "MFP-C";
    case IRQ_SRC_MFP_D:return "MFP-D";
    case IRQ_SRC_ACIA:return "ACIA";
    default:return "?";
    }
}
