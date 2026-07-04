#include "os_def.h"
#include "os_syscall_i386.h"
#include "os_idt_i386.h"
#include "os_uart_external.h"
#include "os_sched_external.h"
#include "os_mem_external.h"
#include "os_sem_external.h"
#include "os_cpu.h"

/*
 * i386 syscall 处理
 * - 系统调用表 g_syscallTab：函数指针数组，下标为 syscall 号
 * - OsSyscallHandler 查表分发，不再 switch-case
 * - OsSyscallRegister 注册新 syscall，新增系统调用无需改分发逻辑
 */

OS_SEC_KERNEL_DATA OsSyscallFunc g_syscallTab[OS_SYS_NUM];

/* 写字符串到 UART，返回写入字节数 */
static OS_SEC_KERNEL_TEXT U32 OsSysWrite(U32 buf, U32 len, U32 arg3, U32 arg4)
{
    const char *str = (const char *)(uintptr_t)buf;
    U32 i;
    (void)arg3;
    (void)arg4;
    for (i = 0; i < len; i++) {
        OsUartPutc(str[i]);
    }
    return len;
}

/* 终止当前进程，不返回 */
static OS_SEC_KERNEL_TEXT U32 OsSysExit(U32 exitCode, U32 arg2, U32 arg3, U32 arg4)
{
    (void)exitCode;
    (void)arg2;
    (void)arg3;
    (void)arg4;
    OsTaskDelete(OS_RUNNING_TASK()->pid);
    while (1) {
    }
    return 0;
}

/* 用户态动态内存分配，返回分配地址，0 表示失败 */
static OS_SEC_KERNEL_TEXT U32 OsSysMalloc(U32 size, U32 arg2, U32 arg3, U32 arg4)
{
    struct OsTaskCb *tsk = OS_RUNNING_TASK();
    (void)arg2;
    (void)arg3;
    (void)arg4;

    /* 惰性初始化：首次 malloc 时创建用户堆 FSC */
    if (tsk->usrFscCtrl == NULL) {
        tsk->usrFscCtrl = OsMemFscInitPt((uintptr_t)OS_PROCESS_USR_HEAP_BASE,
                                           OS_USR_HEAP_MEM_SIZE);
        if (tsk->usrFscCtrl == NULL) {
            return 0;
        }
    }

    return (U32)(uintptr_t)OsMemFscAlloc(tsk->usrFscCtrl, (size_t)size, 4);
}

/* 释放用户态动态内存，addr=0 时直接返回 */
static OS_SEC_KERNEL_TEXT U32 OsSysFree(U32 addr, U32 arg2, U32 arg3, U32 arg4)
{
    (void)arg2;
    (void)arg3;
    (void)arg4;
    if (addr == 0) {
        return OS_OK;
    }
    OsMemFscFree((void *)(uintptr_t)addr);
    return OS_OK;
}

/* 创建信号量，成功返回 semId，失败返回 OsSemCreate 错误码 */
static OS_SEC_KERNEL_TEXT U32 OsSysSemCreate(U32 type, U32 initVal, U32 maxCnt, U32 arg4)
{
    U32 semId;
    U32 ret;
    (void)arg4;
    ret = OsSemCreate((enum OsSemType)type, initVal, maxCnt, OS_SEM_WAKE_PRIO, &semId);
    if (ret != OS_OK) {
        return ret;
    }
    return semId;
}

/* 等待信号量，timeout=0 不等待，OS_SEM_WAIT_FOREVER 永久等待 */
static OS_SEC_KERNEL_TEXT U32 OsSysSemPend(U32 semId, U32 timeout, U32 arg3, U32 arg4)
{
    (void)arg3;
    (void)arg4;
    return OsSemPend(semId, timeout);
}

/* 释放信号量 */
static OS_SEC_KERNEL_TEXT U32 OsSysSemPost(U32 semId, U32 arg2, U32 arg3, U32 arg4)
{
    (void)arg2;
    (void)arg3;
    (void)arg4;
    return OsSemPost(semId);
}

/* 删除信号量 */
static OS_SEC_KERNEL_TEXT U32 OsSysSemDelete(U32 semId, U32 arg2, U32 arg3, U32 arg4)
{
    (void)arg2;
    (void)arg3;
    (void)arg4;
    return OsSemDelete(semId);
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
    OsSyscallRegister(OS_SYS_MALLOC, OsSysMalloc);
    OsSyscallRegister(OS_SYS_FREE, OsSysFree);
    OsSyscallRegister(OS_SYS_SEM_CREATE, OsSysSemCreate);
    OsSyscallRegister(OS_SYS_SEM_PEND, OsSysSemPend);
    OsSyscallRegister(OS_SYS_SEM_POST, OsSysSemPost);
    OsSyscallRegister(OS_SYS_SEM_DELETE, OsSysSemDelete);

    return OS_OK;
}
