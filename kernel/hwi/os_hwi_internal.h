#ifndef OS_HWI_INTERNAL_H
#define OS_HWI_INTERNAL_H
#include "os_hwi_external.h"

/* 通用层：中断表大小由架构层定义的 OS_HWI_MAX_NUM 决定 */
struct OsHwiForm {
    OsHwiHandlerFunc isr;
};

#endif /* OS_HWI_INTERNAL_H */