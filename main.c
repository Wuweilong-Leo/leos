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
extern U32 OsSysConfigInit(void);

OS_SEC_KERNEL_TEXT void OsConfigAll(void)
{
    OS_DEBUG_PRINT_STR("OsModuleConfig start\n");
    OsBssInit();
    OsSysConfigInit();
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
    
    void *mem = OsMemKernelAlloc(0x100, 4);
    kprintf("mem = 0x%x\n", (U32)mem);
    *(U32 *)mem = 1;
    kprintf("*(U32 *)mem = 0x%x\n", *(U32 *)mem);

    mem = OsMemKernelAlloc(0x100, 4);
    kprintf("mem = 0x%x\n", (U32)mem);
    *(U32 *)mem = 1;
    kprintf("*(U32 *)mem = 0x%x\n", *(U32 *)mem);

    mem = OsMemKernelAlloc(0x100, 0x1000);
    kprintf("mem = 0x%x\n", (U32)mem);
    *(U32 *)mem = 1;
    kprintf("*(U32 *)mem = 0x%x\n", *(U32 *)mem);

    mem = OsMemKernelAlloc(0x200, 256);
    kprintf("mem = 0x%x\n", (U32)mem);
    *(U32 *)mem = 1;
    kprintf("*(U32 *)mem = 0x%x\n", *(U32 *)mem);

    mem = OsMemKernelAlloc(0x200, 16);
    kprintf("mem = 0x%x\n", (U32)mem);
    *(U32 *)mem = 1;
    kprintf("*(U32 *)mem = 0x%x\n", *(U32 *)mem);
    
    /* never comes here */
    while (1) {}

    return 0;
}