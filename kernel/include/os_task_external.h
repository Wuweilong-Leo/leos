#ifndef OS_TASK_EXTERNAL_H
#define OS_TASK_EXTERNAL_H
#include "os_def.h"
#include "os_list_external.h"

#define OS_TASK_NAME_MAX_SIZE 0x10
#define OS_TASK_MAX_NUM 32
#define OS_TASK_ARG_NUM 4

typedef void (*task_entry)(void **);

enum os_task_status {
  OS_TASK_NOT_CREATE,
  OS_TASK_NOT_RESUME,
  OS_TASK_RUNNING,
  OS_TASK_READY,
  OS_TASK_SEM_PENDING,
  OS_TASK_TIME_SLICE_PENDING,
  OS_TASK_IN_DELAY
};

/* 任务控制块 */
struct os_task_cb {
  uintptr_t stk_ptr;
  uintptr_t kernel_stk_bot;
  struct os_list free_list_node;
  uint32_t pid;
  task_entry entry;
  uint32_t arg[OS_TASK_ARG_NUM];
  enum os_task_status status;
  uint32_t prio;
  uintptr_t pg_dir;
  uint32_t ticks;
  uint32_t delay_ticks;
  char name[OS_TASK_NAME_MAX_SIZE];
  struct os_list rdy_list_node;
  struct os_list pend_list_node;
  /* 拥有的信号量链表 */
  struct os_list sem_list;
  struct os_list delay_list_node;
};

struct OsTaskCreateParam {
  char name[OS_TASK_NAME_MAX_SIZE];
  uint32_t prio;
  task_entry entry_func;
  void *arg[OS_TASK_ARG_NUM];
};
#endif