#include "os_irq_internal.h"
#include "os_sched_external.h"
#include "os_tick_external.h"
#include "os_hwi.h"
#include "os_debug_external.h"
#include "os_sys.h"

OS_SEC_KERNEL_DATA struct OsIrqForm g_irqForm[OS_IRQ_NUM];

OS_SEC_KERNEL_TEXT void OsIrqDefHandler(U32 irqNum)
{
    (void)irqNum;
}

OS_SEC_KERNEL_TEXT U32 OsIrqCreate(U32 irqNum, OsIrqHandlerFunc isr)
{
    g_irqForm[OsIrqNum2Idx(irqNum)].isr = isr;
    return OS_OK;
}

OS_SEC_KERNEL_TEXT void OsIrqDispatcher(U32 irqNum)
{
    struct OsRunQue *rq = OS_RUN_QUE();
    OsIrqHandlerFunc isr = g_irqForm[OsIrqNum2Idx(irqNum)].isr;

    rq->intCount++;
    rq->uniFlag |= OS_HWI_ACTIVE_MSK;
    isr(irqNum);
    rq->uniFlag &= ~OS_HWI_ACTIVE_MSK;
    rq->intCount--;
}

OS_SEC_KERNEL_TEXT void OsIrqTail(void)
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