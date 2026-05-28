#include "os_def.h"
#include "os_print_external.h"
#include "os_timer.h"
#include "os_hwi.h"
#include "os_debug_external.h"
#include "os_mem_external.h"
#include "os_task_external.h"
#include "os_sched_external.h"
#include "os_gdt.h"

extern U32 OsConfigInit(void);

OS_SEC_KERNEL_TEXT S32 main(void)
{
    (void)OsIntLock();
    OsPrintStr("hello kernel\n");
    OsConfigInit();
    
    OsSchedSwitchFirstTsk();
    
    /* never comes here */
    while (1) {}

    return 0;
}