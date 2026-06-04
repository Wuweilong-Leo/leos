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
        OS_LOG_ERROR("OsTaskConfigInit: alloc tskCbArray failed\n");
        while (1) {}
    }

    memset(g_tskCbArray, 0, size);

    for (i = 0; i < g_tskMaxNum; i++) {
        tskCb = OS_TASK_GET_CB(i);

        tskCb->pid = i;
        tskCb->status = 0;
        tskCb->pgDir = (uintptr_t)NULL;
        OsListInit(&tskCb->semList);
        OsListInit(&tskCb->pendListNode);
        OsListInit(&tskCb->dlyListNode);
        OsListAddTail(&g_tskFreeList, &tskCb->freeListNode);
    }

    return OS_OK;
}

OS_SEC_KERNEL_TEXT void Process1(void *para1, void *param2, void *param3, void *param4)
{
    while (1) {
        OsIntLock();
        OS_DEBUG_PRINT_STR("process1\n");
        OsIntUnlock();
    }
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

    // 任务结束，从就绪队列里删除
    OsSchedRdyListDequeTsk(tsk);
    tsk->status = 0;

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

OS_SEC_KERNEL_TEXT U32 OsTaskCreate(struct OsTaskCreateParam * param, U32 *tskId)
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
        OS_LOG_ERROR("OsTaskCreate: alloc kernel stack failed, size=0x%x\n", OS_TASK_KERNEL_STACK_SIZE);
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
    if ((tskCb->status & OS_TASK_STATUS_READY) != 0) {
        OS_LOG_ERROR("OsTaskResume: task %u already ready, status=0x%x\n", tskId, tskCb->status);
        OsIntRestore(intSave);
        return OS_TASK_RESUME_TSK_STATUS_ILL;
    }

    OsSchedRdyListEnqueTsk(tskCb);

    OsTaskSchedule();

    OsIntRestore(intSave);

    return OS_OK;
}

OS_INLINE void OsTaskMakeIdleRdy(struct OsTaskCb *idleTskCb)
{
    OsSchedRdyListEnqueTsk(idleTskCb);
}

OS_SEC_KERNEL_TEXT U32 OsTaskCreateIdle(void)
{
    U32 ret;
    struct OsTaskCreateParam param = {0};
    struct OsTaskCb *idleTskCb;
    U32 idleTskId;

    strcpy(param.name, "idle");
    param.prio = OS_TASK_LOWEST_PRIO;
    param.entryFunc = OsTaskIdleEntry;

    ret = OsTaskCreate(&param, &idleTskId);
    if (ret != OS_OK) {
        OS_LOG_ERROR("OsTaskCreateIdle: create idle task failed, ret=%u\n", ret);
        return ret;
    }
    idleTskCb = OS_TASK_GET_CB(idleTskId);

    /* 先设 runningTsk，避免 OsSchedRdyListEnqueTsk 热路径判 NULL */
    OS_RUN_QUE()->runningTsk = idleTskCb;
    OsTaskMakeIdleRdy(idleTskCb); // 把idle加入就绪队列但是不调度
    OS_RUN_QUE()->idleTsk = idleTskCb;

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

static OS_SEC_KERNEL_TEXT void OsTaskDlyListInsert(struct OsTaskCb *tsk)
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
            tmpTsk = OS_GET_STRUCT_ENTRY(struct OsTaskCb, dlyListNode, tmpNode);
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
        OS_LOG_ERROR("OsTaskDelay: ticks cannot be 0\n");
        return OS_TASK_DELAY_PARAM_ILL;
    }

    intSave = OsIntLock();
    tsk = OS_RUNNING_TASK();

    tsk->expiredTick = g_uniTicks + ticks;

    OsSchedRdyListDequeTsk(tsk);
    OsTaskDlyListInsert(tsk);

    tsk->status |= OS_TASK_STATUS_IN_DELAY;

    OsTaskSchedule();

    tsk->status &= ~OS_TASK_STATUS_IN_DELAY;

    OsIntRestore(intSave);

    return OS_OK;
}