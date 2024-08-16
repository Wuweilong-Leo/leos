#include "os_def.h"
#include "os_hwi_i386.h"
#include "os_task_external.h"
#include "os_sched_external.h"

OS_SEC_KERNEL_TEXT U32 OsEventRead(U32 event)
{
    struct OsTaskCb *tsk;
    enum OsIntStatus intSave;

    /* 不期待任何事件 */
    if (event == 0) {
        return OS_OK;
    }

    intSave = OsIntLock();

    tsk = OS_RUNNING_TASK();

    tsk->eventMsk = event;

    /* 预期的事件没有发生 */
    if (tsk->eventMsk & tsk->curEvent != tsk->eventMsk) {
        OsSchedDelTskFromRdyList(tsk);
        tsk->status = OS_TASK_WAITING_EVENT;
        OsTaskSchedule();
    }

    /* 读到事件了，清掉 */
    tsk->eventMsk = 0;
    tsk->curEvent = 0;

    OsIntRestore(intSave);

    return OS_OK;
}

OS_SEC_KERNEL_TEXT U32 OsEventWrite(U32 tskId, U32 event)
{
    struct OsTaskCb *tsk;
    enum OsIntStatus intSave;

    if (event == 0) {
        return OS_OK;
    }

    intSave = OsIntLock();

    tsk = OS_TASK_GET_CB(tskId);

    /* 写入事件 */
    tsk->curEvent |= event;

    if (tsk->eventMsk & tsk->curEvent == tsk->eventMsk) {
        OsSchedAddTskToRdyListTail(tsk);
        tsk->status = OS_TASK_READY;
        OsTaskSchedule();
    }

    OsIntRestore(intSave);

    return OS_OK;
}