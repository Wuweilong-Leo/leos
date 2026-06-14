#ifndef OS_TASK_EXTERNAL_H
#define OS_TASK_EXTERNAL_H
#include "os_def.h"
#include "os_list_external.h"
#include "os_mem_external.h"

#define OS_TASK_LOWEST_PRIO  31
#define OS_TASK_PRIO_MAX_NUM (OS_TASK_LOWEST_PRIO + 1)

#define OS_TASK_NAME_MAX_SIZE     0x10
#define OS_TASK_MAX_NUM           32
#define OS_TASK_ARG_NUM           4
#define OS_TASK_KERNEL_STACK_SIZE 0x1000

#define OS_TASK_GET_CB(tskId) (&g_tskCbArray[(tskId)])

typedef void (*OsTaskEntryFunc)(void *arg1, void *arg2, void *arg3, void *arg4);

#define OS_TASK_STATUS_USED       0x1U
#define OS_TASK_STATUS_READY      0x2U
#define OS_TASK_STATUS_RUNNING    0x4U
#define OS_TASK_STATUS_PENDING    0x8U
#define OS_TASK_STATUS_IN_DELAY   0x10U
#define OS_TASK_STATUS_TIMEOUT    0x20U
#define OS_TASK_STATUS_SUSPENDED  0x40U

// 两种任务类型，线程和进程
enum OsTaskType { OS_TASK_THREAD, OS_TASK_PROCESS };

/* 任务控制块 */
struct OsTaskCb {
    uintptr_t stkPtr;
    uintptr_t kernelStkTop;
    struct OsList freeListNode;
    U32 pid;
    OsTaskEntryFunc entry;
    void *arg[OS_TASK_ARG_NUM];
    U32 status;
    U32 prio;
    U64 timeSliceTicks; // 时间片的tick数
    U64 expiredTick;    // 延时到期时的tick刻度
    char name[OS_TASK_NAME_MAX_SIZE];
    struct OsList rdyListNode;
    struct OsList pendListNode;
    struct OsList timerListNode;
    U32 eventMsk;
    U32 curEvent;
    enum OsTaskType tskType;
    uintptr_t pgDir;                /* 进程页目录，线程为NULL */
    struct OsList holdSemList;     /* 该任务持有的所有互斥信号量（通过 semCb->holdNode 挂入） */
    struct OsMemPool usrVirMemPool; /* 进程的用户虚拟内存池 */
};

struct OsTaskCreateParam {
    char name[OS_TASK_NAME_MAX_SIZE];
    U32 prio;
    OsTaskEntryFunc entryFunc;
    void *arg[OS_TASK_ARG_NUM];
};

#define OS_TASK_CREATE_NO_FREE_CB      OS_BUILD_ERR_CODE(OS_MID_TASK, 0x0);
#define OS_TASK_CREATE_STK_ALLOC_FAIL  OS_BUILD_ERR_CODE(OS_MID_TASK, 0x1);
#define OS_TASK_RESUME_TSK_STATUS_ILL  OS_BUILD_ERR_CODE(OS_MID_TASK, 0x2);
#define OS_TASK_SUSPEND_TSK_STATUS_ILL OS_BUILD_ERR_CODE(OS_MID_TASK, 0x3);
/* 0x4, 0x5 reserved */
#define OS_TASK_DELAY_PARAM_ILL        OS_BUILD_ERR_CODE(OS_MID_TASK, 0x6);
#define OS_TASK_DELAY_TSK_STATUS_ILL   OS_BUILD_ERR_CODE(OS_MID_TASK, 0x7);
#define OS_TASK_SET_PRIO_PARAM_ILL     OS_BUILD_ERR_CODE(OS_MID_TASK, 0x8);
#define OS_TASK_DELETE_TSK_STATUS_ILL  OS_BUILD_ERR_CODE(OS_MID_TASK, 0x9);
#define OS_TASK_DELETE_HOLD_SEM       OS_BUILD_ERR_CODE(OS_MID_TASK, 0xA); /* 持有互斥信号量不允许删除 */

extern void OsTaskIdleEntry(void);
extern U32 OsTaskConfigInit(void);
extern U32 OsTaskCreateIdle(void);
extern U32 OsTaskCreate(struct OsTaskCreateParam *param, U32 *tskId);
extern U32 OsTaskResume(U32 tskId);
extern U32 OsTaskSuspend(U32 tskId);
extern U32 OsTaskDelete(U32 tskId);
extern void OsTaskSchedule();
extern U32 OsTaskDelay(U32 ticks);
extern void OsTaskTimerListInsert(struct OsTaskCb *tsk);
extern void OsTaskRecycleHandler(U32 hwiNum);
extern void OsTaskRecycleStk(void);

extern struct OsTaskCb *g_tskCbArray;

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