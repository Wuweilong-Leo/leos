#include "os_sched_external.h"
#include "os_task_internal.h"
#include "os_def.h"
#include "os_list_external.h"
#include "os_mem_external.h"
#include "os_hwi.h"
#include "os_cpu.h"
#include "os_debug_external.h"
#include "os_sys.h"
#include "string.h"
#include "os_sem_external.h"
#include "os_process_external.h"
#include "os_process_internal.h"
#include "os_tick_external.h"
#include "os_mem_external.h"
#include "os_base_external.h"
#include "os_reset.h"
#include "os_hwi.h"

/* task分为内核线程和用户进程 */
OS_SEC_KERNEL_BSS struct OsTaskCb *g_tskCbArray;
OS_SEC_KERNEL_BSS U32 g_tskMaxNum;
OS_SEC_KERNEL_DATA struct OsList g_tskFreeList = OS_LIST_INIT(g_tskFreeList);

/* 待回收栈队列：删自己时栈不能立即释放，等软中断在系统栈上回收 */
OS_SEC_KERNEL_DATA struct OsList g_tskRecycleList = OS_LIST_INIT(g_tskRecycleList);

OS_SEC_KERNEL_TEXT U32 OsTaskConfigInit(void)
{
    U32 i;
    struct OsTaskCb *tskCb;
    size_t size;

    g_tskMaxNum = OS_TASK_MAX_NUM;
    size = sizeof(struct OsTaskCb) * g_tskMaxNum;
    g_tskCbArray = (struct OsTaskCb *)OsMemKernelAlloc(size, 4);
    if (g_tskCbArray == NULL) {
        OS_PANIC("alloc tskCbArray failed");
    }

    memset(g_tskCbArray, 0, size);

    for (i = 0; i < g_tskMaxNum; i++) {
        tskCb = OS_TASK_GET_CB(i);

        tskCb->pid = i;
        tskCb->status = 0;
        tskCb->pgDir = (uintptr_t)NULL;
        OsListInit(&tskCb->pendListNode);
        OsListInit(&tskCb->timerListNode);
        OsListInit(&tskCb->holdSemList);
        OsListInit(&tskCb->msgList);
        OsListAddTail(&g_tskFreeList, &tskCb->freeListNode);
    }

    return OS_OK;
}

OS_SEC_KERNEL_TEXT void OsTaskIdleEntry(void)
{
    while (1) {
        OsIntLock();
        OsIntUnlock();
    }
}

OS_SEC_KERNEL_TEXT struct OsTaskCb *OsTaskGetFreeCb(void)
{
    struct OsList *listNode;

    if (OsListIsEmpty(&g_tskFreeList)) {
        OS_LOG_WARN("no free task control block\n");
        return NULL;
    }

    listNode = OsListPopHead(&g_tskFreeList);

    return OS_GET_STRUCT_ENTRY(struct OsTaskCb, freeListNode, listNode);
}

/* 归还 TCB 到空闲链表（用于 fork 等场景的资源回滚） */
OS_SEC_KERNEL_TEXT void OsTaskReleaseFreeCb(struct OsTaskCb *tskCb)
{
    tskCb->status = 0;
    tskCb->pgDir = 0;
    OsListAddTail(&g_tskFreeList, &tskCb->freeListNode);
}

/* 收割 zombie 进程：释放 TCB 回空闲链表（供 waitpid 调用）
 * 必须先调用 OsTaskRecycleStk 清理回收链表，因为 zombie 的 freeListNode
 * 可能仍挂在 g_tskRecycleList 上（OsTaskDelete 中 self-delete 时放入），
 * 直接 OsListAddTail 到 g_tskFreeList 会导致同一节点挂在两条链表上，
 * 后续 OsTaskRecycleStk 遍历 g_tskRecycleList 时会因链表损坏而崩溃。 */
