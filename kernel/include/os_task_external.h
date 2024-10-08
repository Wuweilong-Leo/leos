#ifndef OS_TASK_EXTERNAL_H
#define OS_TASK_EXTERNAL_H
#include "os_def.h"
#include "os_list_external.h"
#include "os_mem_external.h"

#define OS_TASK_LOWEST_PRIO 31
#define OS_TASK_PRIO_MAX_NUM (OS_TASK_LOWEST_PRIO + 1)

#define OS_TASK_NAME_MAX_SIZE 0x10
#define OS_TASK_MAX_NUM 32
#define OS_TASK_ARG_NUM 4
/* 任务栈大小要4K对齐 */
#define OS_TASK_KERNEL_STACK_SIZE 0x1000

#define OS_TASK_GET_CB(tskId) (&g_tskCbArray[(tskId)])
#define OS_TASK_IS_RDY(tskCb) ((tskCb)->status == OS_TASK_READY)
#define OS_TASK_IS_RUNNING(tskCb) ((tskCb)->status == OS_TASK_RUNNING)

typedef void (*OsTaskEntryFunc)(void *arg1, void *arg2, void *arg3, void *arg4);

enum OsTaskStatus {
  OS_TASK_NOT_CREATE,
  OS_TASK_NOT_RESUME,
  OS_TASK_RUNNING,
  OS_TASK_READY,
  OS_TASK_SEM_PENDING,
  OS_TASK_IN_DELAY,
  OS_TASK_IN_SUSPEND,
  OS_TASK_WAITING_EVENT,
};

// 两种任务类型，线程和进程
enum OsTaskType {
  OS_TASK_THREAD,
  OS_TASK_PROCESS
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
  U64 timeSliceTicks; // 时间片的tick数
  U64 expiredTick; // 延时到期时的tick刻度
  char name[OS_TASK_NAME_MAX_SIZE];
  struct OsList rdyListNode;
  struct OsList pendListNode;
  struct OsList semList; /* 拥有的信号量链表 */
  struct OsList dlyListNode;
  U32 eventMsk;
  U32 curEvent;
  enum OsTaskType tskType;
  uintptr_t pgDir; /* 进程页目录，线程为NULL */
  struct OsMemPool usrVirMemPool; /* 进程的用户虚拟内存池 */
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
#define OS_TASK_SUSPEND_TSK_STATUS_ILL OS_BUILD_ERR_CODE(OS_MID_TASK, 0x3);
#define OS_TASK_SUSPEND_TSK_HOLD_SEM OS_BUILD_ERR_CODE(OS_MID_TASK, 0x4);
#define OS_TASK_YIELD_TSK_HOLD_SEM OS_BUILD_ERR_CODE(OS_MID_TASK, 0x5);
#define OS_TASK_DELAY_PARAM_ILL OS_BUILD_ERR_CODE(OS_MID_TASK, 0x6);
#define OS_TASK_DELAY_TSK_STATUS_ILL OS_BUILD_ERR_CODE(OS_MID_TASK, 0x7);
#define OS_TASK_SET_PRIO_PARAM_ILL OS_BUILD_ERR_CODE(OS_MID_TASK, 0x8);

extern void OsTaskIdleEntry(void);
extern void OsTaskConfig(void);
extern U32 OsTaskCreateIdle(U32 *tskId);
extern U32 OsTaskCreate(struct OsTaskCreateParam *param, U32 *tskId);
extern U32 OsTaskResume(U32 tskId);
extern void OsTaskSchedule();
extern U32 OsTaskSuspend(U32 tskId);
extern U32 OsTaskDelay(U32 ticks);

extern struct OsTaskCb g_tskCbArray[OS_TASK_MAX_NUM];

OS_INLINE void OsTaskAdjustPrio(struct OsTaskCb *tsk)
{
    tsk->prio = (tsk->prio + 1) % OS_TASK_PRIO_MAX_NUM;
}

// 时间片跟优先级挂钩，优先级越高时间片越短
OS_INLINE U32 OsTaskCalTimeSlice(struct OsTaskCb *tsk)
{
    return tsk->prio + 1;
}

OS_INLINE void OsTaskSetTimeSlice(struct OsTaskCb *tsk, U32 timeSlice)
{
    tsk->timeSliceTicks = timeSlice;
}
#endif