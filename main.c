#include "os_def.h"
#include "os_print_external.h"
#include "os_timer_i386.h"
#include "os_hwi_i386.h"
#include "os_debug_external.h"
OS_SEC_KERNEL_TEXT void OsConfigAll(void)
{
    OS_DEBUG_PRINT_STR("OsModuleConfig start\n");
    OsHwiConfig();
    OsTimerConfig();
    OS_DEBUG_PRINT_STR("OsModuleConfig end\n");

}

OS_SEC_KERNEL_TEXT S32 main(void)
{
    OsPrintStr("hello kernel\n");
    OsConfigAll();
    OS_EMBED_ASM("sti");
    while (1) {}
}