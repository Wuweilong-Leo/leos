#include "os_sched_external.h"
#include "os_task_internal.h"
#include "os_def.h"

OS_SEC_KERNEL_TEXT void OsTskSchedule(void)
{
    if (!OS_RUN_QUE()->needSched){
        return;
    }

    OsTrapTsk(OS_RUNNING_TASK());
}