OS_SEC_KERNEL_TEXT void OsTaskReapZombie(struct OsTaskCb *tskCb)
{
    /* 先处理回收链表：释放 zombie 的内核栈，将其从 g_tskRecycleList 摘除 */
    OsTaskRecycleStk();

    tskCb->status = 0;
    tskCb->pgDir = 0;
    tskCb->usrFscCtrl = NULL;
    OsListAddTail(&g_tskFreeList, &tskCb->freeListNode);
}

static OS_SEC_KERNEL_TEXT void OsTaskExit(void)
{
    U32 ret = OsTaskDelete(OS_RUNNING_TASK()->pid);
    if (ret != OS_OK) {
        OS_PANIC("task exit delete failed, ret=%u\n", ret);
    }
}

OS_SEC_KERNEL_TEXT void OsTaskCommonEntry(U32 tskId)
{
    struct OsTaskCb *tskCb = OS_TASK_GET_CB(tskId);
    void **arg = tskCb->arg;

    /* 强制开中断 */
    (void)OsIntUnlock();
    tskCb->entry(arg[0], arg[1], arg[2], arg[3]);
    /* 强制关中断 */
    OsIntLock();

    OsTaskExit();
}

OS_INLINE void OsTaskSetCb(struct OsTaskCb *tskCb, struct OsTaskCreateParam *param)
{
    memcpy(tskCb->name, param->name, OS_TASK_NAME_MAX_SIZE);
    tskCb->entry = param->entryFunc;
    tskCb->prio = param->prio;
    tskCb->oriPrio = param->prio;
    tskCb->status |= OS_TASK_STATUS_USED;
    tskCb->arg[0] = param->arg[0];
    tskCb->arg[1] = param->arg[1];
    tskCb->arg[2] = param->arg[2];
    tskCb->arg[3] = param->arg[3];
}

OS_INLINE void OsTaskInitKernelStack(uintptr_t stkBase, size_t stkSize)
{
    memset((void *)stkBase, 0xCA, stkSize);
    ((U32 *)stkBase)[0] = OS_TASK_STACK_TOP_MAGIC;
}

/* 实际创建逻辑(无优先级校验),供 OsTaskCreate 与 OsTaskCreateIdle 复用 */
static OS_SEC_KERNEL_TEXT U32 OsTaskCreateInternal(struct OsTaskCreateParam *param, U32 *tskId)
{
    struct OsTaskCb *tskCb;
    uintptr_t stkMemBase;
    enum OsIntStatus intSave;

    intSave = OsIntLock();

    tskCb = OsTaskGetFreeCb();
    if (tskCb == NULL) {
        OS_LOG_ERROR("no free task CB\n");
        OsIntRestore(intSave);
        return OS_TASK_CREATE_NO_FREE_CB;
    }

    stkMemBase = (uintptr_t)OsMemKernelAlloc(OS_TASK_KERNEL_STACK_SIZE, 16);
    if (stkMemBase == NULL) {
        OS_LOG_ERROR("alloc kernel stack failed, size=0x%x\n",
                     OS_TASK_KERNEL_STACK_SIZE);
        OsIntRestore(intSave);
        return OS_TASK_CREATE_STK_ALLOC_FAIL;
    }
    tskCb->kernelStkTop = stkMemBase;

    OsTaskInitKernelStack(stkMemBase, OS_TASK_KERNEL_STACK_SIZE);

    OsTaskSetCb(tskCb, param);
    // 任务的初始时间片
    OsTaskSetTimeSlice(tskCb, OsTaskCalTimeSlice(tskCb));

    OsSetContext(stkMemBase, OS_TASK_KERNEL_STACK_SIZE, tskCb);
    tskCb->tskType = OS_TASK_THREAD;
    *tskId = tskCb->pid;

    OsIntRestore(intSave);
    return OS_OK;
}

