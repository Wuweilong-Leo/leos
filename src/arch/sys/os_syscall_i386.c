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
#include "os_context_i386.h"
#include "os_hwi.h"
#include "string.h"

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

/* 等待子进程/线程退出，返回 pid，status 写入退出码。
 * 【DET-003】detached 线程不参与 waitpid：扫描跳过、hasJoinable 不计、指定 pid 时返回 -1。 */
static OS_SEC_KERNEL_TEXT U32 OsSysWaitpid(U32 pid, U32 statusPtr, U32 options, U32 arg4)
{
    struct OsTaskCb *curTsk = OS_RUNNING_TASK();
    U32 i;
    (void)arg4;

    /* 指定 pid 时：先确认是调用者的子进程，再检查 detached。
     * 不在函数开头盲查 detached——需先过 parentPid 权限校验，避免对非子进程误报。 */
    if (pid != OS_WAIT_ANY_CHILD) {
        if (pid >= g_tskMaxNum) {
            return (U32)-1;
        }
        {
            struct OsTaskCb *t = &g_tskCbArray[pid];
            if (!(t->status & OS_TASK_STATUS_USED) || t->parentPid != curTsk->pid) {
                return (U32)-1;  /* 不是你的子进程 */
            }
            if (t->detached) {
                return (U32)-1;  /* 已 detached：不可 join */
            }
        }
    }

    /* 非阻塞扫描找 ZOMBIE 子进程（跳过 detached） */
    for (i = 0; i < g_tskMaxNum; i++) {
        struct OsTaskCb *child = &g_tskCbArray[i];
        if (child == curTsk) continue;
        if (child->parentPid != curTsk->pid) continue;
        if (!(child->status & OS_TASK_STATUS_ZOMBIE)) continue;
        if (child->detached) continue;
        if (pid != OS_WAIT_ANY_CHILD && child->pid != pid) continue;

        /* 收割 ZOMBIE */
        if (statusPtr != 0) {
            *(U32 *)(uintptr_t)statusPtr = (child->exitCode << 8);
        }
        OsTaskReapZombie(child);
        curTsk->waitPid = 0;
        return child->pid;
    }

    /* 没找到 ZOMBIE，检查是否有可 join 的子进程（detached 不算） */
    {
        bool hasJoinable = FALSE;
        for (i = 0; i < g_tskMaxNum; i++) {
            if (g_tskCbArray[i].parentPid == curTsk->pid &&
                (g_tskCbArray[i].status & OS_TASK_STATUS_USED) &&
                !g_tskCbArray[i].detached) {
                hasJoinable = TRUE;
                break;
            }
        }
        if (!hasJoinable) {
            return (U32)-1;  /* ECHILD: 没有可 join 的子进程 */
        }
    }

    /* 有可 join 子进程但还没退出 */
    if (options & OS_SYS_WAITPID_WNOHANG) {
        return 0;
    }

    /* 阻塞等待 */
    curTsk->waitPid = pid;
    curTsk->status |= OS_TASK_STATUS_PENDING;
    OsSchedRdyListDequeTsk(curTsk);
    OsTaskSchedule();

    /* 被唤醒后再次扫描（单核非抢占，无竞态）。可能被 detached 线程退出唤醒（无 ZOMBIE），
     * 找不到时重新检查 hasJoinable：有则继续等，无则返回 ECHILD。 */
    for (;;) {
        for (i = 0; i < g_tskMaxNum; i++) {
            struct OsTaskCb *child = &g_tskCbArray[i];
            if (child->parentPid != curTsk->pid) continue;
            if (!(child->status & OS_TASK_STATUS_ZOMBIE)) continue;
            if (child->detached) continue;
            if (pid != OS_WAIT_ANY_CHILD && child->pid != pid) continue;

            if (statusPtr != 0) {
                *(U32 *)(uintptr_t)statusPtr = (child->exitCode << 8);
            }
            OsTaskReapZombie(child);
            curTsk->waitPid = 0;
            return child->pid;
        }

        /* 未找到 ZOMBIE，重新检查是否还有可 join 子进程 */
        {
            bool hasJoinable = FALSE;
            for (i = 0; i < g_tskMaxNum; i++) {
                if (g_tskCbArray[i].parentPid == curTsk->pid &&
                    (g_tskCbArray[i].status & OS_TASK_STATUS_USED) &&
                    !g_tskCbArray[i].detached) {
                    hasJoinable = TRUE;
                    break;
                }
            }
            if (!hasJoinable) {
                curTsk->waitPid = 0;
                return (U32)-1;  /* ECHILD: 所有子线程都 detached 或已退出 */
            }
        }

        /* 还有 joinable 子进程未退出，重新 PENDING 等待 */
        curTsk->waitPid = pid;
        curTsk->status |= OS_TASK_STATUS_PENDING;
        OsSchedRdyListDequeTsk(curTsk);
        OsTaskSchedule();
    }
}

