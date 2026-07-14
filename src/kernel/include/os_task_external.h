#ifndef OS_TASK_EXTERNAL_H
#define OS_TASK_EXTERNAL_H
#include "os_def.h"
#include "os_list_external.h"
#include "os_mem_external.h"

#define OS_TASK_LOWEST_PRIO  31
#define OS_TASK_PRIO_MAX_NUM (OS_TASK_LOWEST_PRIO + 1)
/* 默认时间片(滴答数),固定值,同优先级内时间片轮转用 */
#define OS_TASK_TIME_SLICE_DEFAULT 10

#define OS_TASK_NAME_MAX_SIZE     0x10
#define OS_TASK_MAX_NUM           32

extern U32 g_tskMaxNum;
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
#define OS_TASK_STATUS_PEND_MSG   0x80U  /* 在等消息接收 */
#define OS_TASK_STATUS_ZOMBIE     0x100U /* 僵尸态（已exit，等waitpid收割） */

#define OS_WAIT_ANY_CHILD  ((U32)-1)     /* waitpid 等待任意子进程 */

// 两种任务类型：内核线程和用户进程
// 共享地址空间线程也是 OS_TASK_PROCESS，通过 pgShareMaster 区分
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
    U32 oriPrio;              /* 优先级继承用：任务创建时的原始优先级 */
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
    struct OsList msgList;         /* 该任务的消息信箱（OsMsgHeader.queueNode 挂入） */
    struct OsMemPool usrVirMemPool;    /* 进程的用户虚拟内存池 */
    struct OsMemFscCtrl *usrFscCtrl;   /* 进程的用户堆 FSC 控制块，线程为 NULL */
    struct OsTaskCb *pgShareMaster;    /* 共享地址空间组的主 TCB（独立进程指向自己，共享线程指向创建者） */
    U16 pgDirRefCnt;                   /* 页目录引用计数（仅 master 中有效，clone 时 +1，退出时 -1） */
    U32 parentPid;                     /* 父进程 pid（0=无父进程/内核任务） */
    U32 exitCode;                      /* 退出码（ZOMBIE 时有效） */
    U32 waitPid;                       /* waitpid 等待的目标 pid（0=不在等待，OS_WAIT_ANY_CHILD=等任意子进程） */
    bool detached;                     /* TRUE=分离线程，退出时自动回收不需 join；FALSE=joinable（默认）退出后 ZOMBIE 等 waitpid */
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
#define OS_TASK_TSK_ID_INVALID         OS_BUILD_ERR_CODE(OS_MID_TASK, 0x4);
/* 0x5 reserved */
#define OS_TASK_DELAY_PARAM_ILL        OS_BUILD_ERR_CODE(OS_MID_TASK, 0x6);
#define OS_TASK_DELAY_TSK_STATUS_ILL   OS_BUILD_ERR_CODE(OS_MID_TASK, 0x7);
#define OS_TASK_SET_PRIO_PARAM_ILL     OS_BUILD_ERR_CODE(OS_MID_TASK, 0x8);
#define OS_TASK_DELETE_TSK_STATUS_ILL  OS_BUILD_ERR_CODE(OS_MID_TASK, 0x9);
#define OS_TASK_DELETE_HOLD_SEM       OS_BUILD_ERR_CODE(OS_MID_TASK, 0xA); /* 持有互斥信号量不允许删除 */
#define OS_TASK_CREATE_PRIO_ILL      OS_BUILD_ERR_CODE(OS_MID_TASK, 0xB); /* 优先级非法(>= LOWEST_PRIO,占用 idle 层) */

/* pthread_detach 错误码（0x20-0x25） */
#define OS_TASK_DETACH_INVALID_PID   OS_BUILD_ERR_CODE(OS_MID_TASK, 0x20)  /* pid 越界或 status 无效 */
#define OS_TASK_DETACH_NOT_PROCESS   OS_BUILD_ERR_CODE(OS_MID_TASK, 0x21)  /* 目标不是用户进程/线程 */
#define OS_TASK_DETACH_NOT_SAME_VM   OS_BUILD_ERR_CODE(OS_MID_TASK, 0x22)  /* 调用者与目标不在同一地址空间组 */
#define OS_TASK_DETACH_NOT_CLONE     OS_BUILD_ERR_CODE(OS_MID_TASK, 0x23)  /* 目标是独立进程(fork/OsProcessCreate)，不可 detach */
#define OS_TASK_DETACH_ALREADY       OS_BUILD_ERR_CODE(OS_MID_TASK, 0x24)  /* 目标已 detached */
#define OS_TASK_DETACH_HAS_WAITER    OS_BUILD_ERR_CODE(OS_MID_TASK, 0x25)  /* 有线程正在 waitpid 等待目标 */

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
extern void OsTaskRecycleStk(void);
extern void OsTaskReleaseFreeCb(struct OsTaskCb *tskCb);
extern void OsTaskReapZombie(struct OsTaskCb *tskCb);
extern struct OsTaskCb *OsTaskGetFreeCb(void);

extern struct OsTaskCb *g_tskCbArray;

/* 固定时间片,同优先级内时间片轮转用 */
OS_INLINE U32 OsTaskCalTimeSlice(struct OsTaskCb *tsk)
{
    (void)tsk;
    return OS_TASK_TIME_SLICE_DEFAULT;
}

OS_INLINE void OsTaskSetTimeSlice(struct OsTaskCb *tsk, U32 timeSlice)
{
    tsk->timeSliceTicks = timeSlice;
}
#endif