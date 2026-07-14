#include "os_debug_external.h"
#include "os_print_external.h"
#include "os_def.h"
#include "os_list_external.h"
#include "os_sched_external.h"
#include "os_task_external.h"
#include "os_hwi.h"
#include "os_mem_external.h"

/* ---- 日志级别控制 ---- */

OS_SEC_KERNEL_DATA enum OsLogLevel g_logLevel = OS_LOG_WARN;

OS_SEC_KERNEL_TEXT void OsDebugSetLogLevel(enum OsLogLevel level)
{
    g_logLevel = level;
}

OS_SEC_KERNEL_TEXT enum OsLogLevel OsDebugGetLogLevel(void)
{
    return g_logLevel;
}

/* ---- Panic ---- */

OS_SEC_KERNEL_TEXT void OsDebugPanicSpin(const char *filename, U32 line, const char *func,
                                         const char *cond)
{
    OsIntLock();
    kprintf("\n===== KERNEL PANIC =====\n");
    kprintf("File: %s\n", filename);
    kprintf("Line: %u\n", line);
    kprintf("Func: %s\n", func);
    kprintf("Cond: %s\n", cond);
    kprintf("========================\n");
    while (1) {
    }
}

/* ---- 断言失败入口（供宏调用） ---- */

OS_SEC_KERNEL_TEXT void OsDebugAssertFail(const char *filename, U32 line, const char *func,
                                          const char *cond)
{
    OsDebugPanicSpin(filename, line, func, cond);
}

/* ---- 链表调试 ---- */

OS_SEC_KERNEL_TEXT void OsDebugPrintList(struct OsList *list)
{
    struct OsList *tmpNode;

    OS_LIST_FOR_EACH(list, tmpNode) {
        kprintf("0x%x<->0x%x ", (uintptr_t)tmpNode->prev, (uintptr_t)tmpNode->next);
    }
    kprintf("[end]\n");
}

/* ---- 就绪队列调试 ---- */

OS_SEC_KERNEL_TEXT void OsDebugPrintRdyList(void)
{
    U32 i;
    struct OsRunQue *rq = OS_RUN_QUE();

    kprintf("--- Ready Lists ---\n");
    for (i = 0; i < OS_TASK_PRIO_MAX_NUM; i++) {
        if (!OsListIsEmpty(&rq->rdyList[i])) {
            kprintf("prio%u: ", i);
            OsDebugPrintList(&rq->rdyList[i]);
        }
    }
}

/* ---- 任务状态调试 ---- */

OS_SEC_KERNEL_TEXT void OsDebugPrintTaskInfo(struct OsTaskCb *tsk)
{
    if (tsk == NULL) {
        kprintf("Task: NULL\n");
        return;
    }
    kprintf("Task pid=%u name=%s prio=%u status=0x%x stkTop=0x%x\n", tsk->pid, tsk->name, tsk->prio,
            tsk->status, (uintptr_t)tsk->kernelStkTop);
}

OS_SEC_KERNEL_TEXT void OsDebugPrintAllTasks(void)
{
    U32 i;
    struct OsRunQue *rq = OS_RUN_QUE();

    kprintf("=== All Tasks ===\n");
    kprintf("Running: ");
    OsDebugPrintTaskInfo(rq->runningTsk);
    kprintf("Idle:    ");
    OsDebugPrintTaskInfo(rq->idleTsk);

    for (i = 0; i < OS_TASK_PRIO_MAX_NUM; i++) {
        struct OsList *node;
        OS_LIST_FOR_EACH(&rq->rdyList[i], node) {
            /* 通过 rdyListNode 偏移反推 OsTaskCb */
            struct OsTaskCb *tsk =
                (struct OsTaskCb *)((U8 *)node - (uintptr_t)(&((struct OsTaskCb *)0)->rdyListNode));
            OsDebugPrintTaskInfo(tsk);
        }
    }
    kprintf("=================\n");
}

/* ---- 内存池调试 ---- */

OS_SEC_KERNEL_TEXT void OsDebugPrintMemPool(struct OsMemPool *pool, const char *name)
{
    if (pool == NULL) {
        kprintf("MemPool [%s]: NULL\n", name);
        return;
    }
    kprintf("MemPool [%s] base=0x%x size=0x%x btmp.base=0x%x btmp.bits=%u\n", name, (uintptr_t)pool->base,
            (size_t)pool->size, (uintptr_t)pool->btmp.base, pool->btmp.bitNum);
}

/* ---- 系统状态概览 ---- */

OS_SEC_KERNEL_TEXT void OsDebugSystemStatus(void)
{
    struct OsRunQue *rq = OS_RUN_QUE();

    kprintf("\n====== System Status ======\n");
    kprintf("Running task: pid=%u\n", rq->runningTsk ? rq->runningTsk->pid : 0xFFFFFFFF);
    kprintf("Need resched:  %s\n", rq->needSched ? "yes" : "no");
    kprintf("Int count:     %u\n", rq->intCount);

    OsDebugPrintRdyList();

    kprintf("===========================\n\n");
}