#include "os_def.h"
#include "os_print_external.h"
#include "os_timer_i386.h"
#include "os_hwi_i386.h"
#include "os_debug_external.h"
#include "os_mem_external.h"
#include "os_task_external.h"
#include "os_sched_external.h"
#include "os_gdt.h"

extern void OsBssInit(void);

OS_SEC_KERNEL_TEXT void OsConfigAll(void)
{
    OS_DEBUG_PRINT_STR("OsModuleConfig start\n");
    OsBssInit();
    OsBuildUsrGdtEntry();
    OsHwiConfig();
    OsMemConfig();
    OsSchedConfig();
    OsTaskConfig();
    OsTimerConfig();
    OsSemConfig();
    OS_DEBUG_PRINT_STR("OsModuleConfig end\n");

}

OS_SEC_KERNEL_TEXT S32 main(void)
{
    (void)OsIntLock();
    OsPrintStr("hello kernel\n");
    OsConfigAll();
    
    size_t size = 500;
    void *addr = OsMemKernelAlloc(0x500, 0x10);
    kprintf("addr = 0x%x\n, size = 0x%x\n", (U32)addr, size);
    size = 0x2064;
    addr = OsMemKernelAlloc(size, 256);
    kprintf("addr = 0x%x\n, size = 0x%x\n", (U32)addr, size);
    size = 0x3074;
    addr = OsMemKernelAlloc(size, 256);
    kprintf("addr = 0x%x\n, size = 0x%x\n", (U32)addr, size);
    size = 0x512;
    addr = OsMemKernelAlloc(size, 256);
    kprintf("addr = 0x%x\n, size = 0x%x\n", (U32)addr, size);
    size = 0x4000;
    addr = OsMemKernelAlloc(size, 256);
    kprintf("addr = 0x%x\n, size = 0x%x\n", (U32)addr, size);

    size = 0x20;
    addr = OsMemKernelAlloc(size, 256);
    kprintf("addr = 0x%x\n, size = 0x%x\n", (U32)addr, size);
    // OsSchedSwitchIdle();

    /* never comes here */
    while (1) {}

    return 0;
}