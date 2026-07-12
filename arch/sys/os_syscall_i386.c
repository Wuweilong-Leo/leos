#include "os_def.h"
#include "os_syscall_i386.h"
#include "os_idt_i386.h"
#include "os_uart_external.h"
#include "os_sched_external.h"
#include "os_mem_external.h"
#include "os_sem_external.h"
#include "os_cpu.h"
#include "os_process_external.h"
#include "os_task_external.h"

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
    struct OsTaskCb *tskCb = OS_RUNNING_TASK();
    (void)arg2;
    (void)arg3;
    (void)arg4;

    tskCb->exitCode = exitCode;
    tskCb->status |= OS_TASK_STATUS_ZOMBIE;
    OsTaskDelete(tskCb->pid);
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

/* 获取当前进程 PID */
static OS_SEC_KERNEL_TEXT U32 OsSysGetPid(U32 arg1, U32 arg2, U32 arg3, U32 arg4)
{
    (void)arg1; (void)arg2; (void)arg3; (void)arg4;
    return OS_RUNNING_TASK()->pid;
}

/* 创建子进程，返回子进程 pid（父进程）/ 0（子进程） */
static OS_SEC_KERNEL_TEXT U32 OsSysFork(U32 arg1, U32 arg2, U32 arg3, U32 arg4)
{
    (void)arg1; (void)arg2; (void)arg3; (void)arg4;
    return OsProcessFork();
}

/* waitpid 选项 */
#define OS_SYS_WAITPID_WNOHANG  1

/* 等待子进程退出，返回子进程 pid，status 写入退出码 */
static OS_SEC_KERNEL_TEXT U32 OsSysWaitpid(U32 pid, U32 statusPtr, U32 options, U32 arg4)
{
    struct OsTaskCb *curTsk = OS_RUNNING_TASK();
    U32 i;
    (void)arg4;

    /* 非阻塞扫描找 ZOMBIE 子进程 */
    for (i = 0; i < g_tskMaxNum; i++) {
        struct OsTaskCb *child = &g_tskCbArray[i];
        if (child == curTsk) continue;
        if (child->parentPid != curTsk->pid) continue;
        if (!(child->status & OS_TASK_STATUS_ZOMBIE)) continue;
        if (pid != OS_WAIT_ANY_CHILD && child->pid != pid) continue;

        /* 收割 ZOMBIE */
        if (statusPtr != 0) {
            *(U32 *)(uintptr_t)statusPtr = (child->exitCode << 8);
        }
        OsTaskReapZombie(child);
        curTsk->waitPid = 0;
        return child->pid;
    }

    /* 没找到 ZOMBIE，检查是否有子进程 */
    {
        bool hasChild = FALSE;
        for (i = 0; i < g_tskMaxNum; i++) {
            if (g_tskCbArray[i].parentPid == curTsk->pid &&
                (g_tskCbArray[i].status & OS_TASK_STATUS_USED)) {
                hasChild = TRUE;
                break;
            }
        }
        if (!hasChild) {
            return (U32)-1;  /* ECHILD: 没有子进程 */
        }
    }

    /* 有子进程但还没退出 */
    if (options & OS_SYS_WAITPID_WNOHANG) {
        return 0;
    }

    /* 阻塞等待 */
    curTsk->waitPid = pid;
    curTsk->status |= OS_TASK_STATUS_PENDING;
    OsSchedRdyListDequeTsk(curTsk);
    OsTaskSchedule();

    /* 被唤醒后再次扫描（单核非抢占，无竞态） */
    for (i = 0; i < g_tskMaxNum; i++) {
        struct OsTaskCb *child = &g_tskCbArray[i];
        if (child->parentPid != curTsk->pid) continue;
        if (!(child->status & OS_TASK_STATUS_ZOMBIE)) continue;
        if (pid != OS_WAIT_ANY_CHILD && child->pid != pid) continue;

        if (statusPtr != 0) {
            *(U32 *)(uintptr_t)statusPtr = (child->exitCode << 8);
        }
        OsTaskReapZombie(child);
        curTsk->waitPid = 0;
        return child->pid;
    }

    /* 不应到达（单核非抢占下唤醒后必然能找到） */
    curTsk->waitPid = 0;
    return (U32)-1;
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
    OsSyscallRegister(OS_SYS_GETPID, OsSysGetPid);
    OsSyscallRegister(OS_SYS_FORK, OsSysFork);
    OsSyscallRegister(OS_SYS_WAITPID, OsSysWaitpid);

    return OS_OK;
}