/* 创建共享地址空间的用户线程（类似 Linux clone(CLONE_VM)）
 * stackTop: 用户传入的线程栈顶
 * startRoutine: 线程入口函数
 * arg: 传给入口的参数
 * trampoline: 跳板函数地址（子线程 iret 后从这里开始执行）
 * 返回：子线程 tid（父）/ 0 不会到达（子，eip 被改为 trampoline）
 */
static OS_SEC_KERNEL_TEXT U32 OsSysClone(U32 stackTop, U32 startRoutine, U32 arg, U32 trampoline)
{
    struct OsTaskCb *parent;
    struct OsTaskCb *child;
    U32 childPid;
    uintptr_t childStk;
    struct OsAllSaveContext *ctx;

    parent = OS_RUNNING_TASK();

    /* 仅用户进程可调用 clone */
    if (parent->tskType != OS_TASK_PROCESS) {
        return (U32)-1;
    }

    /* 1. 取空闲 TCB */
    child = OsTaskGetFreeCb();
    if (child == NULL) {
        return (U32)-1;
    }

    /* 2. 分配内核栈 */
    childStk = (uintptr_t)OsMemKernelAlloc(OS_TASK_KERNEL_STACK_SIZE, 16);
    if (childStk == 0) {
        OsTaskReleaseFreeCb(child);
        return (U32)-1;
    }
    child->kernelStkTop = childStk;

    /* 3. 拷贝父进程内核栈（保留 AllSaveContext，与 fork 一致）
     *    INT 0x80 期间中断关闭，内核栈内容稳定，memcpy 安全 */
    memcpy((void *)childStk, (void *)parent->kernelStkTop, OS_TASK_KERNEL_STACK_SIZE);

    /* 4. 设置 stkPtr 偏移 */
    child->stkPtr = childStk - parent->kernelStkTop + parent->stkPtr;

    /* 5. 修改子线程的 AllSaveContext */
    ctx = (struct OsAllSaveContext *)child->stkPtr;
    ctx->eax = 0;                   /* 子线程从跳板入口开始，不会读到 eax */
    ctx->eip = trampoline;          /* iret 后跳到跳板函数 */
    ctx->esp = stackTop;            /* 用户传入的栈顶 */
    ctx->ebx = startRoutine;        /* 跳板通过 ebx 读取 start_routine */
    ctx->ecx = arg;                 /* 跳板通过 ecx 读取 arg */
    ctx->eflags = OS_PROCESS_EFLAGS; /* IF=1, IOPL=0，确保子线程开中断启动 */
    /* 段寄存器从父进程 memcpy 继承，已是用户态选择子，无需修改 */

    /* 6. 设置 TCB 字段 */
    child->status = OS_TASK_STATUS_USED | OS_TASK_STATUS_SUSPENDED;
    child->prio = parent->prio;
    child->oriPrio = parent->oriPrio;
    child->entry = parent->entry;
    memcpy(child->arg, parent->arg, sizeof(parent->arg));
    child->timeSliceTicks = parent->timeSliceTicks;
    child->expiredTick = 0;
    strcpy(child->name, "pthread");
    child->tskType = OS_TASK_PROCESS;
    child->pgDir = parent->pgDir;                     /* 共享 pgDir */
    child->usrVirMemPool = parent->usrVirMemPool;     /* 浅拷贝：共享位图 */
    OsListInit(&child->usrVirMemPool.memCtrlList);    /* 独立链表头，与 fork 一致 */
    child->usrFscCtrl = parent->usrFscCtrl;           /* 共享 FSC */
    child->parentPid = parent->pid;                   /* waitpid 依赖 */
    child->exitCode = 0;
    child->waitPid = 0;
    child->detached = FALSE;                          /* clone 线程默认 joinable，需显式 detach */
    child->pgShareMaster = parent->pgShareMaster;     /* 指向同一个 master */
    parent->pgShareMaster->pgDirRefCnt++;             /* 引用计数 +1 */
    OsListInit(&child->holdSemList);
    OsListInit(&child->msgList);

    /* 7. 设置时间片 */
    OsTaskSetTimeSlice(child, OsTaskCalTimeSlice(child));

    /* 8. 恢复子线程 */
    childPid = child->pid;
    OsTaskResume(childPid);

    return childPid;
}

