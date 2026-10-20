#ifndef ASF_IRQ_H
#define ASF_IRQ_H
#include <stdint.h>
/* Phase 6 (v10): one central place where VBL/HBL/MFP Timer A-D and ACIA
 * handlers register, instead of ad-hoc vector patching.  The dispatcher is
 * bounded (fixed slot count) and does no allocation, so it is safe to call
 * from an interrupt.  Host-tested in tests/test_irq.c.
 * NOTE: registering a handler is not yet wired to the actual hardware
 * vectors; that belongs to the kernel entry (kernel_entry.S). */
typedef enum {
    IRQ_SRC_VBL = 0,
    IRQ_SRC_HBL,
    IRQ_SRC_MFP_A,
    IRQ_SRC_MFP_B,
    IRQ_SRC_MFP_C,
    IRQ_SRC_MFP_D,
    IRQ_SRC_ACIA,
    IRQ_SRC_COUNT
} irq_source_t;

typedef void (*irq_handler_t)(irq_source_t source,void *context);

int  irq_register(irq_source_t source,irq_handler_t handler,void *context);
int  irq_unregister(irq_source_t source,irq_handler_t handler);
void irq_dispatch(irq_source_t source);      /* call every handler for source */
int  irq_handler_count(irq_source_t source); /* registered handlers */
int  irq_dispatch_count(irq_source_t source);/* times dispatched */
void irq_reset(void);
const char *irq_source_name(irq_source_t source);
#endif
