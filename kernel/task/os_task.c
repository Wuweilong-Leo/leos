#include "os_sched_external.h"
#include "os_task_internal.h"
#include "os_def.h"
#include "os_list_external.h"
#include "os_mem_external.h"
#include "os_hwi_i386.h"
#include "os_context_i386.h"
#include "os_debug_external.h"
#include "os_sys.h"
#include "string.h"
#include "os_sem_external.h"
#include "os_process_external.h"
#include "os_tick_external.h"

/* task分为内核线程和用户进程 */
OS_SEC_KERNEL_BSS struct OsTaskCb g_tskCbArray[OS_TASK_MAX_NUM];
OS_SEC_KERNEL_DATA struct OsList g_tskFreeList = OS_LIST_INIT(g_tskFreeList);

OS_SEC_KERNEL_TEXT void OsTaskConfig(void)
{
    U32 i;
    struct OsTaskCb *tskCb;

    for (i = 0; i < OS_TASK_MAX_NUM; i++) {
        tskCb = OS_TASK_GET_CB(i);

        tskCb->pid = i;
        tskCb->status = OS_TASK_NOT_CREATE;
        tskCb->pgDir = NULL;
        OsListInit(&tskCb->semList);
        OsListInit(&tskCb->pendListNode);
        OsListInit(&tskCb->dlyListNode);
        OsListAddTail(&g_tskFreeList, &tskCb->freeListNode);
    }
}

OS_SEC_KERNEL_TEXT void Process1(void *para1, void *param2)
{
    while (1) {
        OsIntLock();
        OS_DEBUG_PRINT_STR("process1\n");
        OsIntUnlock();
    }
}

OS_SEC_KERNEL_TEXT void OsTaskIdleEntry(void)
{
    struct OsTaskCreateParam param = {0};
    U32 processId;

    param.entryFunc = Process1;
    param.prio = 10;

    OsTaskCreate(&param, &processId);
    OsTaskResume(processId);

    while (1) {
        OsIntLock();
        OS_DEBUG_PRINT_STR("idle\n");
        OsIntUnlock();
    }
}

OS_INLINE struct OsTaskCb *OsTaskGetFreeCb(void)
{
    struct OsList *listNode;

    if (OsListIsEmpty(&g_tskFreeList)) {
        return NULL;
    }
    
    listNode = OsListPopHead(&g_tskFreeList);

    return OS_LIST_GET_STRUCT_ENTRY(struct OsTaskCb, freeListNode, listNode);
}

static OS_SEC_KERNEL_TEXT void OsTaskExit(void)
{
    struct OsTaskCb *tsk = OS_RUNNING_TASK();

    // 任务结束，从就绪队列里删除
    OsDequeTskFromRdyList(tsk);
    tsk->status = OS_TASK_NOT_CREATE;

    OsTaskSchedule();
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
    tskCb->status = OS_TASK_NOT_RESUME;
    tskCb->arg[0] = param->arg[0];
    tskCb->arg[1] = param->arg[1];
    tskCb->arg[2] = param->arg[2];
    tskCb->arg[3] = param->arg[3];
}

OS_INLINE void OsTaskInitKernelStack(uintptr_t stkBase, U32 stkSize)
{
    U32 i;

    for (i = 0; i < stkSize / 4; i++) {
        ((U32 *)stkBase)[i] = 0xCACACACA;
    }

    ((U32 *)stkBase)[0] = OS_TASK_STACK_TOP_MAGIC;
}

