#include "os_sem_internal.h"
#include "os_debug_external.h"
#include "os_task_external.h"
#include "os_hwi.h"
#include "os_sched_external.h"
#include "os_base_external.h"
#include "os_tick_external.h"
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
        OS_LOG_ERROR("OsSemConfigInit: alloc semCbArray failed, size=%u\n", (U32)size);
        while (1) {
        }
    }

    memset(g_semCbArray, 0, size);

    for (i = 0; i < g_semMaxNum; i++) {
        semCb = &g_semCbArray[i];
        freeListNode = &semCb->freeListNode;

        semCb->semId = i;
        OsListInit(&semCb->pendList);
        OsListInit(&semCb->semListNode);
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

OS_SEC_KERNEL_TEXT U32 OsSemCreate(U32 semCnt, U32 maxCnt, enum OsSemWakePolicy policy, U32 *semId)
{
    struct OsSemCb *semCb;
    enum OsIntStatus intSave = OsIntLock();

    semCb = OsSemGetFreeCb();
    if (semCb == NULL) {
        OS_LOG_ERROR("OsSemCreate: no free sem CB\n");
        OsIntRestore(intSave);
        return OS_SEM_CREATE_NO_FREE_CB;
    }

    semCb->val = semCnt;
    semCb->semCnt = maxCnt;
    semCb->wakePolicy = policy;
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
    OsIntRestore(intSave);

    return OS_OK;
}

OS_SEC_KERNEL_TEXT U32 OsSemPost(U32 semId)
{
    struct OsSemCb *semCb;
    enum OsIntStatus intSave;
    struct OsTaskCb *pendTsk;

    intSave = OsIntLock();

    semCb = OS_SEM_GET_CB(semId);

    /* 释放资源，val++ */
    if (semCb->val == semCb->semCnt) {
        OS_LOG_WARN("OsSemPost: sem %u is full (val=%u)\n", semId, semCb->val);
        OsIntRestore(intSave);
        return OS_SEM_POST_IS_FULL;
    }
    semCb->val++;

    if (!OsListIsEmpty(&semCb->pendList)) {
        /* 有任务在等，唤醒队首 */
        pendTsk =
            OS_GET_STRUCT_ENTRY(struct OsTaskCb, pendListNode, OsListPopHead(&semCb->pendList));

        /* 如果任务带超时在等，从延时链移除 */
        if (pendTsk->status & OS_TASK_STATUS_IN_DELAY) {
            OsListRemoveNode(&pendTsk->timerListNode);
            pendTsk->status &= ~OS_TASK_STATUS_IN_DELAY;
            OsRefreshNearestTick();
        }

        /* 加回到就绪队列 */
        OsSchedRdyListEnqueTsk(pendTsk);

        /* 可能阻塞的是高优先级的任务，尝试触发调度 */
        OsTaskSchedule();
    }

    OsIntRestore(intSave);

    return OS_OK;
}