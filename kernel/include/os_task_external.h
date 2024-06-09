#ifndef OS_TASK_EXTERNAL_H
#define OS_TASK_EXTERNAL_H
#include "os_def.h"
#include "os_list_external.h"

#define OS_TASK_NAME_MAX_SIZE 0x10
#define OS_TASK_MAX_NUM 32
#define OS_TASK_ARG_NUM 4

typedef void (*OsTaskEntry)(void **);

enum OsTaskStatus {
  OS_TASK_NOT_CREATE,
  OS_TASK_NOT_RESUME,
  OS_TASK_RUNNING,
  OS_TASK_READY,
  OS_TASK_SEM_PENDING,
  OS_TASK_TIME_SLICE_PENDING,
  OS_TASK_IN_DELAY
};

/* 任务控制块 */
struct OsTaskCb {
  uintptr_t stkPtr;
  uintptr_t kernelStkBot;
  struct OsList freeListNode;
  U32 pid;
  OsTaskEntry entry;
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
  OsTaskEntry entryFunc;
  void *arg[OS_TASK_ARG_NUM];
};
#endif