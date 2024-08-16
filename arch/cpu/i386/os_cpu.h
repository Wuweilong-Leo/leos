#ifndef OS_CPU_H
#define OS_CPU_H
#include "os_cpu_i386.h"
#include "os_def.h"
#include "os_task_external.h"

extern void OsProcessInitArch(struct OsTaskCb *process);
extern void OsConfigArchForTskSwitch(struct OsTaskCb *tsk);
#endif