#ifndef OS_RESET_H
#define OS_RESET_H
#include "os_def.h"
#include "os_print_external.h"

OS_SEC_KERNEL_TEXT void OsPanic(void) __attribute__((noreturn));

#define OS_REBOOT(...)                           \
    do {                                         \
        kprintf("[REBOOT][%s:%d] " __VA_ARGS__, __func__, __LINE__); \
        OsPanic();                                \
    } while (0)

#endif /* OS_RESET_H */