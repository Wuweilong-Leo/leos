#ifndef OS_EXC_H
#define OS_EXC_H
#include "os_def.h"
#include "os_target.h"

/* 架构相关 inline 实现由下方条件编译引入 */
#if defined(ARCH_i386)
#include "os_exc_i386.h"
#else
#error "Unsupported architecture. Define ARCH_i386 in os_target.h."
#endif

#endif /* OS_EXC_H */