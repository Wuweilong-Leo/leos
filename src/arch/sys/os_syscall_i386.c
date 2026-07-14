#include "os_def.h"
#include "os_syscall_i386.h"
#include "os_idt_i386.h"
#include "os_uart_external.h"
#include "os_sched_external.h"
#include "os_mem_external.h"
#include "os_cpu.h"
#include "os_task_external.h"
#include "os_context_i386.h"
#include "string.h"

/*
 * i386 专有系统调用实现
 * - OsSysWrite：UART 设备输出（依赖 arch/dev/uart）
 * - OsSysClone：改 AllSaveContext 寄存器，创建共享地址空间用户线程
 * - OsSyscallConfigInit：安装 INT 0x80 中断门 + 注册全部 syscall
 *
 * 架构无关 syscall（Exit、Malloc、Free、Sem系列、GetPid、Fork、Waitpid、Detach）
 * 实现在 kernel/sys/os_syscall.c，由 OsSyscallRegisterAll 统一注册。
 * 本文件只额外注册两个 i386 专有 syscall（Write/Clone）。
 */

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

/* ====== 初始化 ====== */

OS_SEC_KERNEL_TEXT U32 OsSyscallConfigInit(void)
{
    /* 注册 INT 0x80, DPL=3 允许用户态调用 */
    OsIdtBuildEntry(0x80, OS_IDT_ENTRY_ATTR3, OsSyscallVector80);

    /* 架构无关 syscall（kernel/sys/os_syscall.c 实现） */
    OsSyscallRegisterAll();

    /* i386 专有 syscall */
    OsSyscallRegister(OS_SYS_WRITE, OsSysWrite);
    OsSyscallRegister(OS_SYS_CLONE, OsSysClone);

    return OS_OK;
}
