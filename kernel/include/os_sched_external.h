#ifndef OS_SCHED_EXTERNAL_H
#define OS_SCHED_EXTERNAL_H
#include "os_task_external.h"
#include "os_def.h"
#include "os_list_external.h"

#define OS_TASK_LOWEST_PRIO 31
#define OS_TASK_PRIO_MAX_NUM (OS_TASK_LOWEST_PRIO + 1)

struct os_run_que {
    struct os_task_cb *running_task;
    uint32_t uni_flag;
    bool need_sched;
    struct os_task_cb *idle_task;
    uint32_t rdy_list_msk;
    struct os_list rdy_list[OS_TASK_PRIO_MAX_NUM];
    struct os_list delay_list;
};

extern struct os_run_que g_run_que;

#define OS_RUN_QUE() (&g_run_que)
#endif