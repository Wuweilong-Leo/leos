#ifndef OS_SCHED_EXTERNAL_H
#define OS_SCHED_EXTERNAL_H
#include "os_task_external.h"
#include "os_def.h"
#include "os_list_external.h"

typedef struct OsTaskCb * (*OsPickNextTsk) (void);

struct OsScheduler {
    OsPickNextTsk pickNextTsk;
};

struct OsRunQue {
    struct OsTaskCb *runningTsk;
    struct OsTaskCb *idleTsk;
    U32 uniFlag;
    U32 intCount;
    bool needSched;
    U32 rdyListMsk;
    struct OsList rdyList[OS_TASK_PRIO_MAX_NUM];
    struct OsList dlyList;
    struct OsScheduler *scheduler;
    U64 nearestTick;
};

extern struct OsRunQue g_runQue;

#define OS_RUN_QUE() (&g_runQue)
#define OS_RUNNING_TASK() (OS_RUN_QUE()->runningTsk)
#define OS_UNI_FLAG_SET_MSK(msk) (OS_RUN_QUE()->uniFlag |= (msk))
#define OS_UNI_FLAG_CLR_MSK(msk) (OS_RUN_QUE()->uniFlag &= ~(msk))

extern void OsSchedSwitchIdle(void);
extern void OsSchedConfig(void);
extern struct OsTaskCb *OsSchedPickHighestPrioTsk(void);
extern void OsSchedMain(void);
extern void OsEnqueTskToRdyListTail(struct OsTaskCb *tsk);
extern void OsDequeTskFromRdyList(struct OsTaskCb* tsk);
extern void OsSchedModifyTskPrio(struct OsTaskCb *tsk);
#endif