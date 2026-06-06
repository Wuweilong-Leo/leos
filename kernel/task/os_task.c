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

/* task分为内核线程和用户进程 */
OS_SEC_KERNEL_BSS struct OsTaskCb *g_tskCbArray;
OS_SEC_KERNEL_BSS U32 g_tskMaxNum;
OS_SEC_KERNEL_DATA struct OsList g_tskFreeList = OS_LIST_INIT(g_tskFreeList);

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
        OsListInit(&tskCb->semList);
        OsListInit(&tskCb->pendListNode);
        OsListInit(&tskCb->timerListNode);
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

    OsSchedRdyListEnqueTsk(tskCb);

    OsTaskSchedule();

    OsIntRestore(intSave);

    return OS_OK;
}

OS_SEC_KERNEL_TEXT U32 OsTaskSuspend(U32 tskId)
{
    struct OsTaskCb *tskCb;
    enum OsIntStatus intSave;

    intSave = OsIntLock();
    tskCb = OS_TASK_GET_CB(tskId);

    if ((tskCb->status & OS_TASK_STATUS_USED) == 0) {
        OS_LOG_ERROR("OsTaskSuspend: task %u not created, status=0x%x\n", tskId, tskCb->status);
        OsIntRestore(intSave);
        return OS_TASK_SUSPEND_TSK_STATUS_ILL;
    }

    /* 不能挂起自己 */
    if (tskCb == OS_RUNNING_TASK()) {
        OS_LOG_ERROR("OsTaskSuspend: cannot suspend running task %u\n", tskId);
        OsIntRestore(intSave);
        return OS_TASK_SUSPEND_TSK_STATUS_ILL;
    }

    /* 持有信号量时不允许挂起 */
    if (!OsListIsEmpty(&tskCb->semList)) {
        OS_LOG_ERROR("OsTaskSuspend: task %u holds semaphore\n", tskId);
        OsIntRestore(intSave);
        return OS_TASK_SUSPEND_TSK_HOLD_SEM;
    }

    /* 从就绪队列移除 */
    if (tskCb->status & OS_TASK_STATUS_READY) {
        OsSchedRdyListDequeTsk(tskCb);
    }

    /* 从延时链表移除 */
    if (tskCb->status & OS_TASK_STATUS_IN_DELAY) {
        OsListRemoveNode(&tskCb->timerListNode);
        tskCb->status &= ~OS_TASK_STATUS_IN_DELAY;
        OsRefreshNearestTick();
    }

    /* 从信号量等待队列移除 */
    if (tskCb->status & OS_TASK_STATUS_PENDING) {
        OsListRemoveNode(&tskCb->pendListNode);
        tskCb->status &= ~OS_TASK_STATUS_PENDING;
    }

    tskCb->status &= ~(OS_TASK_STATUS_READY | OS_TASK_STATUS_RUNNING);

    OsTaskSchedule();
    OsIntRestore(intSave);
    return OS_OK;
}

OS_SEC_KERNEL_TEXT U32 OsTaskDelete(U32 tskId)
{
    struct OsTaskCb *tskCb;
    enum OsIntStatus intSave;

    intSave = OsIntLock();
    tskCb = OS_TASK_GET_CB(tskId);

    if ((tskCb->status & OS_TASK_STATUS_USED) == 0) {
        OS_LOG_ERROR("OsTaskDelete: task %u not created, status=0x%x\n", tskId, tskCb->status);
        OsIntRestore(intSave);
        return OS_TASK_SUSPEND_TSK_STATUS_ILL;
    }

    /* 不能删除自己 */
    if (tskCb == OS_RUNNING_TASK()) {
        OS_LOG_ERROR("OsTaskDelete: cannot delete running task %u\n", tskId);
        OsIntRestore(intSave);
        return OS_TASK_SUSPEND_TSK_STATUS_ILL;
    }

    /* 持有信号量时不允许删除 */
    if (!OsListIsEmpty(&tskCb->semList)) {
        OS_LOG_ERROR("OsTaskDelete: task %u holds semaphore\n", tskId);
        OsIntRestore(intSave);
        return OS_TASK_SUSPEND_TSK_HOLD_SEM;
    }

    /* 从就绪队列移除 */
    if (tskCb->status & OS_TASK_STATUS_READY) {
        OsSchedRdyListDequeTsk(tskCb);
    }

    /* 从延时链表移除 */
    if (tskCb->status & OS_TASK_STATUS_IN_DELAY) {
        OsListRemoveNode(&tskCb->timerListNode);
        tskCb->status &= ~OS_TASK_STATUS_IN_DELAY;
        OsRefreshNearestTick();
    }

    /* 从信号量等待队列移除 */
    if (tskCb->status & OS_TASK_STATUS_PENDING) {
        OsListRemoveNode(&tskCb->pendListNode);
        tskCb->status &= ~OS_TASK_STATUS_PENDING;
    }

    /* 释放内核栈 */
    OsMemKernelFree(tskCb->kernelStkTop);

    /* 如果是进程，释放页目录和用户虚拟内存 */
    if (tskCb->tskType == OS_TASK_PROCESS) {
        if (tskCb->pgDir != (uintptr_t)NULL) {
            /* TODO: 释放进程页目录和用户空间映射 */
            tskCb->pgDir = (uintptr_t)NULL;
        }
    }

    /* 清空 TCB，归还 freeList */
    memset(tskCb, 0, sizeof(struct OsTaskCb));
    tskCb->pid = tskId;
    OsListInit(&tskCb->freeListNode);
    OsListInit(&tskCb->pendListNode);
    OsListInit(&tskCb->timerListNode);
    OsListInit(&tskCb->semList);
    OsListAddTail(&g_tskFreeList, &tskCb->freeListNode);

    OsTaskSchedule();
    OsIntRestore(intSave);
    return OS_OK;
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