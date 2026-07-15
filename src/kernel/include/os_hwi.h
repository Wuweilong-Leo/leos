#ifndef OS_HWI_H
#define OS_HWI_H
#include "os_def.h"
#include "os_target.h"

enum OsIntStatus { OS_INT_OFF, OS_INT_ON };

/* 架构相关 inline 实现由下方条件编译引入 */
#if defined(ARCH_i386)
#include "os_hwi_i386.h"
#else
#error "Unsupported architecture. Provide os_feature.h with ARCH_<arch> defined."
#endif

#endif /* OS_HWI_H */