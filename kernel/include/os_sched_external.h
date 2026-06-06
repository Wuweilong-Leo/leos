#ifndef OS_SCHED_EXTERNAL_H
#define OS_SCHED_EXTERNAL_H
#include "os_task_external.h"
#include "os_def.h"
#include "os_list_external.h"

typedef struct OsTaskCb *(*OsPickNextTsk)(void);

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
    struct OsScheduler *scheduler;
};

extern struct OsRunQue g_runQue;
extern struct OsList g_timerList;
extern U64 g_nearestTick;
extern uintptr_t g_kernelStackHigh;

#define OS_RUN_QUE()      (&g_runQue)
#define OS_RUNNING_TASK() (OS_RUN_QUE()->runningTsk)

extern U32 OsSchedConfigInit(void);
extern struct OsTaskCb *OsSchedPickHighestPrioTsk(void);
extern void OsSchedMain(void);
extern void OsSchedRdyListEnqueTsk(struct OsTaskCb *tsk);
extern void OsSchedRdyListDequeTsk(struct OsTaskCb *tsk);
extern void OsSchedModifyTskPrio(struct OsTaskCb *tsk);
extern void OsSchedSwitchFirstTsk(void);
#endif