OS_SEC_KERNEL_TEXT U32 OsTaskCreate(struct OsTaskCreateParam *param, U32 *tskId)
{
    /* 普通任务不得占用 idle 专属的最低优先级层 */
    if (param->prio >= OS_TASK_LOWEST_PRIO) {
        OS_LOG_ERROR("task prio %u invalid, must < %u\n", param->prio, OS_TASK_LOWEST_PRIO);
        return OS_TASK_CREATE_PRIO_ILL;
    }

    return OsTaskCreateInternal(param, tskId);
}

OS_SEC_KERNEL_TEXT U32 OsTaskResume(U32 tskId)
{
    struct OsTaskCb *tskCb;
    enum OsIntStatus intSave;

    if (tskId >= g_tskMaxNum) {
        return OS_TASK_TSK_ID_INVALID;
    }

    intSave = OsIntLock();
    tskCb = OS_TASK_GET_CB(tskId);
    if ((tskCb->status & OS_TASK_STATUS_USED) == 0) {
        OS_LOG_ERROR("task %u not created, status=0x%x\n", tskId, tskCb->status);
        OsIntRestore(intSave);
        return OS_TASK_RESUME_TSK_STATUS_ILL;
    }

    tskCb->status &= ~OS_TASK_STATUS_SUSPENDED;

    if (!(tskCb->status & (OS_TASK_STATUS_PENDING | OS_TASK_STATUS_PEND_MSG | OS_TASK_STATUS_IN_DELAY))) {
        OsSchedRdyListEnqueTsk(tskCb);
    }

    /* 系统未进入后台调度状态（第一次调度还没发生），不触发调度 */
    if (OS_RUN_QUE()->uniFlag & OS_BGD_TSK_MSK) {
        OsTaskSchedule();
    }

    OsIntRestore(intSave);

    return OS_OK;
}

OS_SEC_KERNEL_TEXT U32 OsTaskSuspend(U32 tskId)
{
    struct OsTaskCb *tskCb;
    enum OsIntStatus intSave;

    if (tskId >= g_tskMaxNum) {
        return OS_TASK_TSK_ID_INVALID;
    }

    tskCb = OS_TASK_GET_CB(tskId);
    intSave = OsIntLock();

    if ((tskCb->status & OS_TASK_STATUS_USED) == 0) {
        OS_LOG_ERROR("task %u not created\n", tskId);
        OsIntRestore(intSave);
        return OS_TASK_SUSPEND_TSK_STATUS_ILL;
    }

    if (tskCb->status & OS_TASK_STATUS_SUSPENDED) {
        OsIntRestore(intSave);
        return OS_OK;
    }

    if (!OsListIsEmpty(&tskCb->holdSemList)) {
        OsIntRestore(intSave);
        return OS_TASK_DELETE_HOLD_SEM;
    }

    if (tskCb->status & OS_TASK_STATUS_READY) {
        OsSchedRdyListDequeTsk(tskCb);
    }

    tskCb->status |= OS_TASK_STATUS_SUSPENDED;

    if (tskCb == OS_RUNNING_TASK()) {
        OsTaskSchedule();
    }

    OsIntRestore(intSave);
    return OS_OK;
}

