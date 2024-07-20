#ifndef OS_SCHED_EXTERNAL_H
#define OS_SCHED_EXTERNAL_H
#include "os_task_external.h"
#include "os_def.h"
#include "os_list_external.h"

#define OS_TASK_LOWEST_PRIO 31
#define OS_TASK_PRIO_MAX_NUM (OS_TASK_LOWEST_PRIO + 1)

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
    struct OsList delayList;
    struct OsScheduler *scheduler;
};

extern struct OsRunQue g_runQue;

#define OS_RUN_QUE() (&g_runQue)
#define OS_RUNNING_TASK() (OS_RUN_QUE()->runningTsk)

extern void OsSchedSwitchIdle(void);
extern void OsSchedConfig(void);
extern struct OsTaskCb *OsSchedPickHighestPrioTsk(void);
extern void OsSchedMain(void);
extern void OsSchedAddTskToRdyListTail(struct OsTaskCb *tsk);
extern void OsSchedDelTskFromRdyList(struct OsTaskCb* tsk);
extern void OsSchedModifyTskPrio(struct OsTaskCb *tsk);
#endif