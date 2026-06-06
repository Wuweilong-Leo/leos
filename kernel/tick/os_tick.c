#include "os_tick_internal.h"
#include "os_sched_external.h"
#include "os_sys.h"
#include "os_hwi.h"
#include "os_task_external.h"
#include "os_base_external.h"

// 系统ticks
OS_SEC_KERNEL_BSS U64 g_uniTicks;
// 未响应tick数
OS_SEC_KERNEL_BSS U32 g_noRespondTicks;

OS_SEC_KERNEL_TEXT bool OsTickTryHandleExpiredTsk(void)
{
    enum OsIntStatus intSave = OsIntLock();
    struct OsTaskCb *expiredTsk;

    // 有任务到期了
    if ((!OsListIsEmpty(&g_timerList)) && (g_nearestTick <= g_uniTicks)) {
        // 弹出第一个到期任务
        expiredTsk = OS_GET_STRUCT_ENTRY(struct OsTaskCb, timerListNode, OsListPopHead(&g_timerList));

        expiredTsk->status &= ~OS_TASK_STATUS_IN_DELAY;

        // 如果任务在等信号量，从 pendList 移除并标记超时
        if (expiredTsk->status & OS_TASK_STATUS_PENDING) {
            OsListRemoveNode(&expiredTsk->pendListNode);
            expiredTsk->status |= OS_TASK_STATUS_TIMEOUT;
        }

        // SUSPENDED 任务不加就绪队列
        if (!(expiredTsk->status & OS_TASK_STATUS_SUSPENDED)) {
            OsSchedRdyListEnqueTsk(expiredTsk);
        }

        OsIntRestore(intSave);
        return TRUE;
    }

    // 第一个任务都没到期，不需要再尝试了
    OsIntRestore(intSave);
    return FALSE;
}

OS_SEC_KERNEL_TEXT void OsRefreshNearestTick(void)
{
    struct OsTaskCb *firstTsk;
    enum OsIntStatus intSave = OsIntLock();

    if (OsListIsEmpty(&g_timerList)) {
        OsIntRestore(intSave);
        return;
    }

    // 获取延时链上第一个任务
    firstTsk = OS_GET_STRUCT_ENTRY(struct OsTaskCb, timerListNode, OsListGetFirstNode(&g_timerList));
    g_nearestTick = firstTsk->expiredTick;

    OsIntRestore(intSave);
    return;
}

OS_SEC_KERNEL_TEXT void OsTickScanTsks(void)
{
    while (OsTickTryHandleExpiredTsk()) {
        OsRefreshNearestTick();
    }
}

OS_SEC_KERNEL_TEXT void OsTickHandleTimeSlice(void)
{
    enum OsIntStatus intSave = OsIntLock();
    struct OsTaskCb *curTsk = OS_RUNNING_TASK();

    curTsk->timeSliceTicks--;
    // 时间片耗尽是冷分支
    if (UNLIKELY(curTsk->timeSliceTicks == 0)) {
        // 任务先出队
        OsSchedRdyListDequeTsk(curTsk);
        // 调整任务优先级，时间片轮转
        OsTaskAdjustPrio(curTsk);
        // 加回到就绪队列
        OsSchedRdyListEnqueTsk(curTsk);
        // 重新设置时间片
        OsTaskSetTimeSlice(curTsk, OsTaskCalTimeSlice(curTsk));
    }

    OsIntRestore(intSave);
}

// 中断尾部处理ticks
OS_SEC_KERNEL_TEXT void OsTickDispatcher(void)
{
    // 处理时间片
    OsTickHandleTimeSlice();
    // 扫描延时的任务
    OsTickScanTsks();
}

OS_SEC_KERNEL_TEXT void OsTickIsr(void)
{
    // 中断服务程序中要快速处理，其它操作留到中断尾部
    g_uniTicks++;
    g_noRespondTicks++;
}