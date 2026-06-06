#ifndef OS_SEM_EXTERNAL_H
#define OS_SEM_EXTERNAL_H
#include "os_def.h"
#include "os_list_external.h"
#include "os_sys.h"

/* 唤醒策略 */
enum OsSemWakePolicy {
    OS_SEM_WAKE_FIFO = 0, /* 先等先唤醒 */
    OS_SEM_WAKE_PRIO = 1, /* 高优先级先唤醒 */
};

struct OsSemCb {
    U32 semId;
    U32 val;
    U32 semCnt;
    struct OsList freeListNode;
    /* 阻塞在此信号量的任务 */
    struct OsList pendList;
    /* 被任务持有的链表节点, taskCb->semList */
    struct OsList semListNode;
    enum OsSemWakePolicy wakePolicy;
};

#define OS_SEM_WAIT_FOREVER  0xFFFFFFFFU
#define OS_SEM_NO_WAIT       0

#define OS_SEM_PEND_TIMEOUT              OS_BUILD_ERR_CODE(OS_MID_SEM, 0x4)
#define OS_SEM_PEND_UNAVAILABLE          OS_BUILD_ERR_CODE(OS_MID_SEM, 0x5)

extern U32 OsSemCreate(U32 val, U32 maxCnt, enum OsSemWakePolicy policy, U32 *semId);
extern U32 OsSemPend(U32 semId, U32 timeout);
extern U32 OsSemPost(U32 semId);
extern U32 OsSemConfigInit(void);
#endif