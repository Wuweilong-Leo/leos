#include "os_sched_internal.h"
#include "os_list_external.h"
#include "string.h"
#include "os_debug_external.h"
#include "os_sys.h"

OS_SEC_KERNEL_BSS struct OsRunQue g_runQue;

OS_SEC_KERNEL_DATA struct OsScheduler g_rtScheduler = {
    .pickNextTsk = OsSchedPickHighestPrioTsk
};

/* Multilevel Feedback Queue Scheduling */
OS_SEC_KERNEL_DATA struct OsScheduler g_mfqsScheduler = {
    .pickNextTsk = OsSchedPickHighestPrioTsk
};

OS_INLINE U32 OsSchedGetHighestPrio(void)
{
    struct OsRunQue *rq = OS_RUN_QUE();
    U32 bit = 0;

    /* 保证总有一个任务ready */
    while ((rq->rdyListMsk & (1 << bit)) == 0) {
        bit++;
    }

    return bit;
}

OS_SEC_KERNEL_TEXT struct OsTaskCb *OsSchedPickHighestPrioTsk(void)
{
    struct OsList *rdyList;
    U32 highestPrio = OsSchedGetHighestPrio();
    struct OsList *rdyListNode;

    rdyList = &OS_RUN_QUE()->rdyList[highestPrio];
    rdyListNode = OS_LIST_GET_FIRST_NODE(rdyList);

    return OS_LIST_GET_STRUCT_ENTRY(struct OsTaskCb, rdyListNode, rdyListNode);
}

OS_SEC_KERNEL_TEXT void OsSchedConfig(void)
{
    struct OsRunQue *rq = OS_RUN_QUE();
    U32 i;

    rq->runningTsk = NULL;
    rq->rdyListMsk = 0;
    for (i = 0; i < OS_TASK_PRIO_MAX_NUM; i++) {
        OsListInit(&rq->rdyList[i]);
    }
    OsListInit(&rq->delayList);
    rq->intCount = 0;
    rq->scheduler = &g_mfqsScheduler;
    rq->needSched = FALSE;
}

/* 外部关中断 */
OS_SEC_KERNEL_TEXT void OsSchedAddTskToRdyListTail(struct OsTaskCb *tsk)
{
    struct OsRunQue *rq = OS_RUN_QUE();
    U32 tskPrio = tsk->prio;
    struct OsList *rdyList = &rq->rdyList[tskPrio];

    OsListAddTail(rdyList, &tsk->rdyListNode);

    rq->rdyListMsk |= (1 << tskPrio);

    if (tskPrio < OS_RUNNING_TASK()->prio) {
        rq->needSched = TRUE;
    }
}

OS_SEC_KERNEL_TEXT void OsSchedDelTskFromRdyList(struct OsTaskCb* tsk)
{
    struct OsRunQue *rq = OS_RUN_QUE();
    U32 prio = tsk->prio;
    struct OsList *rdyList = &rq->rdyList[prio];

    OsListRemoveNode(&tsk->rdyListNode);

    if (OsListIsEmpty(rdyList)) {
        rq->rdyListMsk &= ~(1 << prio);
    }

    if (tsk == rq->runningTsk) {
        rq->needSched = TRUE;
    }
}

OS_SEC_KERNEL_TEXT void OsSchedModifyTskPrio(struct OsTaskCb *tsk)
{
    tsk->prio = (tsk->prio + 1) % OS_TASK_PRIO_MAX_NUM;
}

OS_SEC_KERNEL_TEXT void OsSchedMain(void)
{
    struct OsRunQue *rq = OS_RUN_QUE();
    struct OsScheduler *scheduler = rq->scheduler;
    struct OsTaskCb *curTsk = OS_RUNNING_TASK();
    struct OsTaskCb *nextTsk = curTsk;

    if (rq->needSched && OS_HWI_NOT_ACTIVE(rq->uniFlag)) {
        rq->needSched = FALSE;
        nextTsk = scheduler->pickNextTsk();
        nextTsk->status = OS_TASK_RUNNING;
        /* 任务切换时的必要的架构配置 */
        OsConfigArchForTskSwitch(nextTsk);
        rq->runningTsk = nextTsk;
    }

    OsLoadTsk(nextTsk);
}

OS_SEC_KERNEL_TEXT void OsSchedSwitchIdle(void)
{
    U32 idleTskId;
    struct OsTaskCb *idleTskCb;
    struct OsRunQue *rq = OS_RUN_QUE();

    /* 创建idle task */
    if (OsTaskCreateIdle(&idleTskId) != OS_OK) {
        OS_DEBUG_KPRINT("%s\n", "create idle err\n");
        return;
    }

    idleTskCb = OS_TASK_GET_CB(idleTskId);

    rq->idleTsk = idleTskCb;
    rq->runningTsk = idleTskCb;
    idleTskCb->status = OS_TASK_RUNNING;

    OsLoadTsk(idleTskCb);
}