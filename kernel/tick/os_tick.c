#include "os_tick_internal.h"
#include "os_sched_external.h"
#include "os_sys.h"
#include "os_hwi_i386.h"
#include "os_task_external.h"

// 系统ticks
OS_SEC_KERNEL_BSS U64 g_uniTicks;
// 未响应tick数
OS_SEC_KERNEL_BSS U32 g_noRespondTicks;

OS_SEC_KERNEL_TEXT bool OsTickTryHandleExpiredTsk(struct OsRunQue *rq)
{
    enum OsIntStatus intSave = OsIntLock();
    struct OsList *dlyListNode;
    struct OsTaskCb *expiredTsk;

    // 有任务到期了
    if ((!OsListIsEmpty(&rq->dlyList)) && (rq->nearestTick >= g_uniTicks)) {
        // 弹出第一个到期任务
        expiredTsk = OS_POP_FIRST_TSK_FROM_DLY_LIST(&rq->dlyList);

        // 加回到就绪队列
        OsEnqueTskToRdyListTail(expiredTsk);

        OsIntRestore(intSave);
        return TRUE;        
    }

    // 第一个任务都没到期，不需要再尝试了
    OsIntRestore(intSave);
    return FALSE;
}

OS_SEC_KERNEL_TEXT void OsRefreshNearestTick(struct OsRunQue *rq)
{
    struct OsTaskCb *firstTsk;
    struct OsList *dlyListNode;
    enum OsIntStatus intSave = OsIntLock();

    if (OsListIsEmpty(&rq->dlyList)) {
        // 没任务在延时了，把nearestTick清除
        rq->nearestTick = 0;
        OsIntRestore(intSave);
        return;
    }

    firstTsk = OS_GET_FIRST_TSK_IN_DLY_LIST(&rq->dlyList);
    rq->nearestTick = firstTsk->expiredTick;

    OsIntRestore(intSave);
    return;  
}

OS_SEC_KERNEL_TEXT void OsTickScanTsks(struct OsRunQue *rq)
{
    bool goOn = FALSE;

    do {
        if (OsTickTryHandleExpiredTsk(rq)) {
            OsRefreshNearestTick(rq);
            goOn = TRUE;
        } 
    } while (goOn);
}

OS_INLINE bool OsTimeSliceOver(struct OsTaskCb *tsk)
{
    return tsk->timeSliceTicks == 0;
}

OS_SEC_KERNEL_TEXT void OsTickHandleTimeSlice(void)
{
    enum OsIntStatus intSave = OsIntLock();
    struct OsTaskCb *curTsk = OS_RUNNING_TASK();

    curTsk->timeSliceTicks--;
    // 时间片耗尽是冷分支
    if (UNLIKELY(OsTimeSliceOver(curTsk))) {
        // 任务先出队
        OsDequeTskFromRdyList(curTsk);
        // 调整任务优先级，时间片轮转
        OsTaskAdjustPrio(curTsk);
        // 加回到就绪队列
        OsEnqueTskToRdyListTail(curTsk);
        curTsk->status = OS_TASK_READY;
        // 重新设置时间片
        OsTaskSetTimeSlice(curTsk, OsTaskCalTimeSlice(curTsk));
    }

    OsIntRestore(intSave);
}

// 中断尾部处理ticks
OS_SEC_KERNEL_TEXT void OsTickDispatcher(void)
{
    struct OsRunQue *rq = OS_RUN_QUE();
    enum OsIntStatus intSave;

    while (UNLIKELY(g_noRespondTicks > 0)) {
        if (OS_TICK_ACTIVE(rq->uniFlag)) {
            // tick已经在处理了，不用再进tick处理
            return;
        }
        OS_UNI_FLAG_SET_MSK(OS_TICK_ACTIVE_MSK);
        /*
         * 这里中断先不开，因为中断会把上下文保存在tcb里，
         * 嵌套以后会把tcb里记录的上个中断的栈指针给冲掉，
         */
        // intSave = OsIntUnlock();
        // 处理时间片
        OsTickHandleTimeSlice();
        // 扫描延时的任务
        OsTickScanTsks(rq);
        // 恢复关中断
        // OsIntRestore(intSave);
        OS_UNI_FLAG_CLR_MSK(OS_TICK_ACTIVE_MSK);
        g_noRespondTicks--;
    }
}

OS_SEC_KERNEL_TEXT void OsTickIsr(void)
{
    // 中断服务程序中要快速处理，其它操作留到中断尾部
    OS_INC_UNI_TICKS();
    OS_INC_NO_RESPOND_TICKS();
}
