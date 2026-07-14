#include "os_def.h"
#include "os_hwi.h"
#include "os_sched_external.h"

extern U32 OsConfigInit(void);

OS_SEC_KERNEL_TEXT S32 main(void)
{
    (void)OsIntLock();
    OsConfigInit();

    OsSchedSwitchFirstTsk();

    while (1) {
    }
    return 0;
}