#include "os_def.h"
#include "os_syscall_i386.h"
#include "os_idt_i386.h"
#include "os_uart_external.h"

/*
 * i386 syscall 处理
 * - 系统调用表 g_syscallTab：函数指针数组，下标为 syscall 号
 * - OsSyscallHandler 查表分发，不再 switch-case
 * - OsSyscallRegister 注册新 syscall，新增系统调用无需改分发逻辑
 */

OS_SEC_KERNEL_DATA OsSyscallFunc g_syscallTab[OS_SYS_NUM];

/* ====== 各 syscall 的处理函数 ====== */

static OS_SEC_KERNEL_TEXT U32 OsSysWrite(U32 arg1, U32 arg2, U32 arg3, U32 arg4)
{
    const char *str = (const char *)(uintptr_t)arg1;
    U32 len = arg2;
    U32 i;
    (void)arg3;
    (void)arg4;
    for (i = 0; i < len; i++) {
        OsUartPutc(str[i]);
    }
    return len;
}

static OS_SEC_KERNEL_TEXT U32 OsSysExit(U32 arg1, U32 arg2, U32 arg3, U32 arg4)
{
    (void)arg2;
    (void)arg3;
    (void)arg4;
    OsUartPrintf("[syscall] exit code=%u\n", arg1);
    return 0;
}

/* ====== 分发 ====== */

OS_SEC_KERNEL_TEXT U32 OsSyscallHandler(U32 sysno, U32 arg1, U32 arg2, U32 arg3, U32 arg4)
{
    OsSyscallFunc func;

    if (sysno >= OS_SYS_NUM) {
        return (U32)-1;
    }

    func = g_syscallTab[sysno];
    if (func == NULL) {
        return (U32)-1;
    }

    return func(arg1, arg2, arg3, arg4);
}

/* ====== 注册 ====== */

OS_SEC_KERNEL_TEXT U32 OsSyscallRegister(U32 sysno, OsSyscallFunc func)
{
    if (sysno >= OS_SYS_NUM) {
        return (U32)-1;
    }
    g_syscallTab[sysno] = func;
    return OS_OK;
}

/* ====== 初始化 ====== */

OS_SEC_KERNEL_TEXT U32 OsSyscallConfigInit(void)
{
    /* 注册 INT 0x80, DPL=3 允许用户态调用 */
    OsIdtBuildEntry(0x80, OS_IDT_ENTRY_ATTR3, OsSyscallVector80);

    /* 填充系统调用表 */
    OsSyscallRegister(OS_SYS_WRITE, OsSysWrite);
    OsSyscallRegister(OS_SYS_EXIT, OsSysExit);

    return OS_OK;
}
