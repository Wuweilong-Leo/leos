#include "os_hwi_internal.h"
#include "os_sched_external.h"
#include "os_tick_external.h"
#include "os_hwi.h"
#include "os_debug_external.h"
#include "os_sys.h"

OS_SEC_KERNEL_DATA struct OsHwiForm g_hwiForm[OS_HWI_MAX_NUM];

OS_SEC_KERNEL_TEXT void OsHwiDefHandler(U32 irqNum)
{
    (void)irqNum;
}

OS_SEC_KERNEL_TEXT U32 OsHwiCreate(U32 irqNum, OsHwiHandlerFunc isr)
{
    g_hwiForm[OsHwiNum2Idx(irqNum)].isr = isr;
    return OS_OK;
}

OS_SEC_KERNEL_TEXT void OsHwiDispatcher(U32 irqNum)
{
    struct OsRunQue *rq = OS_RUN_QUE();
    OsHwiHandlerFunc isr = g_hwiForm[OsHwiNum2Idx(irqNum)].isr;

    rq->intCount++;
    rq->uniFlag |= OS_HWI_ACTIVE_MSK;
    isr(irqNum);
    rq->uniFlag &= ~OS_HWI_ACTIVE_MSK;
    rq->intCount--;
}

OS_SEC_KERNEL_TEXT void OsHwiTail(void)
{
    struct OsRunQue *rq = OS_RUN_QUE();
    enum OsIntStatus intSave;

    if (UNLIKELY(g_noRespondTicks > 0)) {
        if (OS_TICK_ACTIVE(rq->uniFlag)) {
            return;
        }
        rq->uniFlag |= OS_TICK_ACTIVE_MSK;
        do {
            intSave = OsIntUnlock();
            OsTickDispatcher();
            OsIntRestore(intSave);
            g_noRespondTicks--;
        } while (g_noRespondTicks > 0);
        rq->uniFlag &= ~OS_TICK_ACTIVE_MSK;
    }

    OsSchedMain();
}