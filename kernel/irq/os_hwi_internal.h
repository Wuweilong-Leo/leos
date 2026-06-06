#ifndef OS_HWI_INTERNAL_H
#define OS_HWI_INTERNAL_H
#include "os_hwi_external.h"

#define OS_IRQ_MIN  0x20
#define OS_IRQ_MAX  0x20
#define OS_IRQ_NUM  (OS_IRQ_MAX - OS_IRQ_MIN + 1)

struct OsHwiForm {
    OsHwiHandlerFunc isr;
};

OS_INLINE U32 OsHwiNum2Idx(U32 hwiNum)
{
    return hwiNum - OS_IRQ_MIN;
}

#endif /* OS_HWI_INTERNAL_H */