OS_SEC_KERNEL_TEXT U32 OsTaskDelete(U32 tskId)
{
    struct OsTaskCb *tskCb;
    struct OsTaskCb *curTsk;
    enum OsIntStatus intSave;

    if (tskId >= g_tskMaxNum) {
        return OS_TASK_TSK_ID_INVALID;
    }

    tskCb = OS_TASK_GET_CB(tskId);
    intSave = OsIntLock();

    if ((tskCb->status & OS_TASK_STATUS_USED) == 0) {
        OsIntRestore(intSave);
        return OS_TASK_DELETE_TSK_STATUS_ILL;
    }

    /* 持有互斥信号量的任务不允许删除 */
    if (!OsListIsEmpty(&tskCb->holdSemList)) {
        OsIntRestore(intSave);
        return OS_TASK_DELETE_HOLD_SEM;
    }

    /* 从等待队列移除 */
    if (tskCb->status & OS_TASK_STATUS_PENDING) {
        OsListRemoveNode(&tskCb->pendListNode);
        tskCb->status &= ~OS_TASK_STATUS_PENDING;
    }

    /* 清除等消息标志 */
    if (tskCb->status & OS_TASK_STATUS_PEND_MSG) {
        tskCb->status &= ~OS_TASK_STATUS_PEND_MSG;
    }

    /* 从延时链表移除 */
    if (tskCb->status & OS_TASK_STATUS_IN_DELAY) {
        OsListRemoveNode(&tskCb->timerListNode);
        tskCb->status &= ~OS_TASK_STATUS_IN_DELAY;
        OsRefreshNearestTick();
    }

    /* 从就绪队列移除 */
    if (tskCb->status & OS_TASK_STATUS_READY) {
        OsSchedRdyListDequeTsk(tskCb);
    }

    /* 释放该任务消息信箱中所有未读消息 */
    while (!OsListIsEmpty(&tskCb->msgList)) {
        struct OsList *msgNode = OsListPopHead(&tskCb->msgList);
        OsMemKernelFree(msgNode);
    }

    if (tskCb->tskType == OS_TASK_PROCESS && tskCb->pgDir) {
        OsProcessFreeResources(tskCb);
    }

    /* 父进程退出时，处理子进程：
     * - zombie 子进程直接收割 TCB（完全释放）
     * - 活着的子进程 parentPid 设 0（变为孤儿，exit 时自行完全释放）
     */
    if (tskCb->tskType == OS_TASK_PROCESS) {
        U32 ci;
        for (ci = 0; ci < g_tskMaxNum; ci++) {
            struct OsTaskCb *child = &g_tskCbArray[ci];
            if (child->parentPid != tskCb->pid) continue;
            if (!(child->status & OS_TASK_STATUS_USED)) continue;
            if (child->status & OS_TASK_STATUS_ZOMBIE) {
                /* 收割 zombie 子进程 */
                OsTaskReapZombie(child);
            } else {
                /* 子进程还在运行，设为孤儿 */
                child->parentPid = 0;
            }
        }
    }

    /* 保留 ZOMBIE 位（exit 时设置的），其余状态位清零 */
    if (tskCb->status & OS_TASK_STATUS_ZOMBIE) {
        tskCb->status = OS_TASK_STATUS_USED | OS_TASK_STATUS_ZOMBIE;
    } else {
        tskCb->status = 0;
    }

    /* 唤醒等待此任务的父进程（子进程 exit 时 ZOMBIE 已设，OsTaskDelete 中走到这里） */
    if (tskCb->status & OS_TASK_STATUS_ZOMBIE) {
        U32 wi;
        for (wi = 0; wi < g_tskMaxNum; wi++) {
            struct OsTaskCb *waiter = &g_tskCbArray[wi];
            if (!(waiter->status & OS_TASK_STATUS_PENDING)) continue;
            if (waiter->waitPid == 0) continue;
            if (waiter->waitPid == tskCb->pid || waiter->waitPid == OS_WAIT_ANY_CHILD) {
                waiter->status &= ~OS_TASK_STATUS_PENDING;
                waiter->status |= OS_TASK_STATUS_READY;
                OsSchedRdyListEnqueTsk(waiter);
                break;
            }
        }
    }

    curTsk = OS_RUNNING_TASK();

    if (tskCb == curTsk) {
        OsListAddTail(&g_tskRecycleList, &tskCb->freeListNode);
        OsTaskSchedule();
    } else {
        OsMemKernelFree((void *)tskCb->kernelStkTop);
        OsListAddTail(&g_tskFreeList, &tskCb->freeListNode);
    }

    OsIntRestore(intSave);
    return OS_OK;
}

