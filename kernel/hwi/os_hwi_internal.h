#ifndef OS_HWI_INTERNAL_H
#define OS_HWI_INTERNAL_H
#include "os_hwi_external.h"

/* 通用层：中断表大小由架构层定义的 OS_HWI_MAX_NUM 决定 */
struct OsHwiForm {
    OsHwiHandlerFunc isr;
};

/* 软中断向量号：用于任务删除时触发栈回收 */
#define OS_SOFT_INT_VEC  0x30

#endif /* OS_HWI_INTERNAL_H */