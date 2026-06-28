#include "os_msg_internal.h"
#include "os_debug_external.h"
#include "os_task_external.h"
#include "os_hwi.h"
#include "os_sched_external.h"
#include "os_base_external.h"
#include "os_tick_external.h"
#include "os_mem_external.h"
#include "string.h"

/*
 * 任务间点对点消息 IPC
 *
 * 消息内存布局:
 *   [OsList node (8字节)] [payload (size字节)]
 *    ↑ 内核链入目标TCB的msgList    ↑ 返回给用户的指针
 *
 * OsMsgAlloc  — 分配 sizeof(OsList) + size，返回 payload 指针
 * OsMsgSend   — 把消息链入目标 pid 的 msgList，唤醒 PEND_MSG 的目标
 * OsMsgRecv   — 从自己 msgList 取消息，空则阻塞
 * OsMsgFree   — 释放消息（含8字节头）回内核堆
 */

OS_SEC_KERNEL_TEXT U32 OsMsgConfigInit(void)
{
    /* 消息是动态分配的，不需要静态 CB 池。TCB 的 msgList 在 OsTaskConfigInit 中已初始化 */
    return OS_OK;
}

OS_SEC_KERNEL_TEXT void *OsMsgAlloc(size_t size)
{
    struct OsList *header;
    size_t totalSize;

    if (size == 0) {
        return NULL;
    }

    totalSize = sizeof(struct OsList) + size;
    header = (struct OsList *)OsMemKernelAlloc(totalSize, 4);
    if (header == NULL) {
        return NULL;
    }

    OsListInit(header);
    return (void *)(header + 1);
}

OS_SEC_KERNEL_TEXT U32 OsMsgSend(U32 targetPid, void *msgBuf)
{
    struct OsTaskCb *targetTsk;
    struct OsList *header;
    enum OsIntStatus intSave;

    if (targetPid >= OS_TASK_MAX_NUM) {
        return OS_MSG_SEND_PID_INVALID;
    }
    if (msgBuf == NULL) {
        return OS_MSG_SEND_BUF_INVALID;
    }

    header = (struct OsList *)msgBuf - 1;

    intSave = OsIntLock();

    targetTsk = OS_TASK_GET_CB(targetPid);
    if ((targetTsk->status & OS_TASK_STATUS_USED) == 0) {
        OsIntRestore(intSave);
        return OS_MSG_SEND_PID_INVALID;
    }

    /* 消息入目标信箱 */
    OsListAddTail(&targetTsk->msgList, header);

    /* 如果目标在等消息，唤醒它 */
    if (targetTsk->status & OS_TASK_STATUS_PEND_MSG) {
        targetTsk->status &= ~OS_TASK_STATUS_PEND_MSG;

        if (targetTsk->status & OS_TASK_STATUS_IN_DELAY) {
            OsListRemoveNode(&targetTsk->timerListNode);
            targetTsk->status &= ~OS_TASK_STATUS_IN_DELAY;
            OsRefreshNearestTick();
        }

        if (!(targetTsk->status & OS_TASK_STATUS_SUSPENDED)) {
            OsSchedRdyListEnqueTsk(targetTsk);
            OsTaskSchedule();
        }
    }

    OsIntRestore(intSave);
    return OS_OK;
}

OS_SEC_KERNEL_TEXT U32 OsMsgRecv(U32 timeout, void **msgBuf)
{
    struct OsTaskCb *curTsk;
    struct OsList *header;
    enum OsIntStatus intSave;

    if (msgBuf == NULL) {
        return OS_MSG_PARAM_INVALID;
    }

    intSave = OsIntLock();
    curTsk = OS_RUNNING_TASK();

    while (OsListIsEmpty(&curTsk->msgList)) {
        /* 没有消息 */

        if (timeout == OS_MSG_NO_WAIT) {
            OsIntRestore(intSave);
            return OS_MSG_RECV_UNAVAILABLE;
        }

        /* 阻塞自己 */
        OsSchedRdyListDequeTsk(curTsk);
        curTsk->status |= OS_TASK_STATUS_PEND_MSG;

        if (timeout != OS_MSG_WAIT_FOREVER) {
            curTsk->status |= OS_TASK_STATUS_IN_DELAY;
            curTsk->expiredTick = g_uniTicks + timeout;
            OsTaskTimerListInsert(curTsk);
        }

        OsTaskSchedule();

        /* --- 被唤醒 --- */
        if (curTsk->status & OS_TASK_STATUS_TIMEOUT) {
            curTsk->status &= ~OS_TASK_STATUS_TIMEOUT;
            curTsk->status &= ~OS_TASK_STATUS_PEND_MSG;
            OsIntRestore(intSave);
            return OS_MSG_RECV_TIMEOUT;
        }

        /* 正常被 OsMsgSend 唤醒，PEND_MSG 已被 Send 清除 */
    }

    /* msgList 有消息，取出第一个 */
    header = OsListPopHead(&curTsk->msgList);
    *msgBuf = (void *)(header + 1);

    OsIntRestore(intSave);
    return OS_OK;
}

OS_SEC_KERNEL_TEXT U32 OsMsgFree(void *msgBuf)
{
    struct OsList *header;

    if (msgBuf == NULL) {
        return OS_MSG_FREE_BUF_INVALID;
    }

    header = (struct OsList *)msgBuf - 1;
    OsMemKernelFree(header);
    return OS_OK;
}
