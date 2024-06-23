#include "os_sched_internal.h"
#include "os_list_external.h"
#include "string.h"
#include "os_debug_external.h"

OS_SEC_KERNEL_BSS struct OsRunQue g_runQue;

OS_SEC_KERNEL_DATA struct OsScheduler g_rtScheduler = {
    .dequeTsk = NULL,
    .enqueTsk = NULL,
    .pickNextTsk = OsSchedPickNextTskRt
};

OS_SEC_KERNEL_TEXT struct OsTaskCb *OsSchedPickNextTskRt(void)
{
    struct OsList *rdyList;
    U32 highestPrio = OS_RUN_QUE()->curPrio;
    struct OsList *rdyListNode;

    rdyList = &OS_RUN_QUE()->rdyList[highestPrio];
    rdyListNode = OS_LIST_GET_FIRST_NODE(rdyList);

    return OS_LIST_GET_STRUCT_ENTRY(struct OsTaskCb, rdyListNode, rdyListNode);
}

OS_SEC_KERNEL_TEXT void OsSchedConfig(void)
{
    struct OsRunQue *rq = OS_RUN_QUE();
    U32 i;

    rq->curPrio = OS_TASK_PRIO_MAX_NUM;
    rq->runningTsk = NULL;
    rq->rdyListMsk = 0;
    for (i = 0; i < OS_TASK_PRIO_MAX_NUM; i++) {
        OsListInit(&rq->rdyList[i]);
    }
    OsListInit(&rq->delayList);
    rq->intCount = 0;
    rq->scheduler = &g_rtScheduler;
    rq->needSched = FALSE;
}

/* 外部关中断 */
OS_SEC_KERNEL_TEXT void OsSchedAddTskToRdyListTail(struct OsTaskCb *tsk)
{
    struct OsRunQue *rq = OS_RUN_QUE();
    U32 tskPrio = tsk->prio;
    struct OsList *rdyList = &rq->rdyList[tskPrio];

    OsListAddTail(rdyList, &tsk->rdyListNode);

    OS_RUN_QUE()->rdyListMsk |= (1 << tskPrio);

    if (tskPrio < rq->curPrio) {
        rq->curPrio = tskPrio;
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

OS_SEC_KERNEL_TEXT void OsSchedMain(void)
{
    struct OsRunQue *rq = OS_RUN_QUE();
    struct OsScheduler *scheduler = rq->scheduler;
    struct OsTaskCb *curTsk = OS_RUNNING_TASK();
    struct OsTaskCb *nextTsk = curTsk;

    if (rq->needSched) {
        rq->needSched = FALSE;
        nextTsk = scheduler->pickNextTsk();

        if (nextTsk != curTsk) {
            nextTsk->status = OS_TASK_RUNNING;
            rq->runningTsk = nextTsk; 
        }
    }

    OsLoadTsk(nextTsk);
}

OS_SEC_KERNEL_TEXT void OsSchedSwitchIdle(void)
{
    U32 idleTskId;
    struct OsTaskCb *idleTskCb;
    struct OsRunQue *rq = OS_RUN_QUE();

    if (OsTaskCreateIdle(&idleTskId) != OS_OK) {
        OS_DEBUG_PRINT_STR("OsTaskCreateIdle failed\n");
    }

    idleTskCb = OS_TASK_GET_CB(idleTskId);
    rq->idleTsk = idleTskCb;
    rq->runningTsk = idleTskCb;
    rq->idleTsk->status = OS_TASK_RUNNING;

    OsLoadTsk(idleTskCb);
}