#ifndef OS_TASK_INTERNAL_H
#define OS_TASK_INTERNAL_H
#include "os_task_external.h"
#define OS_TASK_STACK_TOP_MAGIC 0xA5A6A7A8
/* 任务栈大小要4K对齐 */
#define OS_TASK_STACK_SIZE 0x2000
extern void OsTrapTsk(struct OsTaskCb *tsk);
#endif