/* 分离线程：标记 detached，退出时自动回收 TCB/内核栈，无需 join/waitpid。
 * 也支持 self-detach（thread == 调用者自身）和对已 ZOMBIE 线程 detach（立即收割）。
 * 返回 0=成功，错误码=失败。 */
static OS_SEC_KERNEL_TEXT U32 OsSysDetach(U32 tid, U32 arg2, U32 arg3, U32 arg4)
{
    struct OsTaskCb *tskCb;
    struct OsTaskCb *curTsk;
    enum OsIntStatus intSave;
    U32 wi;

    (void)arg2; (void)arg3; (void)arg4;

    if (tid >= g_tskMaxNum) {
        return OS_TASK_DETACH_INVALID_PID;
    }

    intSave = OsIntLock();

    tskCb = OS_TASK_GET_CB(tid);
    curTsk = OS_RUNNING_TASK();

    /* 1. 目标必须存在且是用户进程/线程 */
    if (!(tskCb->status & OS_TASK_STATUS_USED)) {
        OsIntRestore(intSave);
        return OS_TASK_DETACH_INVALID_PID;
    }
    if (tskCb->tskType != OS_TASK_PROCESS) {
        OsIntRestore(intSave);
        return OS_TASK_DETACH_NOT_PROCESS;
    }

    /* 2. 调用者与目标必须在同一地址空间组（POSIX: 同进程内线程可互相 detach，含 self-detach） */
    if (tskCb->pgShareMaster != curTsk->pgShareMaster) {
        OsIntRestore(intSave);
        return OS_TASK_DETACH_NOT_SAME_VM;
    }

    /* 3. 只有 clone 线程可以被 detach（fork 子进程和独立进程不可：pgShareMaster==self） */
    if (tskCb->pgShareMaster == tskCb) {
        OsIntRestore(intSave);
        return OS_TASK_DETACH_NOT_CLONE;
    }

    /* 4. 不可重复 detach */
    if (tskCb->detached) {
        OsIntRestore(intSave);
        return OS_TASK_DETACH_ALREADY;
    }

    /* 5. 目标线程的父进程正在 waitpid 等待目标时不允许 detach（防止永久阻塞）。
     *    只检查目标的父进程（waiter->pid == tskCb->parentPid），其他进程的 waitpid(-1) 不影响。 */
    for (wi = 0; wi < g_tskMaxNum; wi++) {
        struct OsTaskCb *waiter = &g_tskCbArray[wi];
        if (!(waiter->status & OS_TASK_STATUS_PENDING)) continue;
        if (waiter->waitPid == 0) continue;
        if (waiter->pid != tskCb->parentPid) continue;
        if (waiter->waitPid == tid || waiter->waitPid == OS_WAIT_ANY_CHILD) {
            OsIntRestore(intSave);
            return OS_TASK_DETACH_HAS_WAITER;
        }
    }

    /* 6. 已 ZOMBIE（先退出后 detach）：直接收割，不设 detached（TCB 已回收）。
     *    非 ZOMBIE：设 detached=TRUE，退出时由 OsTaskDelete 自动回收。 */
    if (tskCb->status & OS_TASK_STATUS_ZOMBIE) {
        OsTaskReapZombie(tskCb);
    } else {
        tskCb->detached = TRUE;
    }

    OsIntRestore(intSave);
    return OS_OK;
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
    OsSyscallRegister(OS_SYS_CLONE, OsSysClone);
    OsSyscallRegister(OS_SYS_DETACH, OsSysDetach);

    return OS_OK;
}
