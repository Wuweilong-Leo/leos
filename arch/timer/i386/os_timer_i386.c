#include "os_timer_i386.h"
#include "os_def.h"
#include "os_hwi_i386.h"
#include "os_print_external.h"
#include "os_debug_external.h"
#include "os_io_i386.h"
#include "os_sched_external.h"

OS_INLINE void OsTimerSetFreq(U8 counterPort, U8 counterNum, U8 rwl,
                           U8 counterMode, U16 counterVal)
{
  OsOutb(PIT_CONTROL_PORT, OS_TIMER_BUILD_FREQ_PORT_MODE(counterNum, rwl, counterMode));
  OsOutb(counterPort, (U8)counterVal);
  OsOutb(counterPort, (U8)(counterVal >> 8));
}

OS_SEC_KERNEL_TEXT void OsTimerIsr(U32 hwiNum, uintptr_t context)
{
    (void)hwiNum;
    (void)context;
    struct OsTaskCb *curTsk = OS_RUNNING_TASK();

    curTsk->ticks--;

    if (curTsk->ticks == 0) {
        /* 删除rdy队列任务 */
        OsSchedDelTskFromRdyList(curTsk);
        /* 降低优先级 */
        OsSchedModifyTskPrio(curTsk);
        /* 加回rdy队列 */
        OsSchedAddTskToRdyListTail(curTsk);
        curTsk->status = OS_TASK_READY;

        /* 更新tick */
        curTsk->ticks = OsTaskGetInitialTick(curTsk->prio);
    }   
}

OS_SEC_KERNEL_TEXT void OsTimerConfig(void)
{
  OS_DEBUG_PRINT_STR("OsTimerConfig start\n");
  OsTimerSetFreq(COUNTER0_PORT, COUNTER0_NO, READ_WRITE_LATCH, COUNTER_MODE, COUNTER0_VALUE);
  (void)OsHwiCreate(0x20, OsTimerIsr);
  OS_DEBUG_PRINT_STR("OsTimerConfig end\n");
}