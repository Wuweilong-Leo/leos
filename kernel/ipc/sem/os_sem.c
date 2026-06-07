#include "os_sem_internal.h"
#include "os_debug_external.h"
#include "os_task_external.h"
#include "os_hwi.h"
#include "os_sched_external.h"
#include "os_base_external.h"
#include "os_tick_external.h"
#include "os_reset.h"
#include "string.h"

OS_SEC_KERNEL_DATA struct OsList g_semFreeList = OS_LIST_INIT(g_semFreeList);
OS_SEC_KERNEL_BSS struct OsSemCb *g_semCbArray;
OS_SEC_KERNEL_BSS U32 g_semMaxNum;

OS_SEC_KERNEL_TEXT U32 OsSemConfigInit(void)
{
    U32 i;
    struct OsSemCb *semCb;
    struct OsList *freeListNode;
    size_t size;

    g_semMaxNum = OS_SEM_MAX_NUM;
    size = g_semMaxNum * sizeof(struct OsSemCb);
    g_semCbArray = (struct OsSemCb *)OsMemKernelAlloc(size, 4);
    if (g_semCbArray == NULL) {
        OS_PANIC("OsSemConfigInit: alloc semCbArray failed, size=%u\n", (U32)size);
    }

    memset(g_semCbArray, 0, size);

    for (i = 0; i < g_semMaxNum; i++) {
        semCb = &g_semCbArray[i];
        freeListNode = &semCb->freeListNode;

        semCb->semId = i;
        OsListInit(&semCb->pendList);
        OsListAddTail(&g_semFreeList, freeListNode);
    }

    return OS_OK;
}

OS_INLINE struct OsSemCb *OsSemGetFreeCb(void)
{
    if (OsListIsEmpty(&g_semFreeList)) {
        return NULL;
    }

    return OS_GET_STRUCT_ENTRY(struct OsSemCb, freeListNode, OsListPopHead(&g_semFreeList));
}

OS_SEC_KERNEL_TEXT U32 OsSemCreate(enum OsSemType type, U32 initVal, U32 maxCnt,
                                   enum OsSemWakePolicy policy, U32 *semId)
{
    struct OsSemCb *semCb;
    enum OsIntStatus intSave;

    /* 参数校验 */
    if (type == OS_SEM_BINARY_SYNC) {
        if (initVal != 0) {
            return OS_SEM_PARAM_INVALID; /* 同步信号量初始必须为0 */
        }
        maxCnt = 1;
    } else if (type == OS_SEM_BINARY_MUTEX) {
        if (initVal != 1) {
            return OS_SEM_PARAM_INVALID; /* 互斥信号量初始必须可用 */
        }
        maxCnt = 1;
    } else if (type == OS_SEM_COUNTING) {
        if (maxCnt == 0 || initVal > maxCnt) {
            return OS_SEM_PARAM_INVALID;
        }
    } else {
        return OS_SEM_PARAM_INVALID;
    }

    intSave = OsIntLock();

    semCb = OsSemGetFreeCb();
    if (semCb == NULL) {
        OS_LOG_ERROR("OsSemCreate: no free sem CB\n");
        OsIntRestore(intSave);
        return OS_SEM_CREATE_NO_FREE_CB;
    }

    semCb->val        = initVal;
    semCb->maxCnt     = maxCnt;
    semCb->type       = type;
    semCb->wakePolicy = policy;
    semCb->holder     = NULL;
#ifdef OS_SEM_BIN_SUPPORT_RECUR
    semCb->nestCnt    = 0;
#endif
    *semId = semCb->semId;

    OsIntRestore(intSave);
    return OS_OK;
}

/* 按优先级插入 pend 队列：优先级数值越小（越高）排越前 */
static OS_SEC_KERNEL_TEXT void OsSemPendListInsertByPrio(struct OsList *pendList,
                                                         struct OsTaskCb *tsk)
{
    struct OsList *node;
    struct OsTaskCb *pos;

    OS_LIST_FOR_EACH(pendList, node)
    {
        pos = OS_GET_STRUCT_ENTRY(struct OsTaskCb, pendListNode, node);
        if (tsk->prio < pos->prio) {
            OsListInsertPrev(&tsk->pendListNode, node);
            return;
        }
    }
    /* 优先级最低，插尾部 */
    OsListAddTail(pendList, &tsk->pendListNode);
}

