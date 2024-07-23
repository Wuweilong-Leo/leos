#ifndef OS_TASK_EXTERNAL_H
#define OS_TASK_EXTERNAL_H
#include "os_def.h"
#include "os_list_external.h"

#define OS_TASK_NAME_MAX_SIZE 0x10
#define OS_TASK_MAX_NUM 32
#define OS_TASK_ARG_NUM 4

typedef void (*OsTaskEntryFunc)(void *arg1, void *arg2, void *arg3, void *arg4);

enum OsTaskStatus {
  OS_TASK_NOT_CREATE,
  OS_TASK_NOT_RESUME,
  OS_TASK_RUNNING,
  OS_TASK_READY,
  OS_TASK_SEM_PENDING,
  OS_TASK_IN_DELAY
};

/* 任务控制块 */
struct OsTaskCb {
  uintptr_t stkPtr;
  uintptr_t kernelStkTop;
  struct OsList freeListNode;
  U32 pid;
  OsTaskEntryFunc entry;
  void *arg[OS_TASK_ARG_NUM];
  enum OsTaskStatus status;
  U32 prio;
  uintptr_t pgDir;
  U32 ticks;
  U32 delayTicks;
  char name[OS_TASK_NAME_MAX_SIZE];
  struct OsList rdyListNode;
  struct OsList pendListNode;
  /* 拥有的信号量链表 */
  struct OsList semList;
  struct OsList delayListNode;
};

struct OsTaskCreateParam {
  char name[OS_TASK_NAME_MAX_SIZE];
  U32 prio;
  OsTaskEntryFunc entryFunc;
  void *arg[OS_TASK_ARG_NUM];
};
#define OS_TASK_CREATE_NO_FREE_CB OS_BUILD_ERR_CODE(OS_MID_TASK, 0x0);
#define OS_TASK_CREATE_STK_ALLOC_FAIL OS_BUILD_ERR_CODE(OS_MID_TASK, 0x1);
#define OS_TASK_RESUME_TSK_STATUS_ILL OS_BUILD_ERR_CODE(OS_MID_TASK, 0x2);

extern void OsTaskIdleEntry(void);
extern void OsTaskConfig(void);
extern U32 OsTaskCreateIdle(U32 *tskId);
extern U32 OsTaskCreate(struct OsTaskCreateParam *param, U32 *tskId);
extern U32 OsTaskResume(U32 tskId);
extern void OsTaskSchedule();

extern struct OsTaskCb g_tskCbArray[OS_TASK_MAX_NUM];

#define OS_TASK_GET_CB(tskId) (&g_tskCbArray[(tskId)])

OS_INLINE U32 OsTaskGetInitialTick(U32 prio)
{
    return prio + 1;
}
#endif