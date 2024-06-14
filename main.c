#include "os_def.h"
#include "os_print_external.h"
#include "os_timer_i386.h"
#include "os_hwi_i386.h"
#include "os_debug_external.h"
#include "os_mem_external.h"
OS_SEC_KERNEL_TEXT void OsConfigAll(void)
{
    OS_DEBUG_PRINT_STR("OsModuleConfig start\n");
    OsHwiConfig();
    OsTimerConfig();
    OsMemConfig();
    OS_DEBUG_PRINT_STR("OsModuleConfig end\n");

}

OS_SEC_KERNEL_TEXT S32 main(void)
{
    uintptr_t addr;
    OsPrintStr("hello kernel\n");
    OsConfigAll();
    (void)OsIntLock();

    addr = OsMemKernelAllocPgs(10);
    if (addr == NULL) {
        OS_DEBUG_PRINT_STR("ALLOC FAILED\n");
    }

    OS_DEBUG_PRINT_STR("addr = ");
    OS_DEBUG_PRINT_HEX((U32)addr);
    *(U32*)addr = 1;

    while (1) {}
}