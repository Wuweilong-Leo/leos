#ifndef OS_SEM_EXTERNAL_H
#define OS_SEM_EXTERNAL_H
#include "os_def.h"
#include "os_list_external.h"
#include "os_sys.h"

/* 信号量类型 */
enum OsSemType {
    OS_SEM_BINARY_SYNC  = 0, /* 二值同步信号量：初始必须为0，事件通知/同步 */
    OS_SEM_BINARY_MUTEX = 1, /* 二值互斥信号量：0/1，互斥保护 */
    OS_SEM_COUNTING     = 2, /* 计数信号量：0~maxCnt，资源计数 */
};

/* 唤醒策略 */
enum OsSemWakePolicy {
    OS_SEM_WAKE_FIFO = 0, /* 先等先唤醒 */
    OS_SEM_WAKE_PRIO = 1, /* 高优先级先唤醒 */
};

struct OsSemCb {
    U32 semId;
    U32 val;                      /* 当前值 */
    U32 maxCnt;                   /* binary 固定为1, counting 用户指定 */
    enum OsSemType type;
    enum OsSemWakePolicy wakePolicy;
    struct OsList freeListNode;   /* 空闲链表节点 */
    struct OsList pendList;       /* 等待队列 */
    struct OsList holdNode;       /* 挂入持有者 TCB 的 holdSemList（仅 BINARY_MUTEX 使用） */
    struct OsTaskCb *holder;      /* BINARY_MUTEX 持有者，其他类型为 NULL */
#ifdef OS_SEM_BIN_SUPPORT_RECUR
    U32 nestCnt;                  /* BINARY_MUTEX 递归嵌套计数 */
#endif
};

#define OS_SEM_WAIT_FOREVER  0xFFFFFFFFU
#define OS_SEM_NO_WAIT       0

#define OS_SEM_CREATE_NO_FREE_CB         OS_BUILD_ERR_CODE(OS_MID_SEM, 0x0)
#define OS_SEM_POST_IS_FULL              OS_BUILD_ERR_CODE(OS_MID_SEM, 0x1) /* 计数型满 */
#define OS_SEM_PEND_TIMEOUT              OS_BUILD_ERR_CODE(OS_MID_SEM, 0x3)
#define OS_SEM_PEND_UNAVAILABLE          OS_BUILD_ERR_CODE(OS_MID_SEM, 0x4)
#define OS_SEM_POST_NOT_HOLDER          OS_BUILD_ERR_CODE(OS_MID_SEM, 0x6) /* BINARY_MUTEX 非持有者 Post */
#define OS_SEM_PARAM_INVALID             OS_BUILD_ERR_CODE(OS_MID_SEM, 0x5)

extern U32 OsSemCreate(enum OsSemType type, U32 initVal, U32 maxCnt,
                       enum OsSemWakePolicy policy, U32 *semId);
extern U32 OsSemPend(U32 semId, U32 timeout);
extern U32 OsSemPost(U32 semId);
extern U32 OsSemConfigInit(void);
#endif