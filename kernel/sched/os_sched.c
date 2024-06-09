#include "os_sched_internal.h"
#include "os_list_external.h"

OS_SEC_KERNEL_BSS struct OsRunQue g_runQue;

/* 外部关中断 */
OS_SEC_KERNEL_TEXT void OsSchedAddTskToRdyListTail(struct OsTaskCb *tsk)
{
    struct OsRunQue *rq = OS_RUN_QUE();
    U32 tskPrio = tsk->prio;
    struct osList *rdyList = &rq->rdyList[tskPrio];

    OsListAddTail(rdyList, &tsk->rdyListNode);
    if (tskPrio < rq->curPrio) {
        rq->curPrio = tskPrio;
        rq->needSched = TRUE;
    }
}

OS_SEC_KERNEL_TEXT void OsSchedDelTskFromRdyList(struct OsTaskCb* tsk)
{
    struct OsRunQue *rq = OS_RUN_QUE();
    struct OsList *rdyList = &rq->rdyList[tsk->prio];

    OsListRemoveNode(&tsk->rdyListNode);
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
        nextTsk = scheduler->pickNextTsk();

        if (nextTsk != curTsk) {
            nextTsk->status = OS_TASK_RUNNING;
            rq->runningTsk = nextTsk; 
        }
    }

    OsLoadTsk(nextTsk);
}