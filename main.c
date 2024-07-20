#include "os_def.h"
#include "os_print_external.h"
#include "os_timer_i386.h"
#include "os_hwi_i386.h"
#include "os_debug_external.h"
#include "os_mem_external.h"
#include "os_task_external.h"
#include "os_sched_external.h"

extern void OsBssInit(void);

OS_SEC_KERNEL_TEXT void OsConfigAll(void)
{
    OS_DEBUG_PRINT_STR("OsModuleConfig start\n");
    OsBssInit();
    OsHwiConfig();
    OsMemConfig();
    OsSchedConfig();
    OsTaskConfig();
    OsTimerConfig();
    OS_DEBUG_PRINT_STR("OsModuleConfig end\n");

}

OS_SEC_KERNEL_TEXT S32 main(void)
{
    (void)OsIntLock();
    OsPrintStr("hello kernel\n");
    OsConfigAll();
    
    OsSchedSwitchIdle();

    /* never comes here */
    while (1) {}

    return 0;
}