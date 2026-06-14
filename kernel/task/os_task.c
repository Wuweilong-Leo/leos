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
        OS_PANIC("%s", "OsTaskConfigInit: alloc tskCbArray failed");
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

OS_INLINE struct OsTaskCb *OsTaskGetFreeCb(void)
{
    struct OsList *listNode;

    if (OsListIsEmpty(&g_tskFreeList)) {
        OS_LOG_WARN("OsTaskGetFreeCb: no free task control block\n");
        return NULL;
    }

    listNode = OsListPopHead(&g_tskFreeList);

    return OS_GET_STRUCT_ENTRY(struct OsTaskCb, freeListNode, listNode);
}

static OS_SEC_KERNEL_TEXT void OsTaskExit(void)
{
    struct OsTaskCb *tsk = OS_RUNNING_TASK();

    OsSchedRdyListDequeTsk(tsk);

    tsk->status = 0;
    OsListInit(&tsk->freeListNode);
    OsListAddTail(&g_tskFreeList, &tsk->freeListNode);

    OsTrapTsk(tsk);
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

OS_SEC_KERNEL_TEXT U32 OsTaskCreate(struct OsTaskCreateParam *param, U32 *tskId)
{
    U32 ret;
    struct OsTaskCb *tskCb;
    uintptr_t stkMemBase;
    enum OsIntStatus intSave;

    intSave = OsIntLock();

    tskCb = OsTaskGetFreeCb();
    if (tskCb == NULL) {
        OS_LOG_ERROR("OsTaskCreate: no free task CB\n");
        OsIntRestore(intSave);
        return OS_TASK_CREATE_NO_FREE_CB;
    }

    stkMemBase = (uintptr_t)OsMemKernelAlloc(OS_TASK_KERNEL_STACK_SIZE, 16);
    if (stkMemBase == NULL) {
        OS_LOG_ERROR("OsTaskCreate: alloc kernel stack failed, size=0x%x\n",
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

OS_SEC_KERNEL_TEXT U32 OsTaskResume(U32 tskId)
{
    struct OsTaskCb *tskCb;
    enum OsIntStatus intSave;

    intSave = OsIntLock();
    tskCb = OS_TASK_GET_CB(tskId);
    if ((tskCb->status & OS_TASK_STATUS_USED) == 0) {
        OS_LOG_ERROR("OsTaskResume: task %u not created, status=0x%x\n", tskId, tskCb->status);
        OsIntRestore(intSave);
        return OS_TASK_RESUME_TSK_STATUS_ILL;
    }

    tskCb->status &= ~OS_TASK_STATUS_SUSPENDED;

    /* 如果任务不在等待状态，加入就绪队列 */
    if (!(tskCb->status & (OS_TASK_STATUS_PENDING | OS_TASK_STATUS_IN_DELAY))) {
        OsSchedRdyListEnqueTsk(tskCb);
    }

    OsTaskSchedule();

    OsIntRestore(intSave);

    return OS_OK;
}

/*
 * 从调度系统移除任务（只检查状态，不从延时/等待链表移除）
 * 返回: OS_OK 成功, 错误码 失败
 */
static OS_SEC_KERNEL_TEXT U32 OsTaskRemoveFromSched(struct OsTaskCb *tskCb)
{
    if ((tskCb->status & OS_TASK_STATUS_USED) == 0) {
        OS_LOG_ERROR("task %u not created\n", tskCb->pid);
        return OS_TASK_SUSPEND_TSK_STATUS_ILL;
    }

    if (tskCb == OS_RUNNING_TASK()) {
        OS_LOG_ERROR("cannot operate on running task %u\n", tskCb->pid);
        return OS_TASK_SUSPEND_TSK_STATUS_ILL;
    }

    /* signal量无持有者，不需要检查 */

    /* 持有互斥信号量的任务不允许删除 */
    if (!OsListIsEmpty(&tskCb->holdSemList)) {
        return OS_TASK_DELETE_HOLD_SEM;
    }

    if (tskCb->status & OS_TASK_STATUS_READY) {
        OsSchedRdyListDequeTsk(tskCb);
    }

    return OS_OK;
}

OS_SEC_KERNEL_TEXT U32 OsTaskSuspend(U32 tskId)
{
    struct OsTaskCb *tskCb = OS_TASK_GET_CB(tskId);
    enum OsIntStatus intSave = OsIntLock();
    U32 ret = OsTaskRemoveFromSched(tskCb);

    if (ret != OS_OK) {
        OsIntRestore(intSave);
        return ret;
    }

    tskCb->status |= OS_TASK_STATUS_SUSPENDED;
    OsTaskSchedule();
    OsIntRestore(intSave);
    return OS_OK;
}

OS_SEC_KERNEL_TEXT U32 OsTaskDelete(U32 tskId)
{
    struct OsTaskCb *tskCb = OS_TASK_GET_CB(tskId);
    struct OsTaskCb *curTsk;
    enum OsIntStatus intSave = OsIntLock();

    if ((tskCb->status & OS_TASK_STATUS_USED) == 0) {
        OsIntRestore(intSave);
        return OS_TASK_DELETE_TSK_STATUS_ILL;
    }

    /* 持有互斥信号量的任务不允许删除 */
    if (!OsListIsEmpty(&tskCb->holdSemList)) {
        OsIntRestore(intSave);
        return OS_TASK_DELETE_HOLD_SEM;
    }

    curTsk = OS_RUNNING_TASK();

    if (tskCb == curTsk) {
        /* 删除自己：标记待删除，从就绪队列移出，栈和TCB延迟回收 */

        /* 从就绪队列移出 */
        if (tskCb->status & OS_TASK_STATUS_READY) {
            OsSchedRdyListDequeTsk(tskCb);
        }

        /* 标记已删除，不会被调度选中，也不算USED */
        tskCb->status = OS_TASK_STATUS_DELETED;
        tskCb->pgDir = 0;

        /* 挂入回收队列，等时钟中断在系统栈上回收栈和TCB */
        OsListAddTail(&g_tskRecycleList, &tskCb->freeListNode);

        /* 切走，不会再回来 */
        OsTaskSchedule();
    }

    /* 删除其他任务 */

    /* 从等待队列移除 */
    if (tskCb->status & OS_TASK_STATUS_PENDING) {
        OsListRemoveNode(&tskCb->pendListNode);
        tskCb->status &= ~OS_TASK_STATUS_PENDING;
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

    OsMemKernelFree((void *)tskCb->kernelStkTop);

    if (tskCb->tskType == OS_TASK_PROCESS && tskCb->pgDir) {
        /* TODO: 释放进程页目录和用户空间映射 */
    }

    tskCb->status = 0;
    tskCb->pgDir = 0;
    OsListInit(&tskCb->freeListNode);
    OsListAddTail(&g_tskFreeList, &tskCb->freeListNode);

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

        /* 回收TCB */
        tskCb->status = 0;
        tskCb->pgDir = 0;
        OsListAddTail(&g_tskFreeList, &tskCb->freeListNode);
    }
}

/* 保留软中断handler兼容（不再使用） */
OS_SEC_KERNEL_TEXT void OsTaskRecycleHandler(U32 hwiNum)
{
    (void)hwiNum;
}

OS_SEC_KERNEL_TEXT U32 OsTaskCreateIdle(void)
{
    U32 ret;
    struct OsTaskCreateParam param = {0};
    U32 idleTskId;

    strcpy(param.name, "idle");
    param.prio = OS_TASK_LOWEST_PRIO;
    param.entryFunc = OsTaskIdleEntry;

    ret = OsTaskCreate(&param, &idleTskId);
    if (ret != OS_OK) {
        OS_LOG_ERROR("OsTaskCreateIdle: create idle task failed, ret=%u\n", ret);
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
        OS_LIST_FOR_EACH(timerList, tmpNode)
        {
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
        OS_LOG_ERROR("OsTaskDelay: ticks cannot be 0\n");
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