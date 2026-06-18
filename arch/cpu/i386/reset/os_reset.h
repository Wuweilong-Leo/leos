#ifndef OS_RESET_H
#define OS_RESET_H
#include "os_def.h"

OS_SEC_KERNEL_TEXT void OsPanic(void) __attribute__((noreturn));

#define OS_REBOOT(...) OsPanic()

#endif /* OS_RESET_H */