/* 回收待回收栈和TCB（在OsHwiTail系统栈上调用，安全回收） */
OS_SEC_KERNEL_TEXT void OsTaskRecycleStk(void)
{
    struct OsList *node;
    struct OsTaskCb *tskCb;

    while (!OsListIsEmpty(&g_tskRecycleList)) {
        node = OsListPopHead(&g_tskRecycleList);
        tskCb = OS_GET_STRUCT_ENTRY(struct OsTaskCb, freeListNode, node);

        /* 回收栈 */
        OsMemKernelFree((void *)tskCb->kernelStkTop);
        tskCb->kernelStkTop = 0;

        if (tskCb->status & OS_TASK_STATUS_ZOMBIE) {
            if (tskCb->parentPid == 0) {
                /* 孤儿 zombie：无父进程可 waitpid，直接回收 TCB */
                tskCb->status = 0;
                tskCb->pgDir = 0;
                tskCb->usrFscCtrl = NULL;
                OsListAddTail(&g_tskFreeList, &tskCb->freeListNode);
            }
            /* 有父进程：保留 ZOMBIE，等父进程 waitpid 收割 */
        } else {
            /* 回收TCB */
            tskCb->status = 0;
            tskCb->pgDir = 0;
            OsListAddTail(&g_tskFreeList, &tskCb->freeListNode);
        }
    }
}


OS_SEC_KERNEL_TEXT U32 OsTaskCreateIdle(void)
{
    U32 ret;
    struct OsTaskCreateParam param = {0};
    U32 idleTskId;

    strcpy(param.name, "idle");
    param.prio = OS_TASK_LOWEST_PRIO;
    param.entryFunc = OsTaskIdleEntry;

    ret = OsTaskCreateInternal(&param, &idleTskId);
    if (ret != OS_OK) {
        OS_LOG_ERROR("create idle task failed, ret=%u\n", ret);
        return ret;
    }

    OS_RUN_QUE()->idleTsk = OS_TASK_GET_CB(idleTskId);

    return OS_OK;
}

OS_SEC_KERNEL_TEXT void OsTaskSchedule(void)
{
    struct OsRunQue *rq = OS_RUN_QUE();

    if (!rq->needSched) {
        return;
    }

    /* 系统操作中，不要调度 */
    if (OS_SYS_ACTIVE(rq->uniFlag)) {
        return;
    }

    OsTrapTsk(OS_RUNNING_TASK());
}

OS_SEC_KERNEL_TEXT void OsTaskTimerListInsert(struct OsTaskCb *tsk)
{
    struct OsList *timerList = &g_timerList;
    struct OsList *tmpNode;
    struct OsTaskCb *tmpTsk;

    if (OsListIsEmpty(timerList)) {
        OsListAddTail(timerList, &tsk->timerListNode);
    } else {
        OS_LIST_FOR_EACH(timerList, tmpNode) {
            tmpTsk = OS_GET_STRUCT_ENTRY(struct OsTaskCb, timerListNode, tmpNode);
            if (tsk->expiredTick < tmpTsk->expiredTick) {
                break;
            }
        }

        OsListInsertPrev(&tsk->timerListNode, tmpNode);
    }

    OsRefreshNearestTick();
}

OS_SEC_KERNEL_TEXT U32 OsTaskDelay(U32 ticks)
{
    struct OsTaskCb *tsk;
    enum OsIntStatus intSave;

    if (ticks == 0) {
        OS_LOG_ERROR("ticks cannot be 0\n");
        return OS_TASK_DELAY_PARAM_ILL;
    }

    intSave = OsIntLock();
    tsk = OS_RUNNING_TASK();

    tsk->expiredTick = g_uniTicks + ticks;

    OsSchedRdyListDequeTsk(tsk);
    OsTaskTimerListInsert(tsk);

    tsk->status |= OS_TASK_STATUS_IN_DELAY;

    OsTaskSchedule();

    tsk->status &= ~OS_TASK_STATUS_IN_DELAY;

    OsIntRestore(intSave);

    return OS_OK;
}