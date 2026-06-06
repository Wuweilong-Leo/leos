#ifndef OS_IRQ_INTERNAL_H
#define OS_IRQ_INTERNAL_H
#include "os_irq_external.h"

#define OS_IRQ_MIN  0x20
#define OS_IRQ_MAX  0x20
#define OS_IRQ_NUM  (OS_IRQ_MAX - OS_IRQ_MIN + 1)

struct OsIrqForm {
    OsIrqHandlerFunc isr;
};

OS_INLINE U32 OsIrqNum2Idx(U32 irqNum)
{
    return irqNum - OS_IRQ_MIN;
}

#endif /* OS_IRQ_INTERNAL_H */