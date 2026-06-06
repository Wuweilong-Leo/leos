#ifndef OS_RESET_H
#define OS_RESET_H
#include "os_def.h"

/* 主动复位 CPU */
OS_SEC_KERNEL_TEXT void OsReboot(void);

/* 带 printf 的复位：输出信息后复位 */
#define OS_REBOOT(fmt, ...)                  \
    do {                                      \
        kprintf(fmt, ##__VA_ARGS__);          \
        OsReboot();                           \
    } while (0)

#endif /* OS_RESET_H */