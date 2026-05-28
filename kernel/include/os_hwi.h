#ifndef OS_HWI_H
#define OS_HWI_H
#include "os_def.h"
#include "os_target.h"

enum OsIntStatus {
    OS_INT_OFF,
    OS_INT_ON
};

/* 通用中断 API 声明 */
extern U32 OsHwiCreate(U32 hwiNum, void (*isr)(U32));
extern void OsHwiConfigInit(void);

/* 架构相关 inline 实现由下方条件编译引入 */
#if defined(ARCH_i386)
#include "os_hwi_i386.h"
#else
#error "Unsupported architecture. Define ARCH_i386 in os_target.h."
#endif

#endif
