#ifndef OS_TICK_EXTERNAL_H
#define OS_TICK_EXTERNAL_H
#include "os_def.h"
#include "os_sched_external.h"
extern U64 g_uniTicks;
extern void OsTickIsr(void);
extern void OsTickDispatcher(void);
extern void OsRefreshNearestTick(struct OsRunQue *rq);
#endif