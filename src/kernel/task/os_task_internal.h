#ifndef OS_TASK_INTERNAL_H
#define OS_TASK_INTERNAL_H
#include "os_task_external.h"
#define OS_TASK_STACK_TOP_MAGIC 0xA5A6A7A8
extern void OsTrapTsk(struct OsTaskCb *tsk);
#endif