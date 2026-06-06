#ifndef OS_RESET_H
#define OS_RESET_H
#include "os_def.h"
#include "os_print_external.h"

/*
 * OsPanic — 触发 #UD (Undefined Instruction) 异常
 *
 * CPU 自动保存上下文，走正常异常流程：
 *   OsExcDispatcher → OsExcReport（打印寄存器/地址）→ while(1) 挂死
 */
OS_SEC_KERNEL_TEXT void OsPanic(void) __attribute__((noreturn));

/* 带打印的 panic：输出信息后触发异常 */
#define OS_PANIC(fmt, ...)                    \
    do {                                      \
        kprintf("\nPANIC: " fmt, ##__VA_ARGS__); \
        OsPanic();                             \
    } while (0)

/* 兼容旧名 */
#define OS_REBOOT(fmt, ...) OS_PANIC(fmt, ##__VA_ARGS__)

#endif /* OS_RESET_H */