OS_SEC_KERNEL_TEXT U32 OsSemPend(U32 semId, U32 timeout)
{
    struct OsSemCb *semCb;
    enum OsIntStatus intSave;
    struct OsTaskCb *curTsk;

    intSave = OsIntLock();

    semCb = OS_SEM_GET_CB(semId);
    curTsk = OS_RUNNING_TASK();

    /* BINARY_MUTEX 递归持有检查 */
#ifdef OS_SEM_BIN_SUPPORT_RECUR
    if (semCb->type == OS_SEM_BINARY_MUTEX && semCb->holder == curTsk) {
        semCb->nestCnt++;
        OsIntRestore(intSave);
        return OS_OK;
    }
#endif

    if (semCb->val == 0) {
        /* 无可用资源 */

        if (timeout == OS_SEM_NO_WAIT) {
            /* 不等待，直接返回 */
            OsIntRestore(intSave);
            return OS_SEM_PEND_UNAVAILABLE;
        }

        /* 加入到信号量 pending 队列 */
        if (semCb->wakePolicy == OS_SEM_WAKE_PRIO) {
            OsSemPendListInsertByPrio(&semCb->pendList, curTsk);
        } else {
            OsListAddTail(&semCb->pendList, &curTsk->pendListNode);
        }

        /* 从就绪队列里删除 */
        OsSchedRdyListDequeTsk(curTsk);
        curTsk->status |= OS_TASK_STATUS_PENDING;

        /* 如果有超时，同时挂到延时链 */
        if (timeout != OS_SEM_WAIT_FOREVER) {
            curTsk->status |= OS_TASK_STATUS_IN_DELAY;
            curTsk->expiredTick = g_uniTicks + timeout;
            OsTaskTimerListInsert(curTsk);
        }

        /* 切出去，等 OsSemPost 或超时唤醒 */
        OsTaskSchedule();

        /* --- 被唤醒后到这里，中断仍处于关闭状态 --- */
        curTsk->status &= ~OS_TASK_STATUS_PENDING;

        if (curTsk->status & OS_TASK_STATUS_TIMEOUT) {
            /* 超时唤醒，未获取信号量 */
            curTsk->status &= ~OS_TASK_STATUS_TIMEOUT;
            OsIntRestore(intSave);
            return OS_SEM_PEND_TIMEOUT;
        }

        /* 正常被 OsSemPost 唤醒，继续往下执行 val-- */
    }

    /* 获取资源 */
    semCb->val--;

    /* BINARY_MUTEX: 记录持有者 */
    if (semCb->type == OS_SEM_BINARY_MUTEX) {
        semCb->holder = curTsk;
    }

    OsIntRestore(intSave);

    return OS_OK;
}

OS_SEC_KERNEL_TEXT U32 OsSemPost(U32 semId)
{
    struct OsSemCb *semCb;
    enum OsIntStatus intSave;
    struct OsTaskCb *curTsk;
    struct OsTaskCb *pendTsk;

    intSave = OsIntLock();

    semCb = OS_SEM_GET_CB(semId);
    curTsk = OS_RUNNING_TASK();

    /* BINARY_MUTEX: 只有持有者能 Post */
    if (semCb->type == OS_SEM_BINARY_MUTEX) {
        if (semCb->holder != curTsk) {
            OsIntRestore(intSave);
            return OS_SEM_POST_NOT_HOLDER;
        }
#ifdef OS_SEM_BIN_SUPPORT_RECUR
        if (semCb->nestCnt > 0) {
            semCb->nestCnt--;
            OsIntRestore(intSave);
            return OS_OK;
        }
#endif
        semCb->holder = NULL;
    }

    /* 有任务在等，直接移交，唤醒队首 */
    if (!OsListIsEmpty(&semCb->pendList)) {
        pendTsk =
            OS_GET_STRUCT_ENTRY(struct OsTaskCb, pendListNode, OsListPopHead(&semCb->pendList));

        pendTsk->status &= ~OS_TASK_STATUS_PENDING;

        /* 如果任务带超时在等，从延时链移除 */
        if (pendTsk->status & OS_TASK_STATUS_IN_DELAY) {
            OsListRemoveNode(&pendTsk->timerListNode);
            pendTsk->status &= ~OS_TASK_STATUS_IN_DELAY;
            OsRefreshNearestTick();
        }

        /* SUSPENDED 任务不加就绪队列 */
        if (!(pendTsk->status & OS_TASK_STATUS_SUSPENDED)) {
            OsSchedRdyListEnqueTsk(pendTsk);
            OsTaskSchedule();
        }

        OsIntRestore(intSave);
        return OS_OK;
    }

    /* 没人等，val 递增 */
    if (semCb->type == OS_SEM_BINARY_SYNC || semCb->type == OS_SEM_BINARY_MUTEX) {
        if (semCb->val == 1) {
            OsIntRestore(intSave);
            return OS_SEM_POST_AGAIN;
        }
        semCb->val = 1;
    } else {
        /* 计数信号量 */
        if (semCb->val >= semCb->maxCnt) {
            OsIntRestore(intSave);
            return OS_SEM_POST_IS_FULL;
        }
        semCb->val++;
    }

    OsIntRestore(intSave);

    return OS_OK;
}