OS_SEC_KERNEL_TEXT U32 OsTaskCreate(struct OsTaskCreateParam * param, U32 *tskId)
{
    U32 ret;
    struct OsTaskCb *tskCb;
    uintptr_t stkMemBase;
    enum OsIntStatus intSave;

    intSave = OsIntLock();

    tskCb = OsTaskGetFreeCb();
    if (tskCb == NULL) {
        OS_DEBUG_KPRINT("%s\n", "OsTaskCreate: OsTaskGetFreeCb failed");
        OsIntRestore(intSave);
        return OS_TASK_CREATE_NO_FREE_CB;
    }
    
    OS_DEBUG_KPRINT("OsTaskCreate: freeCb = 0x%x\n", (U32)tskCb);

    stkMemBase = (uintptr_t)OsMemKernelAllocPgs(OS_TASK_KERNEL_STACK_SIZE / OS_PG_SIZE);
    if (stkMemBase == NULL) {
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
    if ((tskCb->status != OS_TASK_NOT_RESUME) && (tskCb->status != OS_TASK_IN_SUSPEND)) {
        OsIntRestore(intSave);
        return OS_TASK_RESUME_TSK_STATUS_ILL;
    }

    OsEnqueTskToRdyListTail(tskCb);
    tskCb->status = OS_TASK_READY;

    OsTaskSchedule();

    OsIntRestore(intSave);

    return OS_OK;
}

OS_INLINE void OsTaskMakeIdleRdy(struct OsTaskCb *idleTskCb)
{
    OsEnqueTskToRdyListTail(idleTskCb);
    idleTskCb->status = OS_TASK_READY;
}

OS_SEC_KERNEL_TEXT U32 OsTaskCreateIdle(U32 *tskId)
{
    U32 ret;
    struct OsTaskCreateParam param = {0};
    struct OsTaskCb *idleTskCb;

    strcpy(param.name, "idle");
    param.prio = OS_TASK_LOWEST_PRIO;
    param.entryFunc = OsTaskIdleEntry;

    ret = OsTaskCreate(&param, tskId);
    if (ret != OS_OK) {
        return ret;
    }
    OS_DEBUG_KPRINT("idle tskId == 0x%x\n", *tskId);

    idleTskCb = OS_TASK_GET_CB(*tskId);

    OsTaskMakeIdleRdy(idleTskCb);

    return OS_OK;
}

OS_SEC_KERNEL_TEXT void OsTaskSchedule(void)
{
    if (!OS_RUN_QUE()->needSched) {
        return;
    }

    /* 系统操作中，不要调度 */
    if (OS_SYS_ACTIVE(OS_RUN_QUE()->uniFlag)) {
        return;
    }

    OsTrapTsk(OS_RUNNING_TASK());
}

OS_INLINE bool OsTaskHoldsSem(struct OsTaskCb *tsk)
{
    return !OsListIsEmpty(&tsk->semList); 
}

OS_SEC_KERNEL_TEXT U32 OsTaskSuspend(U32 tskId)
{
    struct OsTaskCb *tsk;
    enum OsIntStatus intSave;

    intSave = OsIntLock();

    tsk = OS_TASK_GET_CB(tskId);

    /* 只允许挂起ready的任务*/
    if ((!OS_TASK_IS_RDY(tsk)) && (!OS_TASK_IS_RUNNING(tsk))) {
        OsIntRestore(intSave);
        return OS_TASK_SUSPEND_TSK_STATUS_ILL;
    }

    /* 如果持有信号量，不允许挂起，不然会死锁 */
    if (OsTaskHoldsSem(tsk)) {
        OsIntRestore(intSave);
        return OS_TASK_SUSPEND_TSK_HOLD_SEM;
    }

    /* 如果是ready状态一定在就绪队列里, 直接删除 */
    OsDequeTskFromRdyList(tsk);
    tsk->status = OS_TASK_IN_SUSPEND;

    /* 可能删除的是运行任务，尝试调度 */
    OsTaskSchedule();

    OsIntRestore(intSave);

    return OS_OK;
}

OS_SEC_KERNEL_TEXT U32 OsTaskYield(void)
{
    enum OsIntStatus intSave;
    struct OsTaskCb *curTsk;

    intSave = OsIntLock();

    curTsk = OS_RUNNING_TASK();

    /* 如果有持有信号量，不能让步，不然死锁 */
    if (OsTaskHoldsSem(curTsk)) {
        OsIntRestore(intSave);
        return OS_TASK_YIELD_TSK_HOLD_SEM;
    }

    /* 只能让步给同一优先级的任务 */
    OsDequeTskFromRdyList(curTsk);
    /* 重新加入尾部 */
    OsEnqueTskToRdyListTail(curTsk);

    /* 触发调度 */
    OsTaskSchedule();

    OsIntRestore(intSave);

    return OS_OK;
}

static OS_SEC_KERNEL_TEXT void OsInsertTskToDlyList(struct OsTaskCb *tsk)
{
    struct OsRunQue *rq = OS_RUN_QUE();
    struct OsList *dlyList = &rq->dlyList;
    struct OsList *tmpNode;
    struct OsTaskCb *tmpTsk;

    if (OsListIsEmpty(dlyList)) {
        // 空的加入尾部就行
        OsListAddTail(dlyList, &tsk->dlyListNode);
    } else {
        OS_LIST_FOR_EACH(dlyList, tmpNode) {
            tmpTsk = OS_LIST_GET_STRUCT_ENTRY(struct OsTaskCb, dlyListNode, tmpNode);
            if (tsk->expiredTick < tmpTsk->expiredTick) {
                break;
            }
        }

        // 如果expiredTick大于所有任务，tmpNode == dlyList, 插入到dlyList前即可。
        OsListInsertPrev(&tsk->dlyListNode, tmpNode);
    }

    OsRefreshNearestTick(rq);
}

OS_SEC_KERNEL_TEXT U32 OsTaskDelay(U32 ticks)
{
    struct OsTaskCb *tsk;
    enum OsIntStatus intSave;

    if (ticks == 0) {
        return OS_TASK_DELAY_PARAM_ILL;
    }

    intSave = OsIntLock();
    tsk = OS_RUNNING_TASK();

    tsk->expiredTick = g_uniTicks + ticks;

    OsDequeTskFromRdyList(tsk);
    OsInsertTskToDlyList(tsk);

    tsk->status = OS_TASK_IN_DELAY;

    OsTaskSchedule();

    OsIntRestore(intSave);

    return OS_OK;
}

OS_SEC_KERNEL_TEXT U32 OsTaskSetPrio(U32 tskId, U32 prio)
{
    struct OsTaskCb *tsk;
    enum OsIntStatus intSave;

    if (prio >= OS_TASK_LOWEST_PRIO) {
        return OS_TASK_SET_PRIO_PARAM_ILL;
    }

    intSave = OsIntLock();

    tsk = OS_TASK_GET_CB(tskId);
    tsk->prio = prio;

    /* 在就绪队列里 */
    if (OS_TASK_IS_RDY(tsk) || OS_TASK_IS_RUNNING(tsk)) {
        OsDequeTskFromRdyList(tsk);
        OsEnqueTskToRdyListTail(tsk);
    }

    OsIntRestore(intSave);
    return OS